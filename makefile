server: socket.c
	gcc -g -Wall -Wextra socket.c -o server
clean:
	rm server
