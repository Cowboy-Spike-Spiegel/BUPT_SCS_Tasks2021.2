#include"ID.h"
#include"global.h"

#define ID_SIZE 72
#define TTL 10


// initialize
void initialize_table_ID(table_IDptr ID_table)
{
	ID_table->records = (record_IDptr)malloc(sizeof(record_ID) * ID_SIZE);
	ID_table->index = 0;
	for (int i = 0; i < ID_SIZE; i++)
	{
		ID_table->records[i].url = (char*)malloc(sizeof(char) * URLMAXSIZE);
		ID_table->records[i].urlLength = 0;
		ID_table->records[i].Question_id = 0;
		ID_table->records[i].finished = TRUE;
		ID_table->records[i].time = -1;
		memset(&(ID_table->records[i].client_addr), 0, sizeof(SOCKADDR_IN));
	}
}

// resend out of time
void findOutOfTime(table_IDptr ID_table, int my_socket, SOCKADDR_IN server_addr)
{
	for (int i = 0; i < ID_SIZE; i++) {
		// out of time
		if (ID_table->records[i].urlLength != 0 && ((timeCircle - ID_table->records[i].time + TIMEMOD) % TIMEMOD) > TTL/2) {
			ID_table->records[i].time = timeCircle;
			// resend
			//sendto(my_socket, ID_table->records[i].buf, ID_table->records[i].length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
			//printf("resend: %s\n", ID_table->records[i].buf);
		}
	}
}

// 根据下标寻找
record_IDptr search_id(table_IDptr ID_table, unsigned short ID)
{
	if(ID_table->records[ID - 1].finished == FALSE)
		return &(ID_table->records[ID - 1]);
	return NULL;
}

// 根据下标生成转发id
unsigned short save_id(table_IDptr ID_table, char* buf, int length, unsigned short ID, SOCKADDR_IN client_addr)
{
	if(level >= 2)
		printf("\nFind space in ID table.\nIn record: ");
	unsigned short transID = 0;

	int i = ID_table->index;
	do {
		// refresh state(overTime)
		if (((timeCircle - ID_table->records[i].time + TIMEMOD) % TIMEMOD) > TTL)
			ID_table->records[i].finished = TRUE;
		if (level >= 2)
			printf("%d-%d ", i + 1, ID_table->records[i].finished);

		// has found space for record, conver buf with replacing ID
		if (ID_table->records[i].finished == TRUE) {
			transID = (unsigned short)(i + 1);	// new_id，对应存储在的空间下标+1
			memcpy(buf, &transID, sizeof(unsigned short));	// 构造转发包的id，对应下标

			// 然后存入ID表
			if (UrlInDns(buf, length, ID_table->records[i].url))
				ID_table->records[i].urlLength = strlen(ID_table->records[i].url);
			else
				ID_table->records[i].urlLength = 0;
			ID_table->records[i].Question_id = ID;
			ID_table->records[i].client_addr = client_addr;
			ID_table->records[i].finished = FALSE;
			ID_table->records[i].time = timeCircle;	// 0-999

			i = (i + 1 + ID_SIZE) % ID_SIZE;
			break;
		}
		i = (i + 1 + ID_SIZE) % ID_SIZE;
	} while (i != ID_table->index);

	ID_table->index = i;	// refresh index
	return transID;
}


// 释放ID表
void free_ID(table_IDptr ID_table) {
	for (int i = 0; i < ID_SIZE; i++) {
		free(ID_table->records[i].url);
	}
	free(ID_table->records);
	free(ID_table);
}