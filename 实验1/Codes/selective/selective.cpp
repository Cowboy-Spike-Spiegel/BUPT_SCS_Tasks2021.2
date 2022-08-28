#include <stdio.h>
#include <string.h>

#include "protocol.h"

//缓冲区
#define MAX_SEQ 15
#define NR_BUFS ((MAX_SEQ+1)/2)

//定时器
#define DATA_TIMER  3712       //帧超时时间间隔
#define ACK_TIMER	263       //ack的超时间隔
	
//帧类型
#define FRAME_DATA 1
#define FRAME_ACK  2
#define FRAME_NAK  3

typedef unsigned char f_kind;
typedef unsigned char f__ack;
typedef unsigned char f__seq;

struct FARME {
	f_kind kind;
	f__ack ack;
	f__seq seq;
	unsigned char data[PKT_LEN];
	unsigned int  padding;
};
typedef struct {
	int nak;                    //是否发送nak
	int phl_ready;              //物理层是否准备好
}other_param;

//判断序号是否在窗口里面
static int judge_between(unsigned char a, unsigned char b, unsigned char c)
{
	return ((a <= b && b < c) || ((a <= b||b<c) && c < a));
}

//加入CRC校验
static void create_crc(unsigned char *frame, int len , other_param *nak_and_phl)
{
	*(unsigned int *)(frame + len) = crc32(frame, len);
	send_frame(frame, len + 4);
	nak_and_phl->phl_ready = 0;
}

//发送一包
static void send_data_frame(other_param *nak_and_phl, f_kind fk, f__seq frame_nr, f__ack frame_expected, unsigned char buffer[NR_BUFS][PKT_LEN])
{
	struct FARME frame;

	frame.kind = fk;
	frame.seq = frame_nr;
	frame.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);

	if (frame.kind == FRAME_DATA)
	{
		memcpy(frame.data, buffer[frame_nr % NR_BUFS], PKT_LEN);//复制分组到帧内
		dbg_frame((char *)"Send DATA %d %d, ID %d\n", frame.seq, frame.ack, *(short *)frame.data);
		//lprintf("Send DATA %d %d, ID %d\n", frame.seq, frame.ack, *(short *)frame.data);
		create_crc((unsigned char *)&frame, 3 + PKT_LEN , nak_and_phl);   //加入校验和
		start_timer(frame_nr%NR_BUFS, DATA_TIMER);     //启动定时器
	}
	else if(frame.kind == FRAME_ACK)
	{
		dbg_frame((char *)"Send ACK  %d\n", frame.ack);
		create_crc((unsigned char *)&frame, 3 , nak_and_phl);
	}
	else
	{
		nak_and_phl->nak = 1;
		//lprintf("Send NAK!\n");
		create_crc((unsigned char *)&frame, 3, nak_and_phl);
	}
	nak_and_phl->phl_ready = 0;
	stop_ack_timer();
}

