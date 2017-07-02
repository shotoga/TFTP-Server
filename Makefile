tftpServer: tftpServer.c
	gcc -o tftpServer tftpServer.c
clean:
	rm -f *~ *.o proxy 