#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include "log.h"

#define DEBUG

int write_log(char *log){
    openlog("SDK ",LOG_PID,LOG_USER);
    syslog(LOG_INFO,"Debug programm SDK: %s", log);
    closelog();
    return 0;
}; 
