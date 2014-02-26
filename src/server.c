/* 
 * Roberto Acevedo
 * Multi-threaded socket server
 * COP4610
 * server.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include "parser_sdk.h"

#define NTHREADS 50
#define QUEUE_SIZE 5
#define BUFFER_SIZE 256
#define PORT "32001"

#define HW 		"get_hw"
#define SW 		"get_sw"
#define TEMP 	"get_temp"
#define RELAY 	"get_relay"
#define DRY 	"get_dry"
#define OPTICAL	"get_optical"
#define ALL		"get_all"

struct sdk_param external_sdk_53;

pthread_t threadid[NTHREADS];
pthread_mutex_t lock;
int counter = 0;

void *threadworker(void *arg) {

	int sockfd, rw;
	char *buffer;
	sockfd = (int) arg;

	buffer = malloc(BUFFER_SIZE);
	bzero(buffer, BUFFER_SIZE);

	rw = read(sockfd, buffer, BUFFER_SIZE);
	if (rw < 0) {
		perror("Error reading form socket, exiting thread");
		pthread_exit(0);
	}

	printf("New message received: %s\n", buffer);

	if (strcmp(buffer, HW) == 0) {
		bzero(buffer, BUFFER_SIZE);
		sprintf(buffer, "%d", external_sdk_53.hw);
	}
	if (strcmp(buffer, SW) == 0) {
		bzero(buffer, BUFFER_SIZE);
		sprintf(buffer, "%d", external_sdk_53.sw);
	}
	if (strcmp(buffer, TEMP) == 0) {
		bzero(buffer, BUFFER_SIZE);
		sprintf(buffer, "%d", external_sdk_53.self_temp);
	}
	if (strcmp(buffer, RELAY) == 0) {
		bzero(buffer, BUFFER_SIZE);
		sprintf(buffer, "%d", external_sdk_53.relay);
	}
	if (strcmp(buffer, OPTICAL) == 0) {
			bzero(buffer, BUFFER_SIZE);

			int i = 0;
			for (i; i<20; i++) {
				buffer[i] = external_sdk_53.optical_relay[i];
			}

		}
	if (strcmp(buffer, DRY) == 0) {
		bzero(buffer, BUFFER_SIZE);

		int i = 0;
		for (i; i<20; i++) {
			buffer[i] = external_sdk_53.dry_contact[i];
		}

	}

	rw = write(sockfd, buffer, strlen(buffer));

	if (rw < 0) {
		perror("Error writing to socket, exiting thread");
		pthread_exit(0);
	}

	pthread_mutex_lock(&lock);

	pthread_mutex_unlock(&lock);
	close(sockfd);
	pthread_exit(0);

}

int server_run() {

	int serv_sockfd, new_sockfd;
	struct addrinfo flags;
	struct addrinfo *host_info;
	socklen_t addr_size;

	struct sockaddr_storage client;

	pthread_attr_t attr;
	int i;

	memset(&flags, 0, sizeof(flags));
	flags.ai_family = AF_INET;
	flags.ai_socktype = SOCK_STREAM;
	flags.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, PORT, &flags, &host_info) < 0) {
		perror("Couldn't read host info for socket start");
		exit(-1);
	}

	serv_sockfd = socket(host_info->ai_family, host_info->ai_socktype,
			host_info->ai_protocol);

	if (serv_sockfd < 0) {
		perror("Error opening socket");
		exit(-1);
	}

	if (bind(serv_sockfd, host_info->ai_addr, host_info->ai_addrlen) < 0) {
		perror("Error on binding");
		exit(-1);
	}

	freeaddrinfo(host_info);

	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	listen(serv_sockfd, QUEUE_SIZE);

	addr_size = sizeof(client);
	i = 0;

	while (1) {
		if (i == NTHREADS) {
			i = 0;
		}

		new_sockfd = accept(serv_sockfd, (struct sockaddr *) &client,
				&addr_size);

		if (new_sockfd < 0) {
			perror("Error on accept");
			exit(-1);
		}

		pthread_create(&threadid[i++], &attr, &threadworker,
				(void *) new_sockfd);
		sleep(0);
	}

	return 0;
}
