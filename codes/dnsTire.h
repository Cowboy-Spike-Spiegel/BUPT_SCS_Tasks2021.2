#ifndef DNSTIRE_H
#define DNSTIRE_H

// struct for domain to IP
typedef struct tire
{
    // IP impormation
    char* IPAddress;

    // ptr array for the behind element which is ascii, thus contains 128 ptrs
    struct tire** nextEleArray;
} Tire, * TirePtr;

TirePtr buildTire(FILE* dnsFile);
void freeTrieNode(TirePtr node);
char* findIP(char* domain, TirePtr dnsRoot);

#endif
