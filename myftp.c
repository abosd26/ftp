#include	"myftp.h"

int getDeviceName( int socketfd, char *device ) {
	//Function: To get the device name
	//Hint:     Use ioctl() with SIOCGIFCONF as an arguement to get the interface list
	struct ifconf ifconf;
	struct ifreq ifr[10];
	ifconf.ifc_len = sizeof(ifr);
	ifconf.ifc_req = ifr;
	if(!ioctl(socketfd, SIOCGIFCONF, (char *)&ifconf)){
		//ifr[0].ifr_name is lo...
		//printf("we have %d interfaces\n", ifconf.ifc_len / (int)sizeof(struct ifreq));
		strcpy(device, ifr[ifconf.ifc_len / (int)sizeof(struct ifreq) - 1].ifr_name);
	}
	return 0;
}

int initServerAddr( int socketfd, int port, const char *device, struct sockaddr_in *addr ) {
	//Function: Bind device with socketfd
	//          Set sever address(struct sockaddr_in), and bind with socketfd
	//Hint:     Use setsockopt to bind the device
	//          Use bind to bind the server address(struct sockaddr_in)
	//clear the buffer to 0s
	memset((char *)addr, 0, sizeof(*addr));
	//code for address family, constant
	addr->sin_family = AF_INET;
	//host ip which server will receive, INADDR_ANY will set it as 0.0.0.0(means every hosts can be receive)
	addr->sin_addr.s_addr = INADDR_ANY;
	//printf("my address : %s\n", inet_ntoa(addr->sin_addr));
	//port #, converted from host byte order to network byte order
	addr->sin_port = htons(port);
	//set the socket to be bind with NIC name
	if(setsockopt(socketfd, SOL_SOCKET, SO_BINDTODEVICE, device, sizeof(device))){
		printf("setsockopt error!\n");
		return -1;
	}
	//bind system call, bind the socket to the address
	if(bind(socketfd, (struct sockaddr *)addr, sizeof(*addr)) < 0){
		printf("Error on binding!");
		return -1;
	}
	printf("Myftp Server Start!!\n");
	//printf("Share file: %s\n", filename);
	return 0;
}
int listenClient(int socketfd, int port, int *tempPort, char *filename, struct sockaddr_in *clientaddr, const char* device){
	struct startServerInfo buffer;
	int clientaddr_size = sizeof(struct sockaddr_in);
	printf("wait client!\n");
	while(1){
		int recvSize = recvfrom(socketfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)clientaddr, &clientaddr_size);
		if(recvSize < 0){
			printf("recvfrom error!\n");
			return -1;
		}
		if(!strcmp(buffer.filename, filename)){
			printf("\t[Request]\n");
			printf("\t\tfile : %s\n", buffer.filename);
			printf("\t\tfrom : %s\n", inet_ntoa(clientaddr->sin_addr));
			printf("\t[Reply]\n");
			printf("\t\tfile : %s\n", filename);
			*tempPort = 44301 + (rand() % 999 + 1);
			printf("\t\tconnectPort : %d\n", *tempPort);
			memset(&buffer, 0, sizeof(buffer));
			//get server ip
			struct ifreq req;
			memset(&req, 0, sizeof(req));
			strcpy(req.ifr_name, device);
			if(ioctl(socketfd, SIOCGIFADDR, &req) == -1){
				printf("Error in getting ip\n");
				return -1;
			}
			char* myip = inet_ntoa(((struct sockaddr_in*)&req.ifr_addr)->sin_addr);
			strcpy(buffer.servAddr, myip);
			buffer.connectPort = *tempPort;
			strcpy(buffer.filename, filename);
			int n = sendto(socketfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)clientaddr, sizeof(struct sockaddr_in));
			if(n < 0){
				int errsv = errno;
				printf("sendto error! %d\n", errsv);
				return -1;
			}
			break;
		}
	}
	return 0;
}
int initClientAddr( int socketfd, int port, char *sendClient, struct sockaddr_in *addr ) {
	//Function: Set socketfd with broadcast option and the broadcast address(struct sockaddr_in)
	//Hint:     Use setsockopt to set broadcast option
	int broadcast = 1;
	//set socketfd with broadcast option
	if(setsockopt(socketfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast))){
		printf("setsockopt error!\n");
		return -1;
	}
	//set timeout mechanism for socket
	struct timeval timeout = {TIMELIMIT, 0};
	if(setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval))){
		printf("setsockopt SO_RCVTIMEO error!\n");
		return -1;
	}
	//clear the buffer to 0s
	memset((char *)addr, 0, sizeof(*addr));
	//code for address family, constant
	addr->sin_family = AF_INET;
	//destination ip, use INADDR_BROADCAST for sending a broadcast packet
	addr->sin_addr.s_addr = INADDR_BROADCAST;
	//server port #, converted from host byte order to network byte order
	addr->sin_port = htons(port);
	return 0;
}

