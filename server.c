#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>
#include<arpa/inet.h>

#include<pthread.h>
#include<errno.h>
#include<signal.h>

#define MAX_CLIENTS 5000
#define SA struct sockaddr
static _Atomic unsigned int cli_count = 0;
static long long int uid = 10;
int adminactive = 0;
int admin_private = 0;
long int array_size = 1048576;		// 1048576 bits == 1 MB
//client structure
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32],rno[32];
	int admin;
	int uni;
	int private_uid;
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER,
								adminclient_mutex = PTHREAD_MUTEX_INITIALIZER,
								fileSending_mutex = PTHREAD_MUTEX_INITIALIZER ;

void str_overwrite_stdout()
{
	printf("\r%s", "> ");
	fflush(stdout);
}

void str_trim_lf(char* arr, int length)
{
	for(int i=0; i<length ; i++)
	{
		if(arr[i] == '\n')
		{
			arr[i] = '\0';
			break;
		}
	}
}

void print_ip_addr( struct sockaddr_in addr )
{
	printf("\n %d.%d.%d.%d" ,
	addr.sin_addr.s_addr & 0xff,
	(addr.sin_addr.s_addr & 0xff00) >> 8,
	(addr.sin_addr.s_addr & 0xff0000)>>16,
	(addr.sin_addr.s_addr & 0xff000000)>>24);
}

