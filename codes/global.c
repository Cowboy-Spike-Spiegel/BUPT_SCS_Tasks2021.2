#include<windows.h>
#include<stdio.h>
#include"global.h"

// global variables for main
int level;//µ÷ÊÔµÈ¼¶

// for cache
int timeCircle;

void* timePass()
{
	timeCircle = 0;
	while (1) {
		Sleep(1000);
		timeCircle = (timeCircle + 1) % TIMEMOD;
		//printf("%d secs.\n", timeCircle);
	}
}