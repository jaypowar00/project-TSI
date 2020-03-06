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

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

static _Atomic unsigned int cli_count = 0;
static int uid = 10;
int adminactive = 0;

//client structure
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
	int admin;
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER, adminclient_mutex = PTHREAD_MUTEX_INITIALIZER;

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
	printf("\n(%s)\n",arr);
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
void self_send_message(char *s, int uid)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(clients[i])
		{
			if(clients[i] -> uid ==uid)
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
void *handle_client(void *arg)
{
	char buff_out[BUFFER_SZ];
	char name[32];
	int leave_flag = 0;
	cli_count ++;

	client_t *cli = (client_t *)arg;

	//name
	if(recv(cli->sockfd , name , 32 , 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1)
	{
	printf("\nEnter the name correctly...\n");
	leave_flag = 1;
	}
	else
	{
		cli -> admin = 0;
		strcpy(cli -> name, name);
		sprintf(buff_out, "\n%s has joined\n" , cli -> name);
		printf("%s" , buff_out);
		send_message(buff_out , cli -> uid);
	}
	bzero(buff_out , BUFFER_SZ);

	while(1)
	{
		if(leave_flag)
		{
			break;
		}
		int receive = recv(cli -> sockfd , buff_out , BUFFER_SZ , 0);
		if(receive > 0 )
		{
			if(strlen(buff_out) > 0)
			{
				printf("admin status : %d\n",adminactive);
				str_trim_lf(buff_out,strlen(buff_out));
				if(strcmp(buff_out,"set-admin")==0){
					if(adminactive==0){
						pthread_mutex_lock(&adminclient_mutex);
						
						sprintf(buff_out,"[+] You are now admin");
						self_send_message(buff_out,cli->uid);
						sprintf(buff_out,"[+] %s has been set as admin",cli->name);
						send_message(buff_out,cli->uid);
						printf("%s\n",buff_out);
						adminactive=1;
						cli->admin=1;
						
						pthread_mutex_unlock(&adminclient_mutex);
					}else if(adminactive==1){
						if(!cli->admin){
							sprintf(buff_out,"[+] There is already a Admin present inside this server\nNo more admins can be created.");
						self_send_message(buff_out,cli->uid);
						printf("[+] %s requested to become an admin\n",cli->name);
						}else{
							sprintf(buff_out,"[+] You are already a admin...");
							self_send_message(buff_out,cli->uid);
						}
					}
				}else if(strcmp(buff_out,"unset-admin")==0){
					if(adminactive==1){
						if(cli->admin){
							pthread_mutex_lock(&adminclient_mutex);
							
							sprintf(buff_out,"[+] You are not admin anymore");
							self_send_message(buff_out,cli->uid);
							sprintf(buff_out,"[+] %s has been removed as admin",cli->name);
							send_message(buff_out,cli->uid);
							printf("%s\n",buff_out);
							adminactive=0;
							cli->admin=0;
							
							pthread_mutex_unlock(&adminclient_mutex);
						}
						else{
							sprintf(buff_out,"[+] You are not admin...");
							self_send_message(buff_out,cli->uid);
						}
					}else if(adminactive==0){
						sprintf(buff_out,"[+] You are already not a admin");
						self_send_message(buff_out,cli->uid);
					}
				}else{
					send_message(buff_out , cli -> uid);
					str_trim_lf(buff_out, strlen(buff_out));
					printf("%s\n",buff_out);
				}
			}
		}
		else if(receive == 0 || strcmp(buff_out ,"exit" ) == 0 )
		{
			if(adminactive==1){
				adminactive=0;
				sprintf(buff_out,"The Admin (%s) has left",cli->name);
				send_message(buff_out,cli->uid);
				printf("%s\n",buff_out);
			}
			sprintf(buff_out,"\n%s has left\n", cli->name);
			printf("%s",buff_out);
			send_message(buff_out , cli->uid);
			leave_flag = 1;
		}
		else
		{
			printf("\nERROR : -1\n");
			leave_flag = 1;
		}
		bzero(buff_out , BUFFER_SZ);
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
	if(argc!=2)
	{
		printf("\nInvalid Arguments passed ...\nUsage : %s <port>\n",argv[0]);
		return EXIT_FAILURE;
	}
	char *ip ="127.0.0.1";
	int port = atoi(argv[1]);

	int option = 1;
	int sockfd = 0, connfd = 0;
	struct sockaddr_in servaddr , clientaddr ;

	pthread_t tid;

	//socket settings
	sockfd = socket(AF_INET , SOCK_STREAM, 0);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(ip);
	servaddr.sin_port = htons(port);

	//signals
	signal(SIGPIPE , SIG_IGN);

	if(setsockopt(sockfd , SOL_SOCKET , (SO_REUSEPORT | SO_REUSEADDR) , (char *) &option , sizeof(option) ) < 0)
	{
		printf("\nError : setsockopt\n");
		return EXIT_FAILURE;
	}

	//bind
	if(bind(sockfd, (struct sockaddr *) &servaddr , sizeof(servaddr)) < 0 )
	{
		printf("\nERROR : Bind\n");
		return EXIT_FAILURE;
	}

	//listen
	if(listen(sockfd , 10) < 0)
	{
		printf("\nERROR : Listen\n");
		return EXIT_FAILURE;
	}

	printf("\n~~~ Welcome to TSI ~~~ss\n");

	while(1)
	{
		socklen_t clilen = sizeof(clientaddr);
		connfd = accept(sockfd , (struct sockaddr *) &clientaddr , &clilen);

		//check for max clients
		if((cli_count + 1) == MAX_CLIENTS)
		{
			printf("\nMaximum Clients connected ... Connection rejected ...\n");
			print_ip_addr(clientaddr);
			printf(":%d\n", clientaddr.sin_port);
			close(connfd);
			continue;
		}

		//client settings
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli -> address = clientaddr;
		cli -> sockfd = connfd;
		cli -> uid = uid++;

		//Add client to queue
		queue_add(cli);
		pthread_create(&tid , NULL , &handle_client , (void*)cli);

		//reduce CPU usage
		sleep(1);

	}
	return EXIT_SUCCESS;
}
