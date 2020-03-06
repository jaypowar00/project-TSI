#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<unistd.h>

#include<errno.h>
#include<pthread.h>
#include<signal.h>


#define SA struct sockaddr

volatile sig_atomic_t flag = 0;
char string[2048];
char name[32];
int sockfd=0;

void str_overwite_stdout()
{
	printf("\r%s","> ");
	fflush(stdout);
}

void str_trim(char *arr, int len)
{
	for(int i=0;i<len;i++)
	{
		if(arr[i] == '\n')
		{
			arr[i] = '\0';
			break;
		}
	}
}

void ctrl_c(){
	flag = 1;
}

void send_f(){
	char buffer[2048] = {};
	char msg[2048+35] = {};
	while(1){
		str_overwite_stdout();
		gets(buffer,2048);
		str_trim(buffer,2048);

		if(strcmp(buffer,"exit") == 0){
			break;
		}else{
			if(strcmp(buffer,"set-admin")==0 || strcmp(buffer,"unset-admin")==0){
				send(sockfd,buffer,strlen(buffer),0);
			}else{
				sprintf(msg,"%s : %s\n",name,buffer);
				send(sockfd,msg,strlen(msg),0);
			}
		}
		bzero(buffer,2048);
		bzero(msg,2048+35);
	}
	ctrl_c(2);
}
void recv_f(){
	char	msg[2048];
	while(1){
		int r = recv(sockfd,msg,2048,0);
		if(r>0){
			printf("%s\n",msg);
			str_overwite_stdout();
		} else if(r==0){
			break;
		}
		bzero(msg,2048);
	}
}

int main(int argc,char **argv)
{
	if(argc == 3 || argc == 2){;}
	else
	{
		printf("\nInvalid number of arguments passed...\n(hint : syntax is like './a.out <port_no> <ip_address>')\n");
		return EXIT_FAILURE;
	}

	signal(SIGINT, ctrl_c);

	printf("Enter your name: ");
	fgets(name,32,stdin);
	str_trim(name,strlen(name));

	if(strlen(name) > 32 || strlen(name) < 2)
	{
		printf("\nEnter the name correctly...\n");
		return EXIT_FAILURE;
	}


	struct sockaddr_in servaddr,cliaddr;
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0)
	{
		printf("\nERROR: socket()\n");
		return EXIT_FAILURE;
	}
	else
		printf("\nsocket successfully created..\n");
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[1]));
	if(argc == 3)
		servaddr.sin_addr.s_addr = inet_addr(argv[2]);
	else
	{
		printf("\nserver ip not provided , so using local ip address...\n");
		servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	}

	if (connect(sockfd,(SA*)&servaddr,sizeof(servaddr))<0)
	{
		printf("\nERROR: Connect()\nServer might be busy...\n");
		return EXIT_FAILURE;
	}
	send(sockfd,name,sizeof(name),0);
	printf("\nconnected to the server...\n");

	pthread_t send_t;
	if(pthread_create(&send_t , NULL , (void*)send_f, NULL)!=0)
	{
		printf("\nError: pthread\n");
		return EXIT_FAILURE;
	}

	pthread_t recv_t;
	if(pthread_create(&recv_t , NULL , (void*)recv_f, NULL)!=0)
	{
		printf("\nError: pthread\n");
		return EXIT_FAILURE;
	}

	while(1)
	{
		if(flag)
		{
			printf("\nexited\n");
			break;
		}
	}

	close(sockfd);
	return EXIT_SUCCESS;
}
