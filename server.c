#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define RCVBUFSIZE 1280
#define MAXCOMMLEN 10
#define MAXFNAMELEN 256
const char defaultPage[] = "index.html";

typedef enum {
    html,jpg,jpeg,png,other
} FileType ;

void error(const char* msg) {
    perror(msg);
    exit(1);
}

void getCommand (char* commLine, char* comm, char* fname) {
    char temp;
    int ind = 0;
    
    temp = commLine[ind];
    while (ind < MAXCOMMLEN && temp != ' ') {
        comm[ind] = temp;
        temp = commLine[++ind];
    }
    if (ind == MAXCOMMLEN)
        printf("command length exceed\n");
    comm[ind] = '\0';
    
    int fInd = 0;
    fname[0]='.';
    fname[1]='/';
    while (commLine[ind]==' ') {
        ++ind;
    }
    if (commLine[ind]=='/') {
        ++ind;
    }
    temp = commLine[ind];
    while (temp != ' ') {
        fname[fInd++] = temp;
        temp = commLine[++ind];
    }
    fname[fInd] = '\0';
}


//Check the file type though its file name,
//Append default page name to fname if needed.
//
FileType checkFileType(char *fname) {
    
    char *c = fname;
    char *tail;
    
    while(*c != '\0')
        ++c;
    
    tail = c;
    
    do {
        --c;
        if (*c == '/'){
            //no extension in file name
            //regard it as a path
            //TODO : ask TA or professor
            if (c+1==tail)
                strcpy(tail,defaultPage);
            else{
                *tail='/';
                strcpy(tail+1,defaultPage);
            }
            return html;
        }
    } while (*c != '.');
    
    ++c;
    if (strcmp(c,"jpg")==0 ||
        strcmp(c,"JPG")==0){
        return jpg;
    }else if (strcmp(c,"jpeg")==0 ||
              strcmp(c,"JPEG")==0){
        return jpeg;
    }else if (strcmp(c,"png")==0 ||
              strcmp(c,"PNG")==0){
        return png;

    }else
        return other;
}

int sendInitLine(int csock, int code){
    const char str200[]="200 OK\r\n";
    const char str404[]="404 Not Found\r\n";
    char s[256]="HTTP/1.0 ";
    switch (code) {
            
        case 200:
            strcat(s,str200);
            break;
        
        case 404:
            strcat(s,str404);
            break;
        
        default:
            printf("Error: Unimplemented init line");
            exit(-1);
            break;
    }
    int l=strlen(s);
    if ( write(csock,s,l) != l)
        error("Error when sending");
    return 0;
}

int sendHeader(int csock, FileType type, int fileSize){
    char s[256]="Content-Type: ";
    switch (type) {
        
        case html:
            strcat(s,"text/html\r\n");
            break;
        
        case jpg:
            strcat(s,"image/jpg\r\n");
            break;
        
        case png:
            strcat(s,"image/png\r\n");
            break;
        
        default:
            printf("Error: Unimplemented file type");
            exit(-1);
            break;
    }
    sprintf(s,"%sContent-Length: %d\r\n\r\n",s,fileSize);
    int l=strlen(s);
    if ( write(csock,s,l) != l)
        error("Error when sending");
    return 0;
}

int sendFile(int csock,char fname[]){
    FILE* fd;
    int fsize;
    FileType type;

//    if (fname[0] == '\0') {       // open default page
//        fd = fopen(defaultPage, "r");
//        type = html;
//    } else {
//        if (isHTML(fname)){
//            type = html;
//            fd = fopen(fname, "r" );
//        }else{
//            type = png;//debug  TODO
//            fd = fopen(fname, "rb");
//        }
//    }
    
    //this will append the default page to fname if needed
    type=checkFileType(fname);
    
    printf("[debug] file name: %s\n",fname);
    
    switch (type) {
        case html:
            fd = fopen(fname, "r" );
            break;
            
        case jpg:
        case png:
            fd = fopen(fname, "rb");
            break;
            
        default:
            break;
    }
    
    if (fd<0) error("File open error");
    
    fseek(fd, 0, SEEK_END);  // set the position of fd in file end(SEEK_END)
    fsize = ftell(fd);       // return the fd current offset to beginning
    rewind(fd);
    
    sendHeader(csock,type,fsize);
    
    char *content = (char*) malloc(fsize);
    if (( fread(content, 1, fsize, fd)) != fsize)
        error("Read file error");
    if (( write(csock, content, fsize)) != fsize)
        error("Send error");
    free(content);
    fclose(fd);
    return 0;
}

int main(int argc, char* argv[]) {
    int sock, csock, portno, clilen, n;
    char rcvBuff[RCVBUFSIZE];
    char comm[MAXCOMMLEN];
    char fname[MAXFNAMELEN];
    
    int rcvMsgSize;
    struct sockaddr_in serv_addr, cli_addr;
    
    if (argc < 3) {
        error("ERROR: Not enough argument. Expecting port number and root path");
    }
    chdir(argv[2]);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("Sockfd is not available");

    bzero((char* ) &serv_addr, sizeof(serv_addr));
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

    if (bind(sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
        error("Bind error");
    
    if (listen(sock, 128) < 0)
        error("Listen error");

    socklen_t cliaddr_len;
    cliaddr_len = sizeof(cli_addr);
    while (1) {
        if((csock = accept(sock, (struct sockaddr*) &cli_addr, &cliaddr_len)) < 0) 
            error("Accepct error");
        if (fork() == 0) {
            if((rcvMsgSize = recv(csock, rcvBuff, RCVBUFSIZE, 0)) < 0)
                error("Receive error");
            rcvBuff[rcvMsgSize]='\0';
            printf("[Received]====================\n%s\n", rcvBuff);

            getCommand(rcvBuff, comm, fname);

            sendInitLine(csock,200);
            sendFile(csock, fname);
            
            bzero(rcvBuff, RCVBUFSIZE);
        }        
        close(csock);
    }
    return 1;
}
