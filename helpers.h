#include <stdbool.h>

#ifndef _HELPERS_H
#define _HELPERS_H 1

#define PACKSIZ sizeof(struct Packet)
#define TCPSIZ sizeof(struct TCP)
#define UDPSIZ sizeof(struct UDP)

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

typedef struct Topic{
	char name[51];
	int sf;
} Topic;

//trimisa de client udp la server
typedef struct UDP{
	char topic[50];
	uint8_t type;
	char content[1501];
} UDP_msg;

//transmisa de server spre client tcp
typedef struct TCP {
	char ip[16];
	uint16_t port;
	char topic[51];
	char type[11];
	char content[1501];
} TCP_msg;

typedef struct Packet {
	char type;
	char ip[16];
	uint16_t port;
	char data_type;
	char topic[51];
	int sf;
	char content[1501];
} Packet;

typedef struct Client{
	char id[10];
	int socket;
	int topics_dim;
	struct Topic topics[100];
	int unsent_msg_dim;
	struct TCP unsent[100];
	bool active;
} Client;

#endif