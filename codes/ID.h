#ifndef ID_H
#define ID_H


#ifdef _WIN32
	#include<winsock2.h>
	#pragma comment(lib,"wsock32.lib")
#endif

#ifdef __linux__
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <sys/types.h>
#endif


// single ID record
typedef struct
{
	char* url;							// domain
	int urlLength;						// package length
	unsigned short Question_id;         // 客户端发给DNS服务器的ID
	SOCKADDR_IN client_addr;            // 请求者的客户端套接字
	int time;                  			// 进入的时间点
	BOOL finished;                      // 标记该请求是否已经完成
} record_ID, *record_IDptr;

// ID table
typedef struct
{
	record_IDptr records;	// array of record_ID
	int index;
} table_ID, *table_IDptr;


void initialize_table_ID(table_IDptr ID_table);
void findOutOfTime(table_IDptr ID_table, int my_socket, SOCKADDR_IN server_addr);
record_IDptr search_id(table_IDptr ID_table, unsigned short ID);
unsigned short save_id(table_IDptr ID_table, char* buf, int length, unsigned short ID, SOCKADDR_IN client_addr);


#endif