int findServerAddr( int socketfd, char *filename, const struct sockaddr_in *broadaddr, struct sockaddr_in *servaddr ) {
	//Function: Send broadcast message to find server
	//          Set timeout to wait for server replay
	//Hint:     Use struct startServerInfo as boradcast message
	//          Use setsockopt to set timeout
	struct startServerInfo packet;
	int servaddr_size;

	strcpy(packet.filename, filename);
	int n = sendto(socketfd, &packet, sizeof(packet), 0, (struct sockaddr *)broadaddr, sizeof(struct sockaddr_in));
	if(n < 0){
		int errsv = errno;
		printf("sendto error! %d\n", errsv);
		return -1;
	}
	memset(&packet, 0, sizeof(packet));

	int recvSize = recvfrom(socketfd, &packet, sizeof(packet), 0, (struct sockaddr *)servaddr, (socklen_t *)&servaddr_size);
	if(recvSize < 0){
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			printf("No server answer!!\n");
			return 1;
		}
		else{
			printf("recvfrom error!\n");
		}
		return -1;
	}
	//server port #, converted from host byte order to network byte order
	servaddr->sin_port = htons(packet.connectPort);
	printf("[Receive Reply]\n");
	printf("\t\tGet MyftpServer servAddr : %s\n", packet.servAddr);
	printf("\t\tMyftp connectPort : %d\n", packet.connectPort);
	return 0;
}

