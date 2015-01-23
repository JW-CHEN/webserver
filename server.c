#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "common.h"
#include "permission.h"
#include "stringProcessing.h"

//TODO:
//400 error: HTTP/1.1 without host header. Or no colon, no value, etc.

int running = 1;

struct RespArg {
    int csock;
    struct sockaddr_in cli_addr;
};

void* userIOSentry(void* sock) {
    printf("Waiting for request. To exit server, type in 'q + <Enter>'.\n");

    char key;
    do {
        key = getchar();
    } while (key != 'q' && key != 'Q');
    running = 0;
    close(*((int*)sock));
    return NULL;
}

int sendInitLine(int csock, int code) {
    char s[256] = "HTTP/1.1 ";

    const char str200[] = "200 OK\r\n";
    const char str404[] = "404 Not Found\r\n"
        "Content-Type: text/plain\r\n\r\nError 404 (Not Found).\r\n";
    const char str403[] = "403 Permission Denied\r\n"
        "Content-Type: text/plain\r\n\r\nError 403 (Permission Denied).\r\n";

    switch (code) {

    case 200:
        strcat(s, str200);
        break;

    case 404:
        strcat(s, str404);
        break;
    
    case 403:
        strcat(s, str403);
        break;

    default:
        error("Error: Unimplemented response code\n");
        break;
    }
    int l = strlen(s);
    if (write(csock, s, l) != l)
        error("Error when sending");
    return 0;
}

static inline void sendEmptyLine(int csock) {
    if (write(csock, "\r\n", 2) != 2)
        error("Error when sending");
}

int sendHeader(int csock, FileType type, int fileSize) {
    char s[256] = "Content-Type: ";
    switch (type) {

    case html:
        strcat(s, "text/html\r\n");
        break;

    case jpg:
    case jpeg:
        strcat(s, "image/jpeg\r\n");
        //Seems like image/jpg is not a standard type
        //https://en.wikipedia.org/wiki/Internet_media_type#Type_image
        break;

    case png:
        strcat(s, "image/png\r\n");
        break;

    case ico:
        strcat(s, "image/x-icon\r\n");
        break;

    default:
        printf("Warning: Unimplemented file type\n");
        s[0] = '\0';
        break;
    }
    sprintf(s, "%sContent-Length: %d\r\n\r\n", s, fileSize);
    //empty line is included

    int l = strlen(s);
    if (write(csock, s, l) != l)
        error("Error when sending");
    return 0;
}

int sendFile(int csock, char fname[]) {
    FILE* fd;
    int fsize;
    FileType type;

    //this will append the default page to fname if needed
    type = getFileType(fname);

    //printf("[debug] file name: %s[end of debug]",fname);

    switch (type) {
    case html:
        fd = fopen(fname, "r");
        break;

    default:
        fd = fopen(fname, "rb");
        break;
    }
    //printf("debug, fd = %lld\n", (long long int)fd);
    if (fd == NULL) return -1;//send 404 later

    sendInitLine(csock, 200);

    fseek(fd, 0, SEEK_END);  // set the position of fd in file end(SEEK_END)
    fsize = ftell(fd);       // return the fd current offset to beginning
    rewind(fd);

    sendHeader(csock, type, fsize);

    char *content = (char*)malloc(fsize);
    if (( fread(content, 1, fsize, fd) ) != fsize)
        error("Read file error");
    if (( write(csock, content, fsize) ) != fsize)
        error("Send error");
    free(content);
    fclose(fd);
    return 0;
}



void* response(void* args) {
    char rcvBuff[RCVBUFSIZE];
    char comm[MAXCOMMLEN];
    char fname[MAXFNAMELEN];
    HttpVersion version;
    int rcvMsgSize;
    int csock = (( struct RespArg* )args)->csock;
    /*
    unsigned int ip = args_t->cli_addr.sin_addr.s_addr;
    char ipClient[30];
    sprintf(ipClient, "%d.%d.%d.%d", ((ip >> 0) & 0xFF), ((ip >> 8) & 0xFF), ((ip >> 16) & 0xFF), ((ip >> 24) & 0xFF));
    printf("Client IP %s\n", ipClient);*/
    if (checkAuth(args_t->cli_addr,".htaccess") == 0) {
        sendInitLine(csock, 403);
        sendEmptyLine(csock);
        printf("debug, sending 403");
        close(csock);
        free(args_t);
        return NULL;
    }
     
    // Guarantees that thread resources are deallocated upon return
    pthread_detach(pthread_self());
    //Q: do we need to wait for the thread to finish, before the server exits by presing 'q' key ?

    /*
    use select() to try persistent connection
    do not close the socket until timeout of select
    */
    fd_set rdfds;
    //printf("I just want to know sizeof(rdfds):%d\n", sizeof(rdfds));
    struct timeval tv;
    int ret = 1;
    while (ret != 0) {
        FD_ZERO(&rdfds);
        FD_SET(csock, &rdfds);
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        ret = select(csock + 1, &rdfds, NULL, NULL, &tv);
        //printf("select return value %d\n", ret);
        if (ret < 0)
            error("Select() error");
        else if (ret>0) {
            if (( rcvMsgSize = recv(csock, rcvBuff, RCVBUFSIZE, 0) ) < 0)
                error("Receive error");
            //TODO: recv might receive part of the packet, as in the book
            //accomplished by checking /r/n/r/n
            //and the head of a request might be in the last packet!
            printf("client socket: %d\n", csock);
            rcvBuff[rcvMsgSize] = '\0';
            getCommand(rcvBuff, comm, fname, version);
            printf("[Received]---------------\n%s %s (...)\n", args_t->comm, args_t->fname);
            if (strcmp("GET", comm) == 0) {
                if (sendFile(csock, fname) == -1) {
                    sendInitLine(csock, 404);
                    sendEmptyLine(csock);
                    //the status line is terminated by an empty line.
                    //see http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
                    printf("debug, sending 404\n");
                    ret = 0;
                }
            }
        }
        if (ret == 0) {
            //printf("closed socket %d\n", csock);
            close(csock);
        }
    }
    free(( struct RespArg* )args);
    return NULL;
}

int main(int argc, char* argv[]) {
    int sock, csock, portno;

    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 3) {
        error("ERROR: Not enough argument. Expecting port number and root path");
    }
    chdir(argv[2]);

    if (( sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0)
        error("Sockfd is not available");

    bzero((char*)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    /*
    struct sockaddr_in have four fields
    sin_family: address family (AF_UNIX, AF_INET)
    sin_port: port number in network order
    in_addr: a struct contains s_addr the IP addr of sockaddr_in
    */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;  // any packet send to sin_port will be accept, regardless of the dest ip addr
    serv_addr.sin_port = htons(portno);

    if (bind(sock, ( struct sockaddr* ) &serv_addr, sizeof(serv_addr)) < 0)
        error("Bind error");

    //set maximum # of waiting connections to 5
    if (listen(sock, 5) < 0)
        error("Listen error");

    pthread_t thread;
    pthread_create(&thread, NULL, userIOSentry, (void*)&sock);

    socklen_t cliaddr_len = sizeof(cli_addr);
    while (running) {
        if (( csock = accept(sock, ( struct sockaddr* ) &cli_addr, &cliaddr_len) ) < 0){
            if (running == 0){
                printf("Server exits normally.\n");
                break;
            }else{
                error("Accepct error");
            }
        }
        printf("master thread call one time **********\n");
        struct RespArg *args;
        args = malloc(sizeof(struct RespArg));
        args->csock = csock;
        args->cli_addr = cli_addr;
        pthread_create(&thread, NULL, response, (void *)args);
    }
    return 0;
}
