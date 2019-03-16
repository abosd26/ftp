#include	"myftp.h"

// use ./myftpServer <port> <filename>
int main( int argc,char **argv ) {
	int socketfd;
	struct stat buf;
	struct sockaddr_in servaddr, clientaddr;
	char device[DEVICELEN];
	int tmpPort;

	srand(time(NULL));
	/* Usage information. */
	if( argc != 3 ) {
		printf( "usage: ./myftpServer <port> <filename>\n" );
		return 0;
	}
	//need superuser authority to start server
	if(getuid() != 0){
		printf("Error: To start server, you must have super user authority!\n");
		exit(1);
	}

	/* Check if file exist. */
	if( lstat( argv[2], &buf ) < 0 ) {
		printf( "unknow file : %s\n", argv[2] );
		return 0;
	}

	/* Open socket. */
	socketfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(socketfd < 0){
		printf("Error opening socket!\n");
		exit(1);
	}
	/* Get NIC device name. */
	if( getDeviceName(socketfd, device) )
		errCTL("getDeviceName");
	printf("network interface = %s\n", device);
	printf("network port = %s\n", argv[1]);
	/* Initial server address. */
	if( initServerAddr(socketfd, atoi(argv[1]), device, &servaddr) ){
		printf("initServerAddr failed!\n");
		exit(1);
	}
	printf("Share file: %s\n", argv[2]);
	//Function: Server can serve multiple clients
	//Hint: Use loop, listenClient(), startMyFtpServer(), and ( fork() or thread )
	while( 1 ) {
		if(listenClient(socketfd, atoi(argv[1]), &tmpPort, argv[2], &clientaddr, device)){
			printf("listenClient error!\n");
			exit(1);
		}
		//fork() return pid, pid = 0 for child process, > 0 for parent process
		if(!fork()) {
			//printf("%d\n", tmpPort);
			if(startMyftpServer(tmpPort, &clientaddr, argv[2])){
				printf("startMyftpServer error!\n");
				printf("wait client!\n");
				exit(1);
			}
			else{
				printf("wait client!\n");
				break;
			}
		}
	}

	return 0;
}
