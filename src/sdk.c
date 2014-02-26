#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <pthread.h> 
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <sys/ioctl.h>
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <stdint.h>


#include "parser_sdk.h"
#include "mini_snmpd.h"
#include "log.h"
#include "server.h"


extern struct sdk_param external_sdk_53;

int main(void) {
	
	pthread_t thread;

	//----------thread serial exchange---------------
	
	int thread_serial;
        int thread_snmpd;

	thread_serial = pthread_create(&thread, NULL, &threading_sdk_serial, &external_sdk_53);

	if (thread_serial != 0) {
		write_log("Crash thread serial exchange");
	}
	thread_snmpd = pthread_create(&thread, NULL, &run_snmpd, NULL);
	//-----------------------------------------------
	
	//----------thread socket server-----------------
	
	//-----------------------------------------------
	
	/*while (i<20) {

		sleep(2);
		int i=0;
		for (i; i<20; i++) {

			printf("dry contact %d: %c\n", i, external_sdk_53.dry_contact[i]);
		}
		i++;
	}*/
	server_run();
	thread_serial = pthread_join(thread, NULL);
	
	return (1);
}