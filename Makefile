#A makefile for CSE 124 project 1
CC= gcc -pthread

# include debugging symbols in object files,
# and enable all warnings
CFLAGS= -g -Wall
CXXFLAGS= -g -Wall

#include debugging symbols in executable
LDFLAGS= -g

httpd: server.o
	@echo Making httpd ...
	@$(CC) -o httpd server.o

server.o: server.c 
	@$(CC) -c -o server.o server.c

clean:
	$(RM) httpd *.o *~
