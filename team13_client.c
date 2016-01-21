/*
** client
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "team13_struct.h"
#include <time.h>

void sendJoin(int sockfd,struct SBCP_Message* clientMessagePtr,char *usernamePtr){
	//form a client messaage
	clientMessagePtr->head.vrsn=3;
	clientMessagePtr->head.type=2;
	clientMessagePtr->attribute[0].type=2;
	memset(clientMessagePtr->attribute[0].content,'\0',sizeof(clientMessagePtr->attribute[0].content));

	strcpy(clientMessagePtr->attribute[0].content,usernamePtr);
}

void processMessage(int sockfd,struct SBCP_Message *serverMessagePtr){
	time_t rawtime;
  	struct tm * timeinfo;
  	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
  	printf("\n%s====================\n%s ", asctime(timeinfo),serverMessagePtr->attribute[0].content);

	switch(serverMessagePtr->head.type){
	case 6://offline
	    //puts(serverMessagePtr->attribute[0].content);
	    printf("is OFFLINE. \n\n");
		break;
	case 8://online
		//puts(serverMessagePtr->attribute[0].content);
	    printf(" is ONLINE. \n\n");
		break;
	case 9://idle
		//puts(serverMessagePtr->attribute[0].content);
	    printf(" is IDLE. \n\n");
		break;
	case 3://fwd
		//puts(serverMessagePtr->attribute[0].content);
		printf(" SAY:\n");
		puts(serverMessagePtr->attribute[1].content);
		//putchar('\n');
		break;
	default:
		printf("said something illegal.\n\n");
		break;
	}
}

int decision(struct SBCP_Message *serverMessagePtr){
	//if ACK printf(all names);return 
	int i,result;
	time_t rawtime;
  	struct tm * timeinfo;
  	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
  	printf("\n%s====================\nreceived a ", asctime(timeinfo));
	if(serverMessagePtr->head.type==7){//ACK
		printf("ACK from server.\n");
		printf("\n%c people now in chatroom:\n    ",serverMessagePtr->attribute[0].content[0]);
		for(i=0;i<serverMessagePtr->attribute[1].length;i++){
			if(serverMessagePtr->attribute[1].content[i]!=','){
			putchar(serverMessagePtr->attribute[1].content[i]);
			}
			else
				printf("\n    ");
		}
		result= 0;
	}
	else{
		//if NAK printf(reason);return 1
		if(serverMessagePtr->head.type==2){//NAK
			printf("NAK from server.\n\n");
			puts(serverMessagePtr->attribute[0].content);
			result= 1;
		}
	}
	return result;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
	if (sa->sa_family == AF_INET) {
    	return &(((struct sockaddr_in*)sa)->sin_addr);
    }
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct timeval timeMinus(struct timeval time1, struct timeval time2){
	//this program execute minus
	struct timeval result;
	int timeTotal;
	timeTotal=1000000*(time1.tv_sec-time2.tv_sec)+time1.tv_usec-time2.tv_usec;
	if(timeTotal<=0){
		result.tv_sec=0;
		result.tv_usec=0;
	}
	else{
		result.tv_sec=timeTotal/1000000;
		result.tv_usec=timeTotal-1000000*result.tv_sec;
	}
	return result;
}

int main(int argc, char *argv[]){
	int sockfd, nbytes,i,fd_return;
	char buf[512];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];  //used to print server address
	char username[17];
	int dec;  //use to decide  ack or nak;
	//int fdmax;  //use to record the max file descriptor
	double time_rest,time_use;

	fd_set master;
	fd_set readfds;

	struct timeval tv,start_time,end_time,interval_time;

	struct SBCP_Message serverMessage,clientMessage;
	time_t rawtime;
  	struct tm * timeinfo;
  	
	//check the number of arguements
	if (argc != 4) {
		fprintf(stderr,"\nServer: wrong arguements number\n");
		exit(1);
	}

	//get the user name
	memset(username,'\0',sizeof username);
	if(strlen(argv[1])>=16){
		fprintf(stderr, "\nServer:the username is too long\n");
		exit(2);
	}
	strcpy(username,argv[1]);
	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
  	printf("\n%s==================\n", asctime(timeinfo));

	printf("username is %s\n",username);

	//set the service information
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "\nclient: failed to connect\n");
		return 2;
	}


	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
  	printf("\n%s====================\n", asctime(timeinfo));

	printf("client: connecting to %s\n", s);
	freeaddrinfo(servinfo); // all done with this structure

	//send a join
	sendJoin(sockfd, &clientMessage,username);
	printf("a JOIN from %s has been sent\n",clientMessage.attribute[0].content);
	if((write(sockfd,(void*)&clientMessage,sizeof(clientMessage)))==-1){
		perror("sendJion");
	}
	else{
		printf("send join successfully\n");
	}

	//read a message, ACK or NAK;
	if((nbytes=read(sockfd,(struct SBCP_Message*)&serverMessage,sizeof(serverMessage)))<0){
		perror("fail to read ACK or NAK");
		exit(1);
	}

	//printf("get a ACK or NAK\n");

	dec=decision(&serverMessage);
	//printf("the decision is %d\n",dec);
	if(dec){
		close(sockfd);
		return 0;
	}


	
	//if it will be here, it means joint the chat successfully
	printf("\n==========You can chat now!==========\n");

	FD_ZERO(&readfds);
	FD_ZERO(&master);

	//set time interval
	tv.tv_sec=10;
	tv.tv_usec=0;


	FD_SET(sockfd,&master);
	FD_SET(STDIN_FILENO,&master);
	

	for(;;){
		readfds=master;

		gettimeofday(&start_time,0);
		if((fd_return=select(sockfd+1,&readfds,NULL,NULL,&tv))==-1){
			perror("select");
			exit(4);
		}

		gettimeofday(&end_time,0);
		interval_time=timeMinus(end_time,start_time);

		if(FD_ISSET(STDIN_FILENO,&readfds)){
			//something typed in;
			nbytes=read(STDIN_FILENO,buf,sizeof(buf));
			printf("%d characters have been typed in\n",nbytes);
			if(nbytes>0){
				buf[nbytes]='\0';
			}
			clientMessage.head.vrsn=3;
			clientMessage.head.type=4;
			clientMessage.attribute[0].type=4;
			memset(clientMessage.attribute[0].content,'\0',sizeof(clientMessage.attribute[0].content));	
			strcpy(clientMessage.attribute[0].content,buf);
			clientMessage.attribute[0].length=strlen(buf);
			if((write(sockfd,(void*)&clientMessage,sizeof(clientMessage)))==-1){
				perror("send");
			}
			time ( &rawtime );
  			timeinfo = localtime ( &rawtime );
			printf("=============A TEXT with %lu characters has been sent at %s",strlen(buf),asctime(timeinfo));

			//set time to 10 seconds
			tv.tv_sec=10;
			tv.tv_usec=0;
		}

		if(FD_ISSET(sockfd,&readfds)){
			//we got a new message
			if(read(sockfd,(struct SBCP_Message*)&serverMessage,sizeof(serverMessage))<0){
				perror("fail to process\n");
				exit(1);
			}
			processMessage(sockfd,&serverMessage);
			tv=timeMinus(tv,interval_time);
			//printf("type time left %lu s %d us\n",tv.tv_sec,tv.tv_usec);
		}

		if(fd_return==0){
			//time out
			time ( &rawtime );
  			timeinfo = localtime ( &rawtime );
  			printf("\n%s==========\nTIME OUT: An IDLE will be sent automatically\n", asctime(timeinfo));

			//printf("time out!\n");
			clientMessage.head.vrsn=3;
			clientMessage.head.type=9;
			if((write(sockfd,(void*)&clientMessage,sizeof(clientMessage)))==-1){
				perror("idle");
			}

			//set time to 10 seconds
			tv.tv_sec=10;
			tv.tv_usec=0;

		}
	}
}






















