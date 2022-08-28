#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include"global.h"
#include"dnsTire.h"


/* build Tire */
TirePtr buildTire(FILE* dnsFile)
{
    /* create head */
    TirePtr dnsRoot = (TirePtr)malloc(sizeof(Tire));
    dnsRoot->IPAddress = NULL;
    dnsRoot->nextEleArray = (TirePtr*)malloc(sizeof(TirePtr) * ASCIISIZE);
    memset(dnsRoot->nextEleArray, 0, sizeof(TirePtr) * ASCIISIZE);

    // build for each domain
    TirePtr curNode;
    char url[URLMAXSIZE];
    char ip[IPMAXSIZE];
    char select[2];
    while (fscanf(dnsFile, "%s %s", ip, url) > 0)
    {
        if(level >= 1)
            printf("[ URL : %s, IP : %s ]\n", url, ip);

        // for this domain, find from root and build paths which not exist
        curNode = dnsRoot;
        int len = (int)strlen(url);
        for (int i = 0; i < len; i++)
        {
            // if path not exist, build it
            if (curNode->nextEleArray[url[i]] == NULL) {
                curNode->nextEleArray[url[i]] = (TirePtr)malloc(sizeof(Tire));
                curNode->nextEleArray[url[i]]->IPAddress = NULL;
                curNode->nextEleArray[url[i]]->nextEleArray = (TirePtr*)malloc(sizeof(TirePtr) * ASCIISIZE);
                memset(curNode->nextEleArray[url[i]]->nextEleArray, 0, sizeof(TirePtr) * ASCIISIZE);
            }
            curNode = curNode->nextEleArray[url[i]];
        }

        // this node is for this domain
        if (curNode->IPAddress == NULL) {
            curNode->IPAddress = (char*)malloc(sizeof(char) * (strlen(ip) + 1));
            memcpy(curNode->IPAddress, ip, strlen(ip) + 1);
        }
        // if IP exists ans not equal to current one, select for replacing
        else if (strcmp(curNode->IPAddress, ip) != 0) {
            // has conflits, select for replace
            printf("\t%s: former IP: %s, current IP: %s.\n", url, curNode->IPAddress, ip);
            printf("\tIP for this domain has been built, do you want to replace the former one?[y/n]  ");
			gets(select);
            if (select[0] == 'y' || select[0] == 'Y') {
                free(curNode->IPAddress);
                curNode->IPAddress = (char*)malloc(sizeof(char) * (strlen(ip) + 1));
                memcpy(curNode->IPAddress, ip, strlen(ip) + 1);
                printf("\n\treplace successfully.\n");
            }
            else printf("\n\talready skip it.\n");
        }
    }
    printf("Tire build ended.\n\n");

    return dnsRoot;
}

/* free Tire */
void freeTrieNode(TirePtr node)
{
    if (node->IPAddress != NULL)
        free(node->IPAddress);
    for (int i = 0; i < ASCIISIZE; i++) {
        if (node->nextEleArray[i] != NULL)
            freeTrieNode(node->nextEleArray[i]);
    }
    free(node->nextEleArray);
    free(node);
}

/* domain -> IP */
char* findIP(char* domain, TirePtr dnsRoot)
{
    if (dnsRoot == NULL)
        return NULL;

    TirePtr curNode = dnsRoot;
    int len = (int)strlen(domain);
    for (int i = 0; i < len; i++) {
        // can't find path
        if (curNode->nextEleArray[domain[i]] == NULL)
            return NULL;
        curNode = curNode->nextEleArray[domain[i]];
    }
    // has found it
    return curNode->IPAddress;
}