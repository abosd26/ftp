all : myftpServer myftpClient

myftpServer : myftpServer.c
	gcc -o myftpServer myftpServer.c myftp.h myftp.c
myftpClient : myftpClient.c
	gcc -o myftpClient myftpClient.c myftp.h myftp.c
