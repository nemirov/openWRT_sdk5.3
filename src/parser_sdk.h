#ifndef PARSER_SDK_H_
#define PARSER_SDK_H_

struct sdk_param {
	long int hw;
	long int sw;
	long int self_temp;
	long int relay;
	int optical_relay[4];
	long int dry_contact[20];
};

void * threading_sdk_serial(void * arg);

#endif /*PARSER_SDK_H_*/
