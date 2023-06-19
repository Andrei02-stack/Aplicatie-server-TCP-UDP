#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <limits.h>
#include "helpers.h"

int create_udp_socket(int port) {
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        error("Failed to create UDP socket");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        error("Failed to bind UDP socket");
    }

    return sockfd;
}

int create_tcp_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("Failed to create TCP socket");
    }

    int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0) {
        error("Failed to set TCP_NODELAY option");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        error("Failed to bind TCP socket");
    }

    if (listen(sockfd, INT_MAX) < 0) {
        error("Failed to listen on TCP socket");
    }

    return sockfd;
}

void udp_to_tcp(const struct UDP* udp, struct TCP* tcp) {
    int int_num;
    float real_num;
	int negative_powOf_10 = 1;

    switch (udp->type) {
        case 0: // numar natural
            int_num = ntohl(*(uint32_t *)(udp->content + 1));
            if (udp->content[0] == 1) { // numar negativ
                int_num *= -1;
            }
            sprintf(tcp->content, "%d", int_num);
            strcpy(tcp->type, "INT");
            break;

        case 1: // short real_num
            real_num = ntohs(*(uint16_t *)(udp->content));
            real_num /= 100;
            sprintf(tcp->content, "%.2f", real_num);
            strcpy(tcp->type, "SHORT_REAL");
            break;

        case 2: // float
            real_num = ntohl(*(uint32_t *)(udp->content + 1));
            for (int i = 1; i <= udp->content[5]; i++) {
                negative_powOf_10 *= 10;
            }
            real_num /= negative_powOf_10;
            if (udp->content[0] == 1) { // numar negativ
                real_num *= -1;
            }
            sprintf(tcp->content, "%lf", real_num);
            strcpy(tcp->type, "FLOAT");
            break;

        default: // string
            strcpy(tcp->content, udp->content);
            strcpy(tcp->type, "STRING");
            break;
    }
}

