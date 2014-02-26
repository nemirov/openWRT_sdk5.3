#include <stdio.h>   /* Standard input/output definitions */
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <pthread.h> 
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <sys/ioctl.h>
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <stdint.h>
#include <sys/select.h>

#include "parser_sdk.h"
#include "serial.h"
#include "log.h"

#define DEBUG 1

long int ascii_to_int(char *);
void * threading_sdk_serial(void * arg);

long int ascii_to_int(char *data) {

        char *ptr;
        long ret;
        ret = strtol(data, &ptr, 16);
        return (ret);
}



void * threading_sdk_serial(void * arg) {
        struct sdk_param *sdk_serial = (struct sdk_param *) arg;

        int len_answer_sdk = -1;
        int fd, i;

        char answer_sdk[256];
        char get_status[] = "TSC10173\r\n";
        char word_2[] = "TSC11576\r\n";
        char word_3[] = "TSC1C105000000000005\r\n";

        fd = open_serial_port("/dev/ttyUSB0", 57600, 8, 1, 0);

        if (fd != 0) {
                while (1) {
                        usleep(550000);
                        /* init sdk and request status device */

                        int len_message = sizeof(get_status) / sizeof(get_status[0]);
                        len_answer_sdk = request_port(get_status, len_message, answer_sdk,
                                        fd);

                        while (len_answer_sdk <= 0) {
                                int len_message = sizeof(get_status) / sizeof(get_status[0]);
                                len_answer_sdk = request_port(get_status, len_message,
                                                answer_sdk, fd);
                                sleep(1);
                                if (DEBUG) {
                                        write_log("Not response from SDK");
                                }

                        }

                        /* parsing response data */

                        /* internal temperature */
                        char tmp_buf[2];
                        tmp_buf[0] = answer_sdk[40];
                        tmp_buf[1] = answer_sdk[41];
                        sdk_serial->self_temp = ascii_to_int(tmp_buf);
                       

                        /* version hardware */
                        tmp_buf[0] = answer_sdk[6];
                        tmp_buf[1] = answer_sdk[7];
                        sdk_serial->hw = ascii_to_int(tmp_buf);
                      

                        /* version software */
                        tmp_buf[0] = answer_sdk[8];
                        tmp_buf[1] = answer_sdk[9];
                        sdk_serial->sw = ascii_to_int(tmp_buf);
                        

                        /* electromagnetic relay */
                        sdk_serial->relay = answer_sdk[19];
                        

                        /* dry contact */
                        i=0;
                        for (i; i<20; i++) {
                                sdk_serial->dry_contact[i] = answer_sdk[i+20];
                        }

                        //--------------------------
                        i = 0;
                        while (i < len_answer_sdk) {
                                printf("%d -- %d\n", answer_sdk[i] - '0', (unsigned char)answer_sdk[i]);

                                i ++;
                        }
                        //--------------------------


                        /* send word_2 */
                        len_message = sizeof(word_2) / sizeof(word_2[0]);
                        len_answer_sdk = request_port(word_2, len_message, answer_sdk, fd);

                        //--------------------------
                        i = 0;
                        while (i < len_answer_sdk) {
                                printf("%c", answer_sdk[i]);
                                i ++;
                        }
                        //--------------------------


                        /*  send word_3  */
                        len_message = sizeof(word_3) / sizeof(word_3[0]);
                        len_answer_sdk = request_port(word_3, len_message, answer_sdk, fd);

                };
        } 
       
}

