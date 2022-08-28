#ifndef PACKAGE_H
#define PACKAGE_H


int UrlInDns(char* DnsInfo, int DnsLength, char* UrlInDns);
int IpInDns(char* DnsInfo, int DnsLength, char* ipInDns);
int CreateResponse(char* DnsInfo, int DnsLength, char* FindIp, char* DnsResponse);
int CreateRequest(char* Domain, char* DnsInfo, unsigned short ID);

void NumToCharIp(char* NumStart, char* ipInDns);
int GetLengthOfDns(char* DnsInfo);


#endif