void queue_add(client_t *cl)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS ; i++)
	{
		if(!clients[i])
		{
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0 ; i<MAX_CLIENTS ; ++i)
	{
		if(clients[i])
		{
			if(clients[i] -> uid == uid)
			{
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *s, int uid)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(clients[i])
		{
			if(clients[i] -> uid !=uid)
			{
				if(write(clients[i] -> sockfd , s , strlen(s)) < 0)
				{
					printf("\n\ERROR : write to descriptor failed...");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

int checkStudent(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(clients[i])
		{
			if(clients[i] -> uid !=uid)
			{
				if(strcmp(clients[i] -> rno,s)==0 || strcmp(clients[i] -> name,s)==0){
					printf("[+] present\n");
					return clients[i] -> uid;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);

	printf("[+] absent");
	return 0;
}

void sendFileToNonAdmin(char *fileName, int uid, void *arg)
{
	client_t *cli = (client_t *)arg;
	char buffer[5000];
	pthread_mutex_lock(&fileSending_mutex);
	{
			
			send(clients[uid] -> sockfd,"$[+]$incoming$file$from$admi$",strlen("$[+]$incoming$file$from$admi$"),0);
			send(clients[uid] -> sockfd , fileName , strlen(fileName) , 0);
			
			char filearray[array_size];
			
			recv(cli -> sockfd , filearray , strlen(filearray) ,0 );
			
			printf("\n\nCode:\n");
			
			printf("%s\n",filearray);
			
			send(clients[uid] -> sockfd,filearray,strlen(filearray),0);
		
	}
	pthread_mutex_unlock(&fileSending_mutex);
}


void *handle_client(void *arg)
{
	char buff_out[5000],buff_out2[5000],buff_out3[5000],buff_name[50];
	char name[32],rno[32];
	int leave_flag = 0;
	cli_count ++;

	client_t *cli = (client_t *)arg;

	//name
	if(recv(cli->sockfd , name , 32 , 0) <= 0 || strlen(name) < 1 || strlen(name) >= 32 - 1)
	{
	printf("\nEnter the name correctly...\n");
	leave_flag = 1;
	}
	if(recv(cli->sockfd , rno , 32, 0) <= 0 || strlen(rno) < 1 || strlen(rno) >= 32 - 1)
	{
	printf("\nEnter the roll number correctly...%s\n",rno);
	leave_flag = 1;
	}
	else
	{
		cli -> admin = 0;
		cli -> private_uid = 0;
		strcpy(cli -> name, name);
		strcpy(cli -> rno, rno);
		sprintf(buff_out, "\n%s_%s has joined\n" , cli -> name,cli->rno);
		printf("%s" , buff_out);
		send_message(buff_out , cli -> uid);
	}
	bzero(buff_out , 5000);

	while(1)
	{
		if(leave_flag)
		{
			break;
		}
		int receive = recv(cli -> sockfd , buff_out , 5000 , 0);
		if(receive > 0 )
		{
			if(strlen(buff_out) > 0)
			{
			  printf("\n~~~ %d ~~~\n%s\n",cli->uid,buff_out);
				str_trim_lf(buff_out,strlen(buff_out));
//			  printf(".\n%s\n.",buff_out);
				if(strcmp(buff_out,"set-admin")==0){
					if(adminactive==0){
					  sprintf(buff_out,"[+] Enter Password to become admin: ");
					  write(cli -> sockfd , buff_out , strlen(buff_out));
					  recv(cli->sockfd, buff_out,5000 , 0);
					  sscanf(buff_out,"%s : %s\n",buff_out2,buff_out);
					  
					  if(strcmp(buff_out,"admin@123")==0)
					  {
						  pthread_mutex_lock(&adminclient_mutex);
						  
						  sprintf(buff_out,"[+] You are now admin");
						  write(cli -> sockfd , buff_out , strlen(buff_out));
						  sprintf(buff_out,"[+] %s has been set as admin",cli->name);
						  send_message(buff_out,cli->uid);
						  printf("%s\n",buff_out);
						  adminactive=1;
						  cli->admin=1;
						  
						  pthread_mutex_unlock(&adminclient_mutex);
						}else{
						  sprintf(buff_out,".\n[+] Wrong password entered...\n.");
						  write(cli -> sockfd , buff_out , strlen(buff_out));
						}
					}else if(adminactive==1){
						if(!cli->admin){
							sprintf(buff_out,"[+] There is already a Admin present inside this server\nNo more admins can be created.");
						write(cli -> sockfd , buff_out , strlen(buff_out));
						printf("[+] %s requested to become an admin\n",cli->name);
						}else{
							sprintf(buff_out,"[+] You are already a admin...");
							write(cli -> sockfd , buff_out , strlen(buff_out));
						}
					}
				}else if(strcmp(buff_out,"unset-admin")==0){
					if(adminactive==1){
						if(cli->admin){
							pthread_mutex_lock(&adminclient_mutex);
							
							sprintf(buff_out,"[+] You are not admin anymore");
							write(cli -> sockfd , buff_out , strlen(buff_out));
							sprintf(buff_out,"[+] %s has been removed as admin",cli->name);
							send_message(buff_out,cli->uid);
							printf("%s\n",buff_out);
							adminactive=0;
							cli->admin=0;
							
							pthread_mutex_unlock(&adminclient_mutex);
						}
						else{
							sprintf(buff_out,"[+] You are not admin...");
							write(cli -> sockfd , buff_out , strlen(buff_out));
						}
					}else if(adminactive==0){
						sprintf(buff_out,"[+] You are already not a admin");
						write(cli -> sockfd , buff_out , strlen(buff_out));
					}
				}else{
				  bzero(buff_name,sizeof(buff_name));
				  bzero(buff_out2,5000);
				  sscanf(buff_out,"%s : %s\n",buff_name,buff_out2);
				  if(strcmp(buff_out2,"send")==0){
				  if(!cli->admin){
				  	printf("[+] send command received...from %s_%s\n",cli->name,cli->rno);
				  	bzero(buff_out2,sizeof(buff_out2));
				  	sprintf(buff_out2,"[+] Enter file name: ");
				  	write(cli -> sockfd , buff_out2 , strlen(buff_out2));
					  recv(cli->sockfd, buff_out , 5000 , 0);
					  sscanf(buff_out,"%s : %s\n",buff_name,buff_out2);
					  printf("[+] file name -> %s\n",buff_out2);
					  
					  // receiving program file from non-admin user to admin(i.e.to server)...
					  
					  {
					  	char location[5000];
					  	
					  	system("mkdir ServerFiles");
					  	
					  	sprintf(location,"ServerFiles/%s",buff_out2);
					  	
					  	FILE *fp;
							
							char filearray[array_size];
							
							printf("[+] new program file created...(at location %s)",location);
							
					  	
				  		recv(cli->sockfd,filearray,sizeof(filearray),0);
				  		printf("[+] code:\n%s\n",filearray);
					  	fp = fopen(location,"w");
				  		fprintf(fp,filearray);
				  		fclose(fp);
				  		
					  	
					  	}
					  	
					 }
					 
					 if(cli->admin){
					 	printf("[+] send command received...from %s_%s\n",cli->name,cli->rno);
				  	bzero(buff_out2,sizeof(buff_out2));
				  	sprintf(buff_out2,"[+] Enter Student name or roll number: ");
				  	write(cli -> sockfd , buff_out2 , strlen(buff_out2));
				  	bzero(buff_out2,sizeof(buff_out2));
				  	
				  	int isStudent = checkStudent(buff_out2,cli->uid);
				  	
				  	if(isStudent != 0){
				  	
							sprintf(buff_out2,"[+] Enter file name: ");
							write(cli -> sockfd , buff_out2 , strlen(buff_out2));
							recv(cli->sockfd, buff_out , 5000 , 0);
							sscanf(buff_out,"%s : %s\n",buff_name,buff_out2);
							printf("[+] file name -> %s\n",buff_out2);
							
							// sending file to non-admin user ...
							sendFileToNonAdmin(buff_out2,isStudent,cli);
//							###########################################
//							###########################################3
//							###########################################
//##################################
//###########################
//#########################
//###########################
					  	}
					 }
				  }else{
					  send_message(buff_out	,cli->uid);
				  }
				}
			}
//			  bzero(buff_out,sizeof(buff_out));
//			  bzero(buff_out2,sizeof(buff_out2));
//			  bzero(buff_out3,sizeof(buff_out3));
//			  bzero(buff_name,sizeof(buff_name));
		}
		else if(receive == 0 || strcmp(buff_out ,"exit" ) == 0 )
		{
			if(adminactive==1 && cli -> admin == 1){
			  if(admin_private && cli->private_uid){ cli->private_uid = 0;admin_private = 0;}
				adminactive=0;
				sprintf(buff_out,"The Admin (%s_%s) has left",cli->name,cli->rno);
				send_message(buff_out,cli->uid);
				printf("%s\n",buff_out);
				leave_flag = 1;
				break;
			}
			else{
			  sprintf(buff_out,"\n%s_%s has left\n", cli->name,cli->rno);
			  printf("%s",buff_out);
			  send_message(buff_out , cli->uid);
			  leave_flag = 1;
			  break;
			}
		}
		else
		{
			printf("\nERROR : -1\n");
			leave_flag = 1;
		}
		bzero(buff_out , 5000);
	}
	close(cli->sockfd);
	queue_remove(cli->uid);
	free(cli);
	cli_count --;
	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv)
{
	if(argc == 3 || argc == 2 || argc == 1){;}
	else
	{
		printf("\nInvalid number of arguments passed...\n(hint : syntax is like '%s <port_no> <ip_address>')\n",argv[0]);
		exit(0);
	}

	int sockfd,newsockfd;
	socklen_t n;
	struct sockaddr_in servaddr , clientaddr ;
	int option = 1;

	pthread_t tid;

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0)
	{
		printf("\nERROR: socket()\n");
		exit(0);
	}
//	else
//		printf("\nsocket successfully created..\n");
	
	//socket settings
	servaddr.sin_family = AF_INET;
	if(argc==2 || argc==3)
  	servaddr.sin_port = htons(atoi(argv[1]));
  else
    servaddr.sin_port = htons(8080);
	if(argc == 3)
		servaddr.sin_addr.s_addr = inet_addr(argv[2]);
	else
//		printf("\nip not provided , so using local ip address...\n");
		servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");


	//signals
	signal(SIGPIPE , SIG_IGN);

	if(setsockopt(sockfd , SOL_SOCKET , (SO_REUSEPORT | SO_REUSEADDR) , (char *) &option , sizeof(option) ) < 0)
	{
		printf("\nError : setsockopt\n");
		return EXIT_FAILURE;
	}

	//bind
		if (bind(sockfd,(SA*)&servaddr,sizeof(servaddr))<0)
	{
		if (argc == 3)
		{
//			printf("\nFailed to bind socket with given ip(i.e. %s)\nIt seems that the given ip is not assosiated with current device...\nSo trying to bind the socekt with local ip(127.0.0.1)\n",argv[1]);
			servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
			if (bind(sockfd,(SA*)&servaddr,sizeof(servaddr))<0)
			{
				printf("\nFailed to bind the socket with local ip too...(The given port might be already in use... try with different port...)\n");
				exit(0);
			}
//			else
//				printf("\nSuccessfully Binded...\n");
		}
		else
		{
			printf("\nFailed to bind the socekt...\n");
			exit(0);
		}
	}

	//listen
	if (listen(sockfd,5)<0)
	{
		printf("\nERROR: listen\n");
		exit(0);
	}
  
  system("cls || clear");
	printf("\n~~~ Welcome to TSI ~~~ss\n");

	while(1)
	{
		n = sizeof(clientaddr);
		newsockfd = accept(sockfd , (SA*) &clientaddr , &n);

		//check for max clients
		if((cli_count + 1) == MAX_CLIENTS)
		{
			printf("\nMaximum Clients connected ... Connection rejected ...\n");
			print_ip_addr(clientaddr);
			printf(":%d\n", clientaddr.sin_port);
			close(newsockfd);
			continue;
		}

		//client settings
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli -> uni = 0;
		cli -> address = clientaddr;
		cli -> sockfd = newsockfd;
		cli -> uid = uid++;

		//Add client to queue
		queue_add(cli);
		pthread_create(&tid , NULL , &handle_client , (void*)cli);

		//reduce CPU usage
		sleep(1);

	}
	return EXIT_SUCCESS;
}
