
all:
	gcc -Wall -g3 -fsanitize=address -pthread server.c -o ser
	gcc -Wall -g3 -fsanitize=address -pthread client.c -o cli
	sleep 1
	clear
	
serv:
	./ser 1111
	
clie:
	./cli 1111
	
