#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "helpers.h"

void send_packet(char *buffer, int new_socket, struct Packet *pack) {
    char *string = strtok(buffer, " ");
    string = strtok(NULL, " ");
    strcpy(pack->topic, string);
    string = strtok(NULL, " ");
    pack->data_type = string[0] - '0';

    int sen = send(new_socket, pack, sizeof(struct Packet), 0);
    DIE(sen < 0, "send");
}

void handle_input(int new_socket, fd_set fd_read, int running) {
    char buffer[100];
    fgets(buffer, 100, stdin);

    if(strncmp(buffer, "subscribe", 9) == 0) {
        // handle subscribe command
        struct Packet pack;
        pack.type = 's';

        send_packet(buffer, new_socket, &pack);

        printf("Subscribed to topic.\n");
    } else if (strncmp(buffer, "unsubscribe", 11) == 0) {
        // handle unsubscribe command
        struct Packet pack;
        pack.type = 'u';

        send_packet(buffer, new_socket, &pack);

        printf("Unsubscribed to topic.\n");
    } else if(strncmp(buffer, "exit", 4) == 0) {
        // handle exit command
        struct Packet pack;
        pack.type = 'e';

        int sen = send(new_socket, &pack, sizeof(struct Packet), 0);
        DIE (sen < 0, "send_err");
		
        running = 0;
    } else {
        printf("Input invalid.\n");
    }
}

int main(int argc, char** argv) {

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc != 4) {
        printf("Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", argv[0]);
        return -1;
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(argv[2], argv[3], &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    int new_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    DIE(new_socket < 0, "socket er");

    int ret = connect(new_socket, res->ai_addr, res->ai_addrlen);
    DIE(ret < 0, "connect server");

    freeaddrinfo(res);

    //Se trimite ip-ul
    ret = send(new_socket, argv[1], 10, 0);
    DIE(ret < 0, "send1");

    fd_set fd_read;
    FD_ZERO(&fd_read);
    FD_SET(new_socket, &fd_read);
    FD_SET(STDIN_FILENO, &fd_read);

	int enable = 1;
    int running = 1;

	//
	setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(int));

	//Gestionam inputurile primite de la server sau din stdin
	while(running) {
		fd_set fd_tmp = fd_read;

		select(new_socket + 1, &fd_tmp, NULL, NULL, NULL);

		if (FD_ISSET(STDIN_FILENO, &fd_tmp)) { //input stdin
			handle_input(new_socket, fd_read, running);
		}

		if(FD_ISSET(new_socket, &fd_tmp)) { //informatie primita de la server

			char buffer[TCPSIZ];

			int recv1 = recv(new_socket, buffer, sizeof(struct TCP), 0);

			if(recv1 == 0)
				break;

			struct TCP *pack_send = (struct TCP *)buffer;
			printf("%s:%u - %s - %s - %s\n", pack_send->ip, pack_send->port, pack_send->topic, 
					pack_send->type, pack_send->content);
		}
	}

	close(new_socket);
	return 0;
}