void main(int argc, char **argv)
{
	int event, arg;
	struct FARME frame;
	int i=0 ,len = 0;
	int arrived[NR_BUFS];
	other_param nak_and_phl;
	static unsigned char ack_expected = 0, next_frame_to_send = 0;
	static unsigned char frame_expected = 0, too_far = NR_BUFS;
	static unsigned char nbuffered=0;
	static unsigned char out_buf[NR_BUFS][PKT_LEN], in_buf[NR_BUFS][PKT_LEN];


	nak_and_phl.nak = 0;
	nak_and_phl.phl_ready = 0;
	protocol_init(argc, argv);
	lprintf((char *)"Designed by 马天成-王宸-罗帅, build: " __DATE__"  \n");

	for (i = 0; i < NR_BUFS; i++)   //标记接收方的缓冲区是满还是空
	{
		arrived[i] = 0;
	}
	enable_network_layer();

	while (1)
	{
		event = wait_for_event(&arg);
		switch (event)
		{
			case  NETWORK_LAYER_READY:
				nbuffered++;                                       //悬而未决的帧加一
				get_packet(out_buf[next_frame_to_send % NR_BUFS]); //将数据放入发送缓冲区
				send_data_frame(&nak_and_phl ,FRAME_DATA, next_frame_to_send, frame_expected, out_buf);//发送数据帧
				next_frame_to_send = (next_frame_to_send + 1) % (MAX_SEQ + 1);    //下一个要发送的帧编号加一
				break;
			case PHYSICAL_LAYER_READY:
				nak_and_phl.phl_ready = 1;
				break;
			case FRAME_RECEIVED:
				len = recv_frame((unsigned char *)&frame, sizeof frame);
				if (len < 5 || crc32((unsigned char *)&frame, len) != 0)             //CRC校验错误
				{	
					dbg_event((char *)"**** Receiver Error, Bad CRC Checksum\n");
					if (nak_and_phl.nak == 0)                                         //如果nak=0，表示没有发送nak帧则发送
					{
						send_data_frame(&nak_and_phl ,FRAME_NAK, 0, frame_expected, out_buf);//发送nak帧，ack为frame_expected
						lprintf("Send NAK frame\n");
					}

					break;
				}
				if (frame.kind == FRAME_DATA)
				{
					//如果接受序号不在接受窗口
					if (!judge_between(frame_expected, frame.seq, too_far) && nak_and_phl.nak == 0)
					{
						send_data_frame(&nak_and_phl, FRAME_NAK, 0, frame_expected, out_buf);
					}
					else
					{
						start_ack_timer(ACK_TIMER);
					}
					if (judge_between(frame_expected, frame.seq, too_far) == 1 && arrived[frame.seq % NR_BUFS] == 0)
					{
						dbg_frame((char *)"Recv DATA %d %d, ID %d\n", frame.seq, frame.ack, *(short *)frame.data);
						arrived[frame.seq % NR_BUFS] = 1;
						memcpy(in_buf[frame.seq%NR_BUFS], frame.data, len - 4 - 3);
						//把小于当前序列号的帧顺次发送到网络层
						while (arrived[frame_expected%NR_BUFS])
						{
							put_packet(in_buf[frame_expected % NR_BUFS], len - 7);
							nak_and_phl.nak = 0;
							arrived[frame_expected%NR_BUFS] = 0;//清空缓冲区标志位

							frame_expected = (frame_expected + 1) % (MAX_SEQ + 1);
							too_far = (too_far + 1) % (MAX_SEQ + 1);//接收方窗口上限+1
							start_ack_timer(ACK_TIMER);
						}
					}
				}
				if ((frame.kind == FRAME_NAK) && judge_between(ack_expected, (frame.ack + 1) % (MAX_SEQ + 1), next_frame_to_send) == 1)
				{
					//发送方接到nak,重传丢失（错误）的帧
					dbg_frame((char *)"Recv NAK %d\n", frame.ack);
					send_data_frame(&nak_and_phl ,FRAME_DATA, (frame.ack + 1) % (MAX_SEQ + 1), frame_expected, out_buf);
				}
				while (judge_between(ack_expected, frame.ack, next_frame_to_send) == 1)//确认帧已经到达
				{
					nbuffered--;
					stop_timer(ack_expected % NR_BUFS);//关闭帧的定时器
					ack_expected = (ack_expected + 1) % (MAX_SEQ + 1);//窗口上限加一
				}
				break;
			case DATA_TIMEOUT:
				dbg_event((char *)"---- DATA %d timeout\n", arg);
				lprintf((char *)"---- DATA %d timeout\n"  , ack_expected);
				stop_timer(ack_expected%NR_BUFS);//关闭定时器
				send_data_frame(&nak_and_phl ,FRAME_DATA, ack_expected, frame_expected, out_buf);//超时后，重传
				break;
			case ACK_TIMEOUT:
				//无反向数据流，就单独发送ack确认帧
				dbg_event((char *)"---- ACK %d timeout\n", arg);
				lprintf((char *)"---- ACK %d timeout\n", frame_expected);
				stop_ack_timer();
				send_data_frame(&nak_and_phl, FRAME_ACK, 0, frame_expected, out_buf);
		}
		if (nbuffered < NR_BUFS && nak_and_phl.phl_ready)
			enable_network_layer();
		else
			disable_network_layer();
	}
}