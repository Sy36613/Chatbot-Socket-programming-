/*
** talker.c -- a datagram "client" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "list.h"

char* lport;
char* rport;
char* rhost;
List* inList;
List* outList;
pthread_mutex_t inmutex, outmutex;
int done = 0, otherstatus = 0, receivestatus = 0;
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void encrypt(char* line) {
	for (int i = 0; i < strlen(line); i++) {
		if (line[i] == '\0' || line[i] == '\n') {
			break;
		}
		line[i] = line[i] + 1;
	}
}

void decrypt(char* line) {
	for (int i = 0; i < strlen(line); i++) {
		if (line[i] == '\0' || line[i] == '\n') {
			break;
		}
		line[i] = line[i] - 1;
	}
}

void *input(void *ptr) {
	char* line = (char *)malloc(256);
	while (done == 0 && fgets(line, 256, stdin)) {
		//printf("%s", line);
		pthread_mutex_lock(&inmutex);
		List_append(inList, line);
		//printf("%ld %s", strlen(line), line);
		if (strcmp(line, "!exit\n") == 0) {
			done = 1;
		}
		pthread_mutex_unlock(&inmutex);
	}
	//printf("input thread done\n");
}

void *output(void *ptr) {
	while (done == 0) {
		pthread_mutex_lock(&outmutex);
		if (List_count(outList) > 0) {
			void* vp = List_remove(outList);
			char* line = (char *)vp;
			printf("%s", line);
			if (strcmp(line, "!exit\n") == 0) {
				done = 1;
			}
		}
		pthread_mutex_unlock(&outmutex);
	}
	//printf("output thread done\n");
}

void *sender(void *ptr) {
	char * message = (char *)ptr;
	//printf("-------------sender thread--------------%s\n", message);
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

//	int hasStarted = 0;
//while (hasStarted == 0) {
	pthread_mutex_lock(&inmutex);
//	if (List_count(inList) > 0) {
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET; // set to AF_INET to use IPv4
		hints.ai_socktype = SOCK_DGRAM;

		if ((rv = getaddrinfo(rhost, rport, &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			//return;
		}

		// loop through all the results and make a socket
		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype,
					p->ai_protocol)) == -1) {
				perror("talker: socket");
				continue;
			}

			break;
		}

		if (p == NULL) {
			fprintf(stderr, "talker: failed to create socket\n");
			//return;
		}
//		hasStarted = 1;
//	}
	pthread_mutex_unlock(&inmutex);
//}

	char* line;
	while (done == 0) {
		pthread_mutex_lock(&inmutex);
		if (List_count(inList) > 0 || receivestatus == 1) {
			if (receivestatus == 1) {
				receivestatus = 0;
				char* line2 = "!acknowledgement\n";
				if ((numbytes = sendto(sockfd, line2, strlen(line2), 0, p->ai_addr, p->ai_addrlen)) == -1) {
					perror("talker: sendto");
					exit(1);
				}
			} else {
				void* vp = List_remove(inList);
				line = (char *)vp;
				if (strcmp(line, "!exit\n") == 0) {
					done = 1;
					encrypt(line);
					if ((numbytes = sendto(sockfd, line, strlen(line), 0, p->ai_addr, p->ai_addrlen)) == -1) {
						perror("talker: sendto");
						exit(1);
					}
				} else if (strcmp(line, "!status\n") == 0) {
					encrypt(line);
					if ((numbytes = sendto(sockfd, line, strlen(line), 0, p->ai_addr, p->ai_addrlen)) == -1) {
						perror("talker: sendto");
						exit(1);
					}
					sleep(2);
					if (otherstatus == 0) {
						printf("Offline\n");
					} else {
						printf("Online\n");
						otherstatus = 0;
					}
				} else {
					encrypt(line);
					if ((numbytes = sendto(sockfd, line, strlen(line), 0, p->ai_addr, p->ai_addrlen)) == -1) {
						perror("talker: sendto");
						exit(1);
					}
				}
			}	
			//printf("sent %d bytes to %s\n", numbytes, rhost);
		}
		pthread_mutex_unlock(&inmutex);
	}

	freeaddrinfo(servinfo);
	free(line);
	//printf("talker: sent %d bytes to %s\n", numbytes, rhost);
	close(sockfd);
	//printf("sender thread done\n");
}

void *receiver(void *ptr) {
	int sockfd, MAXBUFLEN = 100;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, lport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		//return;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		//return;
	}

	freeaddrinfo(servinfo);

	//printf("listener: waiting to recvfrom...\n");

	addr_len = sizeof their_addr;

	while (done == 0) {
		if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}

		//printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof s));
		//printf("listener: packet is %d bytes long\n", numbytes);
		buf[numbytes] = '\0';
		
		//printf("listener: packet contains \"%s\"\n", buf);
		pthread_mutex_lock(&outmutex);
		if (strcmp(buf, "!acknowledgement\n") == 0) {
			otherstatus = 1;
		} else {
			decrypt(buf);
			if (strcmp(buf, "!exit\n") == 0) {
				done = 1;
				List_append(outList, buf);
			} else if (strcmp(buf, "!status\n") == 0) {
				receivestatus = 1;
			} else {
				List_append(outList, buf);
			}
		}
		pthread_mutex_unlock(&outmutex);
	}

	close(sockfd);
	//printf("receiver thread done\n");
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		fprintf(stderr,"Usage:\n  ./lets-talk <local port> <remote host> <remote port>\nExamples:\n  ./lets-talk 3000 192.168.0.513 3001\n  ./lets-talk 3000 some-computer-name 3001\n");
		exit(1);
	}
	lport = argv[1], rport = argv[3], rhost = argv[2];
	printf("Welcome to LetS-Talk! Please type your message now.\n");
	inList = List_create();
	outList = List_create();
	
	pthread_t senderTid, receiverTid, inputTid, outputTid;
	pthread_create(&inputTid, NULL, input, "hello");
	pthread_create(&outputTid, NULL, output, "hello");
	pthread_create(&senderTid, NULL, sender, "hello");
	pthread_create(&receiverTid, NULL, receiver, "hello");
	//pthread_join(inputTid, NULL);
	pthread_join(outputTid, NULL);
	pthread_join(senderTid, NULL);
	//pthread_join(receiverTid, NULL);
	//printf("main: End\n");
	fflush(stdout);
	return 0;
}