int startMyftpServer( int tempPort, struct sockaddr_in *clientaddr, const char *filename ) {
	/* Open socket. */
	int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(socketfd < 0){
		printf("Error opening socket!\n");
		exit(1);
	}
	struct sockaddr_in addr;
	//clear the buffer to 0s
	memset((char *)&addr, 0, sizeof(addr));
	//code for address family, constant
	addr.sin_family = AF_INET;
	//server ip, INADDR_ANY will get it automatically
	addr.sin_addr.s_addr = INADDR_ANY;
	//server port #, converted from host byte order to network byte order
	addr.sin_port = htons(tempPort);
	//bind system call, bind the socket to the address
	if(bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		printf("Error on binding!");
		return -1;
	}
	//set timeout mechanism for socket
	struct timeval timeout = {TIMELIMIT, 0};
	if(setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval))){
		printf("setsockopt SO_RCVTIMEO error!\n");
		return -1;
	}
	//receive FRQ from client
	int frqSize = sizeof(struct myFtphdr) + FNAMELEN - 1;
	int dataSize = sizeof(struct myFtphdr) + MFMAXDATA - 1;
	int msgSize = sizeof(struct myFtphdr);
	struct myFtphdr *frq = (struct myFtphdr *)malloc(frqSize);
	int clientaddr_size;

	do{
		int recvSize = recvfrom(socketfd, frq, frqSize, 0, (struct sockaddr *)clientaddr, &clientaddr_size);
		if(recvSize < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				printf("wait client time out\n");
			}
			else{
				printf("recvfrom error!\n");
				return -1;
			}
		}
	}while(errno == EAGAIN || errno == EWOULDBLOCK || ntohs(frq->mf_opcode) != FRQ);
	free(frq);
	printf("\t[file transmission - start]\n");
	printf("\t\tsend file : <%s> to %s\n", filename, inet_ntoa(clientaddr->sin_addr));
	//start file transmission : send data block to client, keep sending if ACK for last block of data is received from client
	FILE *fptr = fopen(filename, "rb");
	//blockNum for sending packet(can only in 0~65535), realNum for real count block number(can > 65535)
	int blockNum = 1, byteCount, byteSum = 0, realNum = 1, isEnd = 0, retransmit = 0;
	char *block = (char *)malloc(MFMAXDATA);
	struct myFtphdr *data = (struct myFtphdr *)malloc(dataSize);
	struct myFtphdr *msg = (struct myFtphdr *)malloc(msgSize);
	/*//print total file size
	fseek(fptr, 0, SEEK_END);
	printf("total file size : %ld bytes\n", ftell(fptr));
	rewind(fptr);*/
	while(!feof(fptr) || isEnd == 0){
		if(retransmit == 0){
			memset(data, 0, dataSize);
			memset(block, 0, MFMAXDATA);
			byteCount = fread(block, 1, MFMAXDATA, fptr);
			memcpy(data->mf_data, block, MFMAXDATA);
			//if block number reach the upper limit (it can save only 0~65535 for unsigned short) than reset to 1
			if(blockNum % 65536 == 0){
				blockNum = 1;
			}
			data->mf_block = blockNum;
			data->mf_opcode = htons(DATA);
			data->mf_cksum = in_cksum((unsigned short *)data, dataSize);
			//if the last digit of block number is 9, it has the probability of 10% to have the wrong checksum
			if(realNum % 10 == 9){
				int randNum = rand() % 10;
				if(randNum == 9){
					int wrongCS;
					do{
						wrongCS = rand() % 65536;
					}while(wrongCS == data->mf_cksum);
					data->mf_cksum = wrongCS;
				}
			}
			//printf("send to : %s, port : %d\n", inet_ntoa(clientaddr->sin_addr), ntohs(clientaddr->sin_port));
			int n = sendto(socketfd, data, dataSize, 0, (struct sockaddr *)clientaddr, sizeof(struct sockaddr_in));
			if(n < 0){
				printf("sendto error\n");
				return -1;
			}
		}
		//receive ACK from client, retransmit last block of data if receive ERROR
		int recvSize = recvfrom(socketfd, msg, msgSize, 0, (struct sockaddr *)clientaddr, &clientaddr_size);
		if(recvSize < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				printf("time out waiting ACK, send data again\n");
				usleep(50);
				data->mf_cksum = 0;
				data->mf_cksum = in_cksum((unsigned short *)data, dataSize);
				int n = sendto(socketfd, data, dataSize, 0, (struct sockaddr *)clientaddr, sizeof(struct sockaddr_in));
				if(n < 0){
					printf("sendto error\n");
					return -1;
				}
				retransmit = 1;
				//fseek(fptr, -byteCount, SEEK_CUR);
				continue;
			}
			else{
				printf("recvfrom error!\n");
				return -1;
			}
		}
		if(retransmit == 1){
			retransmit = 0;
		}
		if(ntohs(msg->mf_opcode) == ACK){
			if(msg->mf_block == 0){
				isEnd = 1;
			}
			else if(msg->mf_block == blockNum){
				blockNum++;
				realNum++;
				byteSum += byteCount;
				usleep(50);
				//sleep(1);
			}
		}
		else if(ntohs(msg->mf_opcode) == ERROR && msg->mf_block == blockNum){
			usleep(50);
			data->mf_cksum = 0;
			data->mf_cksum = in_cksum((unsigned short *)data, dataSize);
			int n = sendto(socketfd, data, dataSize, 0, (struct sockaddr *)clientaddr, sizeof(struct sockaddr_in));
			if(n < 0){
				printf("sendto error\n");
				return -1;
			}
			retransmit = 1;
			//fseek(fptr, -byteCount, SEEK_CUR);
		}
	}
	//data transmission is finish
	free(data);
	free(msg);
	free(block);
	fclose(fptr);
	printf("\t[file transmission - finish]\n");
	printf("\t\t%d bytes sent\n", byteSum);
	return 0;
}

