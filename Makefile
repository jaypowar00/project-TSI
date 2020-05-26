
all:
	gcc -Wall -g3 -fsanitize=address -pthread TSI_server.c -o ser
	gcc -Wall -g3 -fsanitize=address -pthread TSI_client.c -o cli
	sleep 1
	clear
	
serv:
	./ser 1111
	
clie:
	./cli 1111
	
