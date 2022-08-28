#include"package.h"


//参数说明：DnsInfo 原始报文 ， DnsLength 报文长度 ， UrlInDns 请求的域名 
//返回说明： IPV4返回1 ，否则返回0 
int UrlInDns(char * DnsInfo , int DnsLength , char * UrlInDns)
{
	int i = 12, j, k = 0;
	while (DnsInfo[i] != 0)
	{
		for (j = 0; j < DnsInfo[i]; j++)
		{
			UrlInDns[k] = DnsInfo[i + j + 1];
			k++;
		}
		UrlInDns[k] = '.';
		k++;
		i = i + j + 1;
	}
	UrlInDns[k-1] = 0;
	for (j = 0; UrlInDns[j + 3] != 0; j++)
	{
		if (UrlInDns[j] == 'D' && UrlInDns[j + 1] == 'H' && UrlInDns[j + 2] == 'C' && UrlInDns[j + 3] == 'P')
		{
			UrlInDns[j - 1] = 0;
			UrlInDns[j + 4] = 0;
		}
	}
	return (!(DnsInfo[i + 2] - 1))&&(!DnsInfo[i + 1]);
}


//参数说明：DnsInfo为报文  ， DnsLength为长度 ， ipInDns 为存放IP的地方 
//返回说明：返回1. 
int IpInDns(char *DnsInfo , int DnsLength , char *ipInDns)
{
	int i = 12;
	int j  , offset=0;
	int qnum=0 , anum=0; 
	int Qnum = DnsInfo[5];
	int Anum = DnsInfo[7];
	for(qnum;qnum<Qnum;qnum++)
	{
		while(DnsInfo[i]!=0)
		{
			for(j=0;j<DnsInfo[i];j++)
			{
			
			}
			i = i + j + 1;
		}
	}
	i = i + 4;
	for(anum ; anum<Anum-1;anum++)
	{
		offset = offset + 12 + DnsInfo[i+offset+12];
	}
	i = i + offset + 12;
	NumToCharIp(&DnsInfo[i+1] , ipInDns);
} 


//参数说明：DnsInfo 原始报文 ， DnsLength 报文长度 ， FindIp 找到的IP ， DnsResponse 回复报文 
//返回说明： 返回回复报文的长度 
int CreateResponse(char *DnsInfo , int DnsLength , char *FindIp , char *DnsResponse)
{
	int LengthOfDns  =  GetLengthOfDns(DnsInfo);
	memcpy(DnsResponse, DnsInfo, LengthOfDns);
	unsigned short a = htons(0x8180);
	if(DnsLength==LengthOfDns)
	{
		memcpy(&DnsResponse[2], &a, sizeof(unsigned short));//更改标志位 
	}
	else
	{
		DnsLength = LengthOfDns;
		IpInDns(DnsInfo , DnsLength , FindIp);
	}
	if (strcmp(FindIp,(char *) "0.0.0.0") == 0)
	{
		a = htons(0x0000);//如果找到的IP为0.0.0.0，则屏蔽，回答数为0 
	}
	else
	{
		a = htons(0x0001);//找到ip，因为是在本地的找到，回答数为1 
	}
	memcpy(&DnsResponse[6], &a, sizeof(unsigned short));
	int curLen = 0;
	char answer[16];
	unsigned short Name = htons(0xc00c);
	memcpy(answer, &Name, sizeof(unsigned short));
	curLen += sizeof(unsigned short);

	unsigned short TypeA = htons(0x0001); 
	memcpy(answer + curLen, &TypeA, sizeof(unsigned short));
	curLen += sizeof(unsigned short);

	unsigned short ClassA = htons(0x0001);  
	memcpy(answer + curLen, &ClassA, sizeof(unsigned short));
	curLen += sizeof(unsigned short);

	unsigned long timeLive = htonl(0x7b); 
	memcpy(answer + curLen, &timeLive, sizeof(unsigned long));
	curLen += sizeof(unsigned long);

	unsigned short IPLen = htons(0x0004); 
	memcpy(answer + curLen, &IPLen, sizeof(unsigned short));
	curLen += sizeof(unsigned short);

	unsigned long IP = (unsigned long)inet_addr(FindIp); 
	memcpy(answer + curLen, &IP, sizeof(unsigned long));
	curLen += sizeof(unsigned long);
	curLen += DnsLength;
	memcpy(DnsResponse + DnsLength, answer, sizeof(answer));
 
	return  curLen;
}


//参数说明：Domain为域名 ， DnsInfo 为报文 存放地点 
//返回说明：返回长度 
int CreateRequest(char * Domain, char * DnsInfo, unsigned short ID)
{
	int i , j , k;
	unsigned short id = ID;
	memcpy(DnsInfo, &id, sizeof(unsigned short));
	DnsInfo[2] = 1;
	DnsInfo[3] = 0;
	DnsInfo[4] = 0;
	DnsInfo[5] = 1;
	DnsInfo[6] = 0;
	DnsInfo[7] = 0;
	DnsInfo[8] = 0;
	DnsInfo[9] = 0;
	DnsInfo[10] = 0;
	DnsInfo[11] = 0;
	k = 12 ;
	for(i=0;Domain[i]!=0;i++)
	{
		j = 1;
		while(Domain[i]!='.'&&Domain[i]!=0)
		{
			DnsInfo[k+j] = Domain[i];
			i++;
			j++;
		}
		DnsInfo[k] = j-1;
		//printf("%d %d\n" , k , DnsInfo[k]);
		k = k+j;
	}
	//printf("k:%d\n" , k);
	DnsInfo[k] = 0 ;
	DnsInfo[k+1] = 0 ;
	DnsInfo[k+2] = 1 ;
	DnsInfo[k+3] = 0 ;
	DnsInfo[k+4] = 1 ;
	return k+4+1;
}


//将long形式的IP转化成字符串IP 
void NumToCharIp(char * NumStart , char * ipInDns)
{
	int i , k=0;
	unsigned char xxhh;
	for(i=0;i<4;i++)
	{
		xxhh = NumStart[i];
		if(xxhh/100>0)
		{
			ipInDns[k] = xxhh/100+'0';
			k++;
		}
		if(xxhh/10>0)
		{
			ipInDns[k] = (xxhh%100)/10 + '0';
			k++;
		}
		ipInDns[k] = xxhh%10+ '0';
		k++;
		ipInDns[k] = '.';
		k++;
	}
	ipInDns[k-1] = 0;
}


//得到长度 
int GetLengthOfDns(char *DnsInfo)
{
	int length = 12;
	while (DnsInfo[length] != 0)
	{
		length++;
	}
	length = length + 5;
	return length;
}