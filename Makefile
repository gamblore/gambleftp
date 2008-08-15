CC=gcc
NOTPOSIXCFLAGS=-Wall -g
CFLAGS=-Wall -ansi -posix -g
LFLAGS=-Wall 
OBJS=server.o ftp.o parser.o ftpcmd.o filelist.o clientcmd.o
all: server


parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c
ftp.o: ftp.c ftp.h
	$(CC) $(CFLAGS) -c ftp.c
ftpcmd.o: ftpcmd.c
	$(CC) $(CFLAGS) -c ftpcmd.c
clientcmd.o: clientcmd.c
	$(CC) $(CFLAGS) -c clientcmd.c
filelist.o: filelist.c
	$(CC) $(CFLAGS) -c filelist.c
server.o: server.c
	$(CC) $(NOTPOSIXCFLAGS) -c server.c

server: $(OBJS)
	$(CC) $(LFLAGS) -o server $(OBJS)

psp:
	$(shell make -f Makefile.psp)
nds:
	$(shell make -f Makefile.nds)

pspclean:
	$(shell make -f Makefile.psp clean)
ndsclean:
	$(shell make -f Makefile.nds clean)
clean:
	-rm -f server *.o
