/*
** SBCP_Server.c   the server side of project 1
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <time.h>
#include "team13_struct.h"

/*
void insertList(struct SBCP_Information **head, struct SBCP_Information **tail,struct SBCP_Information *new_node){
	if (head==NULL){
		//it is a empty list
		head=new_node;
		tail=new_node;
	}
	else{
		//there are some nodes in this list
		tail->following=new_node;
		new_node->front=tail;
		tail=new_node;
	}
}
*/

struct SBCP_Information* findName(int n,struct SBCP_Information* head){
	struct SBCP_Information* temp_ptr=head;
	while(temp_ptr!=NULL){
		if(temp_ptr->fd ==n){
			return temp_ptr;
		}
		temp_ptr=temp_ptr->following;
	}
	return NULL;
}

void listDelete(int n,struct SBCP_Information** headptr_addr,struct SBCP_Information** tailptr_addr){
	struct SBCP_Information *temp_ptr,*temp_front,*temp_tail;
	temp_ptr=findName(n,*headptr_addr);
	if((*headptr_addr)==(*tailptr_addr)){
		*headptr_addr=NULL;
		*tailptr_addr=NULL;
	}
	else{
		if(*headptr_addr==temp_ptr){
			//it is the first nod
			*headptr_addr=(*headptr_addr)->following;
			(*headptr_addr)->front=NULL;
		}
		else{//it is the tail or in the body
			if(*tailptr_addr==temp_ptr){
				*tailptr_addr=(*tailptr_addr)->front;
				(*tailptr_addr)->following=NULL;
			}
			else{//it is in the body
				temp_ptr->front->following=temp_ptr->following;
				temp_ptr->following->front=temp_ptr->front;
			}
		}
	}

	free(temp_ptr);
}

