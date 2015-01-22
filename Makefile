#A makefile for CSE 124 project 1
CC= gcc -std=gnu99

# include debugging symbols in object files, and enable all warnings
WarningOptions = -Wall -Wimplicit
CFLAGS = -g -pthread $(WarningOptions)

#include debugging symbols in executable
LDFLAGS= -g -pthread


###########################

PROGRAM = httpd
OFILES = server.o permission.o

##########################

$(PROGRAM): $(OFILES)
	@echo $@ #ð�����
	@$(CC) $(LDFLAGS) $(OFILES) -o $(PROGRAM)

%.o:%.c
	@ echo $< # dependencies list ��ÿһ��
	@ $(CC) $(CFLAGS) -c $<

##########################


clean:
	$(RM) httpd *.o *~
