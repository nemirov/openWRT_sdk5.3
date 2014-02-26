#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <sys/ioctl.h>
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <stdint.h>
#include <sys/select.h>
#include "log.h"

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

#define _POSIX_SOURCE 1 
#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS0"
#define _POSIX_SOURCE 1
#define FALSE 0
#define TRUE 1
#define DEBUG 1


volatile int STOP=FALSE;
int wait_flag=TRUE;
char devicename[80];
long Baud_Rate;

long BAUD;
long DATABITS;
long STOPBITS;
long PARITYON;
long PARITY;

int Data_Bits = 8;
int Stop_Bits = 1;
int Parity = 0;
int Format = 4;

int open_serial_port(char device[], long baud_rate, int data_bits,
		int stop_bits, int parity) {

	int fd, error;
	struct termios oldtio, newtio;
	

	error=0;
	strcpy(devicename, device);

	Baud_Rate=baud_rate;
	Data_Bits=data_bits;
	Stop_Bits=stop_bits;
	Parity=parity;

	switch (Baud_Rate) {
	case 115200:
		BAUD = B115200;
		break;
	case 57600:
	default:
		BAUD = B57600;
		break;
	case 38400:
		BAUD = B38400;
		break;
	case 19200:
		BAUD = B19200;
		break;
	case 9600:
		BAUD = B9600;
		break;
	case 4800:
		BAUD = B4800;
		break;
	case 2400:
		BAUD = B2400;
		break;
	case 1800:
		BAUD = B1800;
		break;
	case 1200:
		BAUD = B1200;
		break;
	case 600:
		BAUD = B600;
		break;
	case 300:
		BAUD = B300;
		break;
	case 200:
		BAUD = B200;
		break;
	case 150:
		BAUD = B150;
		break;
	case 134:
		BAUD = B134;
		break;
	case 110:
		BAUD = B110;
		break;
	case 75:
		BAUD = B75;
		break;
	case 50:
		BAUD = B50;
		break;
	}

	switch (Data_Bits) {
	case 8:
	default:
		DATABITS = CS8;
		break;
	case 7:
		DATABITS = CS7;
		break;
	case 6:
		DATABITS = CS6;
		break;
	case 5:
		DATABITS = CS5;
		break;
	}

	switch (Stop_Bits) {
	case 1:
	default:
		STOPBITS = 0;
		break;
	case 2:
		STOPBITS = CSTOPB;
		break;
	}

	switch (Parity) {
	case 0:
	default:
		PARITYON = 0;
		PARITY = 0;
		break;
	case 1:
		PARITYON = PARENB;
		PARITY = PARODD;
		break;
	case 2:
		PARITYON = PARENB;
		PARITY = 0;
		break;
	}

	fd = open(devicename, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		perror(devicename);
		return 0;
	}

	fcntl(fd, F_SETOWN, getpid());

	tcgetattr(fd, &oldtio);
	newtio.c_cflag = BAUD | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL
			| CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VMIN]=0;
	newtio.c_cc[VTIME]=0;

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);
	if (DEBUG) {
		write_log("Serial Port /dev/ttyXXX is now open \n");
	}

	return fd;

}

int request_port(char *message, int len_mes, char answer[256],
		int file_descriptor) {

	int n, res;
	fd_set fs;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 150000;

	int fd = file_descriptor;

	printf("len_mes = %d\n", len_mes);

	n = write(fd, message, len_mes-1);
	if (n < 0) {
		if (DEBUG) {
			write_log("write() serial of failed!\n");
		}

	} 

	FD_ZERO(&fs);
	FD_SET(fd, &fs);

	char buffer[55];

	if (select(fd+1, &fs, NULL, NULL, &tv) < 0) {
		printf("Timeout for wait read fd");
		return (-1);
	};

	if (FD_ISSET(fd, &fs)) {

		usleep(150000);
		res = read(fd, &buffer, 56);
		printf("receiver: %d byte \n", res);

		if (res <= 0) {
			printf("read result < 0");
			return (-1);
		} else if (!res) {
			close(fd);
			FD_ZERO(&fs);
		} else {

			memset(answer, 0x00, res);
			memcpy(answer, buffer, res);
			return res;
		};

	};
}