int main(int argc, char** argv) {

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	DIE(argc < 2, "< arguments");

	bool running = false;
	int server_port = atoi(argv[1]);

	//vector de clients
	struct Client *clients = calloc(1000, sizeof(struct Client));

	int tcp_sock = create_tcp_socket(server_port);
	int udp_sock = create_udp_socket(server_port);

	fd_set fd_read;
	FD_ZERO(&fd_read);
	FD_SET(tcp_sock, &fd_read);
	FD_SET(udp_sock, &fd_read);
	FD_SET(STDIN_FILENO, &fd_read);

	struct sockaddr_in udp_addr, new_tcp;
	socklen_t udp_len = sizeof(struct sockaddr);

	int fd_max = udp_sock;
	if(tcp_sock > udp_sock)
		fd_max = tcp_sock;

	while(!running) {

		char buffer[PACKSIZ];
		fd_set fd_tmp = fd_read;

		int sel = select(fd_max + 1, &fd_tmp, NULL, NULL, NULL);
		DIE(sel < 0, "select");

		//parcurgem toate socketurile
		for (int i = 0; i <= fd_max; i++) {
			if (FD_ISSET(i, &fd_tmp)) {
				memset(buffer, 0, sizeof(struct Packet));
				
				if(i == udp_sock){

					int err = recvfrom(udp_sock, buffer, 1551, 0, (struct sockaddr *)&udp_addr, &udp_len);
					DIE(err < 0, "recv from udp");

					struct UDP *udp; //ce primim
					struct TCP tcp; //ce transmitem

					//Initializam udp
					udp = (struct UDP *)buffer;

					//Initializam tcp la care se va trimite informatia
					tcp.port = htons(udp_addr.sin_port);
					strcpy(tcp.ip, inet_ntoa(udp_addr.sin_addr));
					strcpy(tcp.topic, udp->topic);
					tcp.topic[50] = 0;

					udp_to_tcp(udp, &tcp);

					for(int j = 5; j <= fd_max; j++) { //parcurgem clientii

						for(int k = 0; k < clients[j].topics_dim; k++) { //topicurile la care este abonat

							if (strcmp(clients[j].topics[k].name, tcp.topic) == 0) { //daca e abonat la topicul respectiv il trimitem
								if(clients[j].active){

									//client active => ii trimitem informatia
									int ret = send(clients[j].socket, &tcp, sizeof(struct TCP), 0);
									DIE(ret < 0, "send to client er");
								} 
								else {

									//daca clientul e offline ii salvam informatia in vectorul de unsent
									if(clients[j].topics[k].sf == 1) {
										clients[j].unsent[clients[j].unsent_msg_dim] = tcp;
										clients[j].unsent_msg_dim++;
									}
								}
								break;
							}
						}
					}
				}
				else if (i == tcp_sock) { //pentru socketul tcp primim informatii

					int socket = accept(tcp_sock, (struct sockaddr *) &new_tcp, &udp_len);
					DIE(socket < 0, "accept tcp er");

					recv(socket, buffer, 10, 0);

					//se cauta clientul cu id-ul dat in vectorul de clienti existenti
					int existing_client_index = -1, active = 0;
					for(int j = 5; j <= fd_max; j++) {
						if(strcmp(clients[j].id, buffer) == 0) {
							existing_client_index = j;
							active = clients[j].active;
							break;
						}
					}

					//daca clientul este nou(nu a fost gasit)
					if(existing_client_index == -1) {
						//adaugam socketul
						FD_SET(socket, &fd_read);
						if(socket > fd_max)
							fd_max = socket;

						//il adaugam la finalul vectorului
						strcpy(clients[fd_max].id, buffer);
						clients[fd_max].active = 1;
						clients[fd_max].socket = socket;

						//output pentru acest caz
						printf("New client %s connected from %s:%d\n", clients[fd_max].id,
							inet_ntoa(new_tcp.sin_addr), ntohs(new_tcp.sin_port));

					} 
					else if(existing_client_index && !active) { //e gasit si nu e active
						FD_SET(socket, &fd_read);
                        clients[existing_client_index].socket = socket;
						clients[existing_client_index].active = 1;
						

						printf("New client %s connected from %s:%d.\n", clients[existing_client_index].id,
							inet_ntoa(new_tcp.sin_addr), ntohs(new_tcp.sin_port));
						
						//se va trimite ce a ramas netrimis cat timp a fost offline
						for(int k = 0; k < clients[existing_client_index].unsent_msg_dim; k++){

							send(clients[existing_client_index].socket, &clients[existing_client_index].unsent[k], sizeof(struct TCP), 0);
						}
						clients[existing_client_index].unsent_msg_dim = 0;

					} 
					else { //deja conectat
						close(socket);
						printf("Client %s already connected.\n", clients[existing_client_index].id);
					}

				} 
				
				else if (i == STDIN_FILENO) { //socketul serverului (stdin)
					
					//gestionarea functiei de oprire
					fgets(buffer, 100, stdin);

					if(strncmp(buffer, "exit", 4) == 0) {
						running = 1;
						break;
					}

				} else { //posibili clienti noi

					int ret = recv(i, buffer, sizeof(struct Packet), 0);
					if (ret) {
  						struct Packet *input = (struct Packet *) buffer;
  						Client* client = NULL;

  						for (int j = 5; j <= fd_max; j++) {
    						if (i == clients[j].socket) {
      							client = &clients[j];
      						break;
    						}
  						}
					int index = -1;
  					switch (input->type) {
    				case 's': //tip pachet s = subscribe
      				for (int j = 0; j < client->topics_dim; j++) {
        				if (strcmp(client->topics[j].name, input->topic) == 0) {
          					index = j;
          				break;
        				}
      				}

      				//daca nu e deja abonat, ne abonam la topic (il adaugam in vectorul de topics)
      				if (index < 0) {
        			strcpy(client->topics[client->topics_dim].name, input->topic);
        			client->topics[client->topics_dim].sf = input->data_type;
        			client->topics_dim++;
      				}
     				 break;
    
    				case 'u': //tip pachet u = unsubscribe
      				for (int k = 0; k < client->topics_dim; k++) {
        				if (strcmp(client->topics[k].name, input->topic) == 0) {
          			index = k;
          			break;
        			}
      			}

     			 //daca clientul e abonat la topicul respectiv, ne dezabonam
      			if (index >= 0) {
        			for (int l = index; l < client->topics_dim; l++) {
          				client->topics[l] = client->topics[l + 1];
        			}
        			client->topics_dim--;
      			}
      		break;
    
    			case 'e': //tip pachet e = deconectare (exit)
      			for (int j = 5; j <= fd_max; j++) {
        			if (clients[j].socket == i) {
          				FD_CLR(i, &fd_read);
          				printf("Client %s disconnected.\n", clients[j].id);
          				clients[j].socket = -1;
          				clients[j].active = 0;
          				close(i);
          			break;
        			}
      			}
    			break;
    
    			default:
      			printf("Invalid type of packet");
     			break;
  			}
		}
					 else if (ret == 0) {
						for (int j = 5; j <= fd_max; j++)
							if(clients[j].socket == i) {

								FD_CLR(i, &fd_read);
								printf("Client %s disconnected.\n", clients[j].id);
								
								clients[j].socket = -1;
								clients[j].active = 0;
								
								close(i);
								break;
							}
					}
				}
			}
		}
	}
	for(int i = 3; i <= fd_max; i++) {
		if(FD_ISSET(i, &fd_read))
			close(i);
	}

	return 0;
}