int startMyftpClient( int socketfd, struct sockaddr_in *servaddr, const char *filename ) {
	int frqSize = sizeof(struct myFtphdr) + FNAMELEN - 1;
	int dataSize = sizeof(struct myFtphdr) + MFMAXDATA - 1;
	int msgSize = sizeof(struct myFtphdr);
	struct myFtphdr *frq = (struct myFtphdr *)malloc(frqSize);
	int servaddr_size;

	memset((char *)frq, 0, frqSize);
	frq->mf_opcode = htons(FRQ);
	memcpy(frq->mf_filename, filename, FNAMELEN);
	//printf("%s\n", frq->mf_filename);
	frq->mf_cksum = in_cksum((unsigned short *)frq, frqSize);
	//printf("checksum : %hu\n", frq->mf_cksum);
	int n = sendto(socketfd, frq, frqSize, 0, (struct sockaddr *)servaddr, sizeof(struct sockaddr_in));
	if(n < 0){
		printf("sendto error\n");
		return -1;
	}
	free(frq);
	//start data reception
	printf("[file transmission - start]\n");
	printf("\t\tdownload to file : client_%s\n", filename);
	printf("\t\tget file : <%s> from %s\n", filename, inet_ntoa(servaddr->sin_addr));
	char *fname = (char *)malloc(sizeof(char) * FNAMELEN);
	strcpy(fname, "client_");
	FILE *fptr = fopen(strcat(fname, filename), "wb");
	int blockNum = 1, byteCount, byteSum = 0, realNum = 1, isFirst = 1;
	char *block = (char *)malloc(sizeof(char) * 512);
	struct myFtphdr *data = (struct myFtphdr *)malloc(dataSize);
	struct myFtphdr *msg = (struct myFtphdr *)malloc(msgSize);
	/*//print total file size
	fseek(fptr, 0, SEEK_END);
	printf("total file size : %ld bytes\n", ftell(fptr));
	rewind(fptr);*/
	while(1){
		//receive data from server
		int recvSize = recvfrom(socketfd, data, dataSize, 0, (struct sockaddr *)servaddr, &servaddr_size);
		if(recvSize < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				printf("time out waiting data\n");
				//if client didn't receive first block of data, then resend FRQ	to server for file requestion
				if(isFirst){
					struct myFtphdr *frq = (struct myFtphdr *)malloc(frqSize);
					memset((char *)frq, 0, frqSize);
					frq->mf_opcode = htons(FRQ);
					memcpy(frq->mf_filename, filename, FNAMELEN);
					frq->mf_cksum = in_cksum((unsigned short *)frq, frqSize);
					int n = sendto(socketfd, frq, frqSize, 0, (struct sockaddr *)servaddr, sizeof(struct sockaddr_in));
					free(frq);
					if(n < 0){
						printf("sendto error\n");
						return -1;
					}
					printf("resend FRQ\n");
				}
				continue;
			}
			else{
				printf("recvfrom error!errno : %d\n", errno);
				return -1;
			}
		}
		if(isFirst){
			isFirst = 0;
		}
		//if checksum of the data block is correct, then reply ACK, otherwise reply ERROR
		memset(msg, 0, msgSize);
		if(blockNum % 65536 == 0){
			blockNum = 1;
		}
		msg->mf_block = blockNum;
		msg->mf_cksum = in_cksum((unsigned short *)msg, msgSize);
		if(in_cksum((unsigned short *)data, dataSize) == 0){
			msg->mf_opcode = htons(ACK);
			blockNum++;
			realNum++;
			memset(block, 0, MFMAXDATA);
			memcpy(block, data->mf_data, MFMAXDATA);
			//if the length of data sent from server is 0, means client has finish downloading
			if(strlen(block) == 0){
				msg->mf_block = 0;
			}
			else{
				byteCount = fwrite(block, 1, strlen(block), fptr);
				byteSum += byteCount;
			}
		}
		else{
			printf("received data checksum error, the block is %d\n", realNum);
			msg->mf_opcode = htons(ERROR);
		}
		int n = sendto(socketfd, msg, msgSize, 0, (struct sockaddr *)servaddr, sizeof(struct sockaddr_in));
		if(n < 0){
			printf("sendto error\n");
			return -1;
		}
		//finish download
		if(msg->mf_block == 0){
			break;
		}
	}
	//data transmission is finish
	free(fname);
	free(data);
	free(msg);
	free(block);
	fclose(fptr);
	printf("[file transmission - finish]\n");
	printf("\t\t%d bytes received\n", byteSum);
	return 0;
}

unsigned short in_cksum( unsigned short *addr, int len ) {
	int nleft = len;
	int sum = 0;
	unsigned short *w = addr;
	unsigned short answer = 0;

	while( nleft > 1 ) {
		sum += *w++;
		nleft -= 2;
	}

	if( nleft == 1 ) {
		*(unsigned char *) (&answer) = *(unsigned char *) w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	answer = ~sum;
	return (answer);
}
