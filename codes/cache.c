#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include"global.h"
#include"cache.h"


#define CAPACITY 20
#define REFRESHSIZE 10
#define OVERTIME 50


CachePtr createCache()
{
	CachePtr dnsCache = (CachePtr)malloc(sizeof(Cache));
	dnsCache->capicity = CAPACITY;
	dnsCache->size = 0;

	// create and initialize Record Head
	dnsCache->records = (RecordPtr)malloc(sizeof(Record));
	dnsCache->records->url = NULL;
	dnsCache->records->ip = NULL;
	dnsCache->records->next = NULL;

	return dnsCache;
}

char* find_WithRefresh(char* url, CachePtr dnsCache)
{
	char* ans = NULL;
	/*
		Find IP for this domain in cache:
			if record exists, refresh its time and put it on the top
	*/
	RecordPtr preNode = dnsCache->records;
	RecordPtr curNode = NULL;
	while (preNode->next != NULL) {
		// find ip
		if (strcmp(preNode->next->url, url) == 0) {
			ans = preNode->next->ip;
			if(level >= 1)
				printf("Cache find:\turl=%s\tip=%s\tcur=%d time=%d, put it on the top.\n", url, ans, timeCircle, preNode->next->time);

			// refresh it, and put it on the top
			RecordPtr curNode = preNode->next;
			curNode->time = timeCircle;
			preNode->next = curNode->next;
			curNode->next = dnsCache->records->next;
			dnsCache->records->next = curNode;

			break;
		}
		preNode = preNode->next;
	}

	return ans;
}

void addRecord(CachePtr dnsCache, char* url, char* ip)
{
	// find it on the top REFRESHSIZE, put it on the top
	if (find_WithRefresh(url, dnsCache) == NULL)
	{
		// don't find it
		RecordPtr curNode = (RecordPtr)malloc(sizeof(Record));
		curNode->url = (char*)malloc(sizeof(char) * URLMAXSIZE);
		memcpy(curNode->url, url, strlen(url) + 1);
		curNode->ip = (char*)malloc(sizeof(char) * IPMAXSIZE);
		memcpy(curNode->ip, ip, strlen(ip) + 1);
		curNode->time = timeCircle;
		curNode->next = dnsCache->records->next;
		dnsCache->records->next = curNode;

		dnsCache->size++;
		printf("Cache added URL=%s, IP=%s\n", url, ip);
		
		// delete time out of OVERTIME or out of capacity
		RecordPtr preNode = dnsCache->records;
		for (int count = 1; preNode->next != NULL; count++) {
			if (((timeCircle - preNode->next->time + TIMEMOD) % TIMEMOD) > OVERTIME || count >= CAPACITY) break;
			preNode = preNode->next;
		}
		while (preNode->next != NULL) {
			dnsCache->size--;
			if(level >= 1)
				printf("Cache delete:\turl=%s\tip=%s\tcur=%d time=%d\n", preNode->next->url, preNode->next->ip, timeCircle, preNode->next->time);
			curNode = preNode->next;
			preNode->next = curNode->next;
			free(curNode->url);
			free(curNode->ip);
			free(curNode);
		}
	}

	printCache(dnsCache);
}

void printCache(CachePtr dnsCache)
{
	RecordPtr preNode = dnsCache->records;
	if (level >= 1) {
		printf("\n--------Cache content:\n");
		for (int count = 1; preNode->next != NULL; count++) {
			printf("\t%d:\t%s\n\t\t%s\ttime=%d\n", count, preNode->next->url, preNode->next->ip, preNode->next->time);
			preNode = preNode->next;
		}
	}
}

void freeCache(CachePtr dnsCache)
{
	RecordPtr curNode;
	while (dnsCache->records->next != NULL) {
		curNode = dnsCache->records->next;
		dnsCache->records->next = curNode->next;
		free(curNode->url);
		free(curNode->ip);
		free(curNode);
	}
	free(dnsCache->records);
	free(dnsCache);
}