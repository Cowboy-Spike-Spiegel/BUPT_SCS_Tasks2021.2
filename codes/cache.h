#ifndef CACHE_H
#define CACHE_H

#include"dnsTire.h"


/* cache unit */
typedef struct record
{
	// information for this record
	char* url;
	char* ip;
	int time;

	struct record* next;
} Record, * RecordPtr;

/* cache */
typedef struct cache
{
	RecordPtr records;
	int capicity;
	int size;
} Cache, * CachePtr;

// initialize cache
CachePtr createCache();
// find IP in cache, if not exist, find in dnsTire
char* find_WithRefresh(char* url, CachePtr dnsCache);
void addRecord(CachePtr dnsCache, char* url, char* ip);
void printCache(CachePtr dnsCache);
void freeCache(CachePtr dnsCache);

#endif