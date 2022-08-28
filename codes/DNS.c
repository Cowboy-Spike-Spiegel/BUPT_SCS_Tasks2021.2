#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<process.h>
#include<time.h>

#include"package.h"
#include"header.h"
#include"ID.h"
#include"dnsTire.h"
#include"cache.h"
#include"global.h"



// functions
void set_commandInfo(int argc, char* argv[], char* server_ip, char* file_name);
void initialize_socket(char* server_ip);


// socket variables -------------------------------------------------------------------------------
int my_socket;					// 套接字
SOCKADDR_IN client_addr;		// 客户端套接字地址
SOCKADDR_IN server_addr;		// 服务器套接字地址
SOCKADDR_IN tmp_sockaddr;		// 接收暂存地址
int sockaddr_in_size = sizeof(SOCKADDR_IN);


int main(int argc, char* argv[]) {
	
	// variables -----------------------------------------------------------------------------------
	// package
	char server_ip[IPMAXSIZE] = "192.168.1.1";	// 默认外部
	struct HEADER* p = NULL;

	// ID table
	table_IDptr table_ID = (table_IDptr)malloc(sizeof(table_ID));;
	initialize_table_ID(table_ID);

	// Cache and Tire
	CachePtr dnsCache = NULL;
	TirePtr dnsTire = NULL;

	// local
	FILE* dnsFile = NULL;
	char file_name[20] = "dnsrelay.txt";


	// argc-命令行中字符串的个数 argv-指向各个字符串 ----------------------------------------------------
	printf("\n\t*****************************************************\n");
	printf("\t*                   DNS Relay Server                *\n");
	printf("\t*****************************************************\n");
	printf("\n");
	printf("\tcommand format: dnsrelay [-d|-dd] [dns-server-ipaddr] [filename]\n\n");
	set_commandInfo(argc, argv, server_ip, file_name);	//读取判断类型


	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, timePass, NULL, 0, NULL);	// create thread for timer
	// initialize dbsCache and dnsTire ---------------------------------------------------------------
	// create dnsCache -------------------------------
	dnsCache = createCache();
	// create dnsTire --------------------------------
	if ((dnsFile = fopen(file_name, "r")) == NULL)
		printf("%s not exist.\n", file_name);
	else {
		dnsTire = buildTire(dnsFile);
		fclose(dnsFile);
	}


	// build socket ----------------------------------------------------------------------------------
	WSADATA wsadata;						// 存储Socket库初始化信息
	WSAStartup(MAKEWORD(2, 2), &wsadata);	// 根据版本通知操作系统，启用SOCKET的动态链接库
	printf("Start initialize socket.\n");
	initialize_socket(server_ip);			// 初始化 连接
	

	// start working ---------------------------------------------------------------------------------
	char buf[LEN] = { 0 };
	char converBuf[LEN] = { 0 };
	char url[URLMAXSIZE];
	char ip[IPMAXSIZE];
	char* curIP;
	/*
		my_socket：已连接的本地套接口
		buf:接收数据缓冲区
		sizeof(buf)：接收缓冲区大小
		0：标志位flag，表示调用操作方式，默认设为0
		client:捕获到的数据发送源地址（Socket地址）
		sockaddr_in_size:地址长度
		返回值：recvLength：成功接收到的数据的字符数（长度），接收失败返回SOCKET_ERROR(-1)
	*/
	while (1)
	{
		// initialize buf
		memset(buf, '\0', LEN);
		int length = recvfrom(my_socket, buf, sizeof(buf), 0, (SOCKADDR*)&tmp_sockaddr, &sockaddr_in_size);
		HEADER* p = (struct HEADER*)buf;
		if (level >= 2) {
			printf("\n\n---------------------- Reveived one package.");
			for (int i = 0; i < length; i++) printf("%02x ", buf[i]);
			printf("\tlen = %d\n", length);
		}
		
		// package from server ----------------------------------------------------------------------------------------------
		if (p->qr == 1)
		{
			printf("\n---------------------- Received from Server.\n");

			// IPv4和IPv6屏蔽机制（外部攻击）
			UrlInDns(buf, length, url);
			curIP = findIP(url, dnsTire);
			if (curIP && strcmp(curIP, "0.0.0.0") == 0) {
				printf("---------- Ipv6 package for %s has been thrown.\n", url);
				continue;
			}
			
			unsigned short id = p->id;
			// find in tableID. If find it, replace id and send to client
			record_IDptr req = search_id(table_ID, id, my_socket, server_addr);
			if (req != NULL) {
				id = req->Question_id;	// reansfer ID
				memcpy(buf, &id, sizeof(unsigned short));
				int sendLength = sendto(my_socket, buf, length, 0, (struct sockaddr*)&req->client_addr, sizeof(req->client_addr));
				printf("Find URL=%s in ID table, transfer ID and send to client.\n", url);

				if (level >= 2) {
					for (int i = 0; i < sendLength; i++) printf("%02x ", buf[i]);
					printf("\tlen = %d\n", length);
				}

				// Ipv4, save into cache
				if (UrlInDns(buf, length, url) == 1) {
					// catch ip in package, and refresh cache
					IpInDns(buf, length, ip);
					addRecord(dnsCache, url, ip);
				}
			}
		}

		// package from client ----------------------------------------------------------------------------------------------
		else
		{
			client_addr = tmp_sockaddr;
			printf("\n---------------------- Received from Client.\n");
			
			// Ipv4 ------------------------------------------------------------------------------------------------
			if (UrlInDns(buf, length, url) == 1) {
				printf("Ipv4 package has URL: %s\n", url);

				int flag = 0;
				// 1. 在cache里找 -------------------------------------------------------------------------
				if (level >= 1)
					printf("1. Find in Cache\n");
				curIP = find_WithRefresh(url, dnsCache);
				if (curIP != NULL)
					flag = 1;
				// 2. cache没找到，在Tire里找 --------------------------------------------------------------
				else {
					if (level >= 1)
						printf("2. Find in Tire\n");
					curIP = findIP(url, dnsTire);
					if (curIP != NULL) {
						flag = 1;
						addRecord(dnsCache, url, curIP);	// 加入该记录
					}
				}
				// Cache || Tire 找到，直接封装包发回给客户端
				if (flag == 1) {
					printf("Find IP in cache or tire: %s, send to client.\n", curIP);
					int respLen = CreateResponse(buf, length, curIP, converBuf);
					int sendLength = sendto(my_socket, converBuf, respLen, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					if (level >= 2) {
						for (int i = 0; i < sendLength; i++) printf("%02x ", converBuf[i]);
						printf("\tlen = %d\n", length);
					}
				}

				// 3. 没找到，用ID转换表，发送给服务端 -------------------------------------------------------
				else {
					printf("Don't find in cache or tire for url=%s, should send to server.\n", url);
					unsigned short transID = save_id(table_ID, buf, length, p->id, client_addr);

					// find record space to use, send to server
					if (transID != 0) {
						int sendLength = sendto(my_socket, buf, length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
						printf("In ID_table, find %dth record to use. Transfer it as this ID.\n", transID);
						if (level >= 2) {
							for (int i = 0; i < sendLength; i++) printf("%02x ", buf[i]);
							printf("\tlen = %d\n", length);
						}
					}
					else
						printf("Can't find space in ID_table, throw away this package.\n");
				}
			}
			
			// Ipv6, prevent or send to server------------------------------------------------------------------------
			else {
				// IPv6屏蔽机制
				char* curIP = findIP(url, dnsTire);
				printf("Ipv6 package has URL: %s\n", url);
				if (curIP && strcmp(curIP, "0.0.0.0") == 0) {
					printf("---------- Ipv6 package for %s has been prevented.\n", url);
					continue;
				}

				printf("Ipv6 package. Should straightly send to server.\n");
				unsigned short transID = save_id(table_ID, buf, length, p->id, client_addr);

				// find record space to use, send to server
				if (transID != 0) {
					int sendLength = sendto(my_socket, buf, length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
					printf("In ID_table, find %dth record to use. Transfer it as this ID.\n", transID);
					if (level >= 2) {
						for (int i = 0; i < sendLength; i++) printf("%02x ", buf[i]);
						printf("\tlen = %d\n", length);
					}
				}
				else
					printf("Can't find space in ID_table, throw away this package.\n");
			}
		}	
	}
	close(my_socket);
}



void set_commandInfo(int argc, char* argv[], char* server_ip, char* file_name) {
	// debug_level
	if (argc >= 2) {
		if (strcmp(argv[1], "-d") == 0)
			level = 1;
		else if (strcmp(argv[1], "-dd") == 0)
			level = 2;
		printf("Set debug_level: %d\n", level);
	}
	// server_ip
	if (argc >= 3) {
		strcpy(server_ip, argv[2]);
		printf("Set DNS server: %s\n", server_ip);
	}
	// File name
	if (argc >= 4) {
		strcpy(file_name, argv[3]);
		printf("Set File Name: %s\n", file_name);
	}
}

void initialize_socket(char* server_ip)
{
	my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// 创建套接字失败，重复尝试
	while (my_socket < 0) {
		printf("Socket build falied!\n");
		my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}
	printf("Socket build successfully! Socket: %d\n", socket);


	client_addr.sin_family = AF_INET;           //IPv4
	client_addr.sin_addr.s_addr = INADDR_ANY;   //本地ip地址随机
	client_addr.sin_port = htons(53);			//绑定到53端口

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(server_ip);//绑定到外部服务器
	server_addr.sin_port = htons(53);

	/* 本地端口号，通用端口号，允许复用端口 */
	int reuse = 0;
	setsockopt(my_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

	// 将端口号与socket关联
	int resp = bind(my_socket, (struct sockaddr*)&client_addr, sizeof(client_addr));
	while (resp < 0) {
		printf("Bind socket port failed\n");
		resp = bind(my_socket, (struct sockaddr*)&client_addr, sizeof(client_addr));
	}
	printf("Bind socket port successfully.\n");
}