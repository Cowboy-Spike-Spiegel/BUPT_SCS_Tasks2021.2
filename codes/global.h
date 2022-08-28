#ifndef GLOBAL_H
#define GLOBAL_H

#include<process.h>


extern int level;//µ÷ÊÔµÈ¼¶


#define ASCIISIZE 128
#define URLMAXSIZE 100
#define IPMAXSIZE 16
#define LEN 512


#define TIMEMOD 1000

// seconds count
extern int timeCircle;
// run timeThread
void* timePass();


#endif