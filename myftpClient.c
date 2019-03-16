#include	"myftp.h"

// use ./myftpClient <port> <filename>
int main( int argc, char **argv ) {
	int socketfd;
	struct sockaddr_in servaddr,broadaddr;
	int result;
	//
	char device[DEVICELEN];

	/* Usage information.	*/
	if( argc != 3 ) {
		printf( "usage: ./myftpClient <port> <filename>\n" );
		return 0;
	}

	/* Open socket.	*/
	socketfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(socketfd < 0){
		printf("Error opening socket!\n");
		exit(1);
	}
	//get device name
	if(getDeviceName(socketfd, device)){
		printf("getDeviceName error!\n");
		exit(1);
	}
	printf("network interface : %s\n", device);
	/* Initial client address information. */
	if( initClientAddr(socketfd, atoi(argv[1]), "", &broadaddr) ){
		printf("initClientAddr error!n");
		exit(1);
	}
	/* Find server address.	*/
	/* & set time out.		*/
	do{
		result = findServerAddr(socketfd, argv[2], &broadaddr, &servaddr);
	}while(result == 1);
	if(result){
		printf("findServerAddr error!\n");
		exit(1);
	}

	/* Start ftp client */
	if(startMyftpClient(socketfd, &servaddr, argv[2])){
		printf("startMyfptClient error!\n");
		exit(1);
	}
	return 0;
}