int nameCheck(char* name,struct SBCP_Information*head){
	struct SBCP_Information* temp_ptr=head;
	while(temp_ptr != NULL){
		if (!strcmp(name,temp_ptr->username)){
			return 2;
		}
		temp_ptr=temp_ptr->following;
	}
	return 0;
}
//get sockaddr, no matter IPv4 or IPv6;
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	else
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main (int argc, char *argv[])
{
	fd_set master;   //master file descriptor list
	fd_set read_fds; //temp file descriptor list for select()
	int fdmax;        //maximum file descriptor mumber
	int listener;     //listeng socket descriptor
	int newfd;        //newly accept()ed socket descriptor
	int resultCheck;
	socklen_t addrlen;
	struct SBCP_Information* listHead= NULL;//list to hold clients information
	struct SBCP_Information* listEnd=NULL;
	struct SBCP_Information* listTemp=NULL;
	struct SBCP_Message serverMessage,clientMessage;


	char buf[512];     //for client data;
	char *reasons;
	int nbytes;
	int clientNumber=0;
	int temp;         //for recover the fdmax
	int len_attribute;
	int len_message;

	char remoterIP[INET6_ADDRSTRLEN];

	int yes = 1;       //for setsockopt() SO_REUSEADDR, below
	int i,j, rv;

	struct addrinfo hints, *ai, *p;
	struct sockaddr_storage remoteaddr;

	time_t rawtime;
  	struct tm * timeinfo;

	FD_ZERO(&master);   //clear the master and temp sets
	FD_ZERO(&read_fds); 

	//get us a socket and bind it
	memset (&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	//hints.ai_flags=AI_PASSIVE;
	
	//check the number of arguements
	if (argc < 4){
		fprintf(stderr, "Error, wrong number of arguements\n");
	}

    //setup service address
	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &ai)) !=0){
		fprintf(stderr, "selectserver :　%s\n", gai_strerror(rv));
		exit(1);
	}

	for(p=ai; p!= NULL; p=p->ai_next){
		//create a socket to listen
		listener=socket(p->ai_family, p->ai_socktype,p->ai_protocol);
		if(listener < 0){
			continue;
		}
		//lose the pesky "address already in use" error message
		setsockopt(listener,SOL_SOCKET,SO_REUSEADDR, &yes,sizeof(int));
        
        //bind the socket to the address
		if(bind(listener,p->ai_addr,p->ai_addrlen) < 0){
			close(listener);
			continue;
		}
		break;
	}

	//if we got here, it means we didn' t get bound
	if (p == NULL){
		fprintf(stderr, "\nselectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); //all done with this

	//listen
	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
  	printf("\n%s =======================\n", asctime(timeinfo));
	printf("CHAT ROOM:setup successfully,start to listen\n");
	if (listen(listener,atoi(argv[3])) == -1){
		perror("listen");
		exit(3);
	}
    
    //add the listener to the master set
	FD_SET(listener, &master);

	//keep track the max file descriptor
	fdmax=listener;

	//main loop
	for(;;){
		read_fds=master;
		if(select(fdmax+1,&read_fds,NULL,NULL,NULL)==-1){
			perror("\nselect\n");
			exit(4);
		}

		//run through the existing connections looking for data to read
		for(i=0;i<=fdmax;i++){
			if(FD_ISSET(i,&read_fds)){
			   //we got one connection!
				time ( &rawtime );
  				timeinfo = localtime ( &rawtime );
  				printf ( "\n%s =======================\n", asctime (timeinfo));
				if(i==listener){
					//it is a new connection
					addrlen=sizeof remoteaddr;
					newfd=accept(listener,(struct sockaddr*)&remoteaddr,&addrlen);

					if (newfd==-1){
						perror("\naccept\n");
					}
					else{
						temp=fdmax;
						FD_SET(newfd,&master);
						if(newfd > fdmax){
							fdmax=newfd;
						}

						//read the first message ,expect a JOIN
						if(read(newfd,(struct SBCP_Message*)&clientMessage,sizeof(clientMessage))<0){
							perror("\nfail to read Join\n");
							exit(1);
						}

						printf("\nServer: a JOIN application received from %s\n",clientMessage.attribute[0].content);

						resultCheck=0;
						if(clientNumber >= 10 ){
							resultCheck=1;// 1 for too many clients,2 for same name;
						}
						else{
							resultCheck=nameCheck(clientMessage.attribute[0].content,listHead);
						}						
						if (!resultCheck){
							//new connection built successfully
							printf("\nServer: ACCEPT the application from %s\n",clientMessage.attribute[0].content);

							//update client information
                            listTemp=(struct SBCP_Information*)malloc(sizeof(struct SBCP_Information));
                            listTemp->fd=newfd;
                            strcpy(listTemp->username,clientMessage.attribute[0].content);
                            listTemp->front=NULL;
                            listTemp->following=NULL;

                            if(listHead==NULL){
                            	listHead=listTemp;
                            	listEnd=listTemp;
                            }
                            else{
                            	listEnd->following=listTemp;
                            	listTemp->front=listEnd;
                            	listEnd=listTemp;
                            }
                            clientNumber++;
                            printf("\nthere are %d people in the chatroom\n", clientNumber);
                            printf("\n%s has been updated to the information list\n",listTemp->username);

							//send ACK to him
							serverMessage.head.vrsn=3;
							serverMessage.head.type=7;
							serverMessage.attribute[0].type=3;
							memset(serverMessage.attribute[0].content,'\0',sizeof(serverMessage.attribute[0].content));
							memset(buf,'\0',sizeof(buf));
							buf[0]=(char)(((int)'0')+clientNumber);
							strcpy(serverMessage.attribute[0].content,buf);
							serverMessage.attribute[0].length=strlen(buf);

							serverMessage.attribute[1].type=2;
							memset(serverMessage.attribute[1].content,'\0',sizeof(serverMessage.attribute[1].content));
							memset(buf,'\0',sizeof(buf));
							for(listTemp=listHead;listTemp != NULL;listTemp=listTemp->following){
								strcat(buf,listTemp->username);
								strcat(buf,",");
							}
							strcpy(serverMessage.attribute[1].content,buf);
							serverMessage.attribute[1].length=strlen(buf);
							if((write(newfd,(void*)&serverMessage,sizeof(serverMessage)))==-1){
											perror("send");
							}
							printf("\nServer: an ACK has been sent to %s\n",clientMessage.attribute[0].content);		
							

							//form a online message
							serverMessage.head.vrsn=3;
							serverMessage.head.type=8;
							serverMessage.attribute[0].type=2;
							listTemp=findName(newfd,listHead);
							if(listTemp==NULL){
								printf("can't find the node\n");
								exit(5);
							}	
							memset(serverMessage.attribute[0].content,'\0',sizeof(serverMessage.attribute[0].content));
							strcpy(serverMessage.attribute[0].content,listTemp->username);
							serverMessage.attribute[0].length=strlen(serverMessage.attribute[0].content);
							//send online to everyone else
							for(j=0; j<=fdmax;j++){
								if (FD_ISSET(j,&master)){
									if(j != listener && j != newfd){
										//j is not the listener,nor itself
										if((write(j,(void*)&serverMessage,sizeof(serverMessage)))== -1){
											perror("send");
										}
									}
								}
							}
							printf("\nServer: an ONLINE of \"%s\" has been sent to other guys",listTemp->username);
						}//end of ACK
						else{
							//send NAK
							printf("\nServer: REJECT the application from %s\n",clientMessage.attribute[0].content);
							serverMessage.head.vrsn=3;
							serverMessage.head.type=2;
							serverMessage.attribute[0].type=1;
							memset(buf,'\0',sizeof(buf));
							if(resultCheck==1){
								//NAK FOR too many people
								strcpy(buf,"there are too many people!");
							}
							else{
								//NAK for same name
								strcpy(buf,"there is a same name!");
							}

							printf("\nREJ reason: %s\n",buf);
							serverMessage.attribute[0].length=strlen(buf);
							memset(serverMessage.attribute[0].content,'\0',sizeof(serverMessage.attribute[0].content));
							strcpy(serverMessage.attribute[0].content,buf);
							if((write(newfd,(void*)&serverMessage,sizeof(serverMessage)))==-1){
											perror("send");
							}
							//reject the join
							printf("\nServer: a NAK has been send to %s\n", clientMessage.attribute[0].content);
							fdmax=temp;
							FD_CLR(newfd,&master);
							printf("\nServer: the line between server and %s has been tired\n",clientMessage.attribute[0].content);
						}//end of NAK
					}//end of successful accept a new connection
				}// if(i==listener)
				else{
					//it is an old connections
					if ((nbytes=read(i,(struct SBCP_Message*)&clientMessage,sizeof(clientMessage)))<=0){
						//we got a error, or the connection closed 
						if (nbytes==0){
							//connection closed
							//FD_CLR(newfd,&master);
							//form a offline message


							serverMessage.head.vrsn=3;
							serverMessage.head.type=6;
							serverMessage.attribute[0].type=2;
							listTemp=findName(i,listHead);
							
							if(listTemp==NULL){
								printf("can't find the node\n");
								exit(5);
							}
							printf("\nServer: %s has been left\n",listTemp->username);
							memset(serverMessage.attribute[0].content,'\0',sizeof(serverMessage.attribute[0].content));
							strcpy(serverMessage.attribute[0].content,listTemp->username);
							serverMessage.attribute[0].length=strlen(serverMessage.attribute[0].content);
							//send offline to all others
							for(j=0;j<=fdmax;j++){
								if(FD_ISSET(j,&master)){
									//except the listener and itself
									if (j != listener && j !=i){
										if((write(j,(void*)&serverMessage,sizeof(serverMessage)))==-1){
											perror("send");
										}
									}
								}
							}
							printf("\nServer: an OFFLINE of \"%s\" has been sent to other guys\n",listTemp->username);

						}
						else{
							//it is a error message
							perror("recv");
						}
						//recycle the resource
						close(i);
						
						FD_CLR(i,&master);
						if(clientNumber==1){
							printf("\nServer: The chatroom is empty now. Goodbye!\n");
							close(listener);
							return 0;
						}
						listDelete(i,&listHead,&listEnd);
						clientNumber--;
						
					}//end of error message
					else{
						//it received a message
						if(clientMessage.head.type==9){
							//it is a idle message
							serverMessage.head.vrsn=3;
							serverMessage.head.type=9;
							serverMessage.attribute[0].type=2;
							listTemp=findName(i,listHead);
							if(listTemp==NULL){
								printf("can't find the node\n");
								exit(5);
							}
							printf("\nServer: Received a IDLE from %s\n",listTemp->username);
							memset(serverMessage.attribute[0].content,'\0',sizeof(serverMessage.attribute[0].content));
							strcpy(serverMessage.attribute[0].content,listTemp->username);
							serverMessage.attribute[0].length=strlen(serverMessage.attribute[0].content);
							//send idle to all others
							for(j=0;j<=fdmax;j++){
								if(FD_ISSET(j,&master)){
									//except the listener and itself
									if (j != listener && j !=i){
										if((write(j,(void*)&serverMessage,sizeof(serverMessage)))==-1){
											perror("send");
										}
									}
								}
						    }
						    printf("\nServer: IDLE from %s has been sent to other guys\n",listTemp->username);
						}//end of idle message
						else{
							//it is a real message,fwd
							serverMessage.head.vrsn=3;
							serverMessage.head.type=3;
							serverMessage.attribute[0].type=2;
							serverMessage.attribute[1]=clientMessage.attribute[0];
							listTemp=findName(i,listHead);
							if(listTemp==NULL){
								printf("can't find the node\n");
								exit(5);
							}
							printf("\nServer: Receive a TEXT from %s\n",listTemp->username);
							memset(serverMessage.attribute[0].content,'\0',sizeof(serverMessage.attribute[0].content));
							strcpy(serverMessage.attribute[0].content,listTemp->username);
							serverMessage.attribute[0].length=strlen(serverMessage.attribute[0].content);
							//forward to all others
							for(j=0;j<=fdmax;j++){
								if(FD_ISSET(j,&master)){
									//except the listener and itself
									if (j != listener && j !=i){
										if((write(j,(void*)&serverMessage,sizeof(serverMessage)))==-1){
											perror("send");
										}
									}
								}
							}
							printf("\nServer: TEXT from %s has been sent to other guys\n",listTemp->username);	
						}//end of real message
					}//end of sucessful message
				}//it is the end of old connection
			}//else i is not in the readfd;if(FD_ISSET(i,&read_fds))
		}//for(i=0;i<=fdmax;i++){
	}//end of the for(;;)
	close(listener);
	return 0;
}






























	