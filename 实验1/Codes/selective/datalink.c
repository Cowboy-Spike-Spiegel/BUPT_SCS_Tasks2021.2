#include <stdio.h>
#include <string.h>

#include "protocol.h"

#define MAX_SEQ 15
#define NR_BUFS ((MAX_SEQ+1)/2)

#define DATA_TIMER  3000       //帧超时时间间隔
#define ACK_TIMER 240          //ack的超时间隔
//帧类型
#define FRAME_DATA 1
#define FRAME_ACK  2
#define FRAME_NAK  3

typedef unsigned char f_kind;
typedef unsigned char f__ack;
typedef unsigned char f__seq;

struct FRAME { 
	f_kind kind;
	f__ack ack;
	f__seq seq;
    unsigned char data[PKT_LEN]; 
    unsigned int  padding;
};

int no_nak=1;                        //标志是否已经发送过nak
int nak = 0;
static int phl_ready = 0;
unsigned char oldest_frame=MAX_SEQ+1;

//判断帧号（ack号 b）是否在窗口内
static int between(unsigned char a,unsigned char b,unsigned char c)  
{
   if(((a<=b)&&(b<c))||((c<a)&&(a<=b))||((b<c)&&(c<a)))
		return 1;
	else
		return 0;

}

static void put_frame(unsigned char *frame, int len)//加入校验和crc
{
    *(unsigned int *)(frame + len) = crc32(frame, len);
    send_frame(frame, len + 4);
    phl_ready = 0;
}

static void send_data_frame(f_kind fk,f__seq frame_nr,f__ack frame_expected,unsigned char buffer[NR_BUFS][PKT_LEN])
{//发送数据帧，或ack，或nak
    struct FRAME s;
    
    s.kind = fk;
    s.seq = frame_nr;
    s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);

	if(fk==FRAME_DATA)
	{
		memcpy(s.data, buffer[frame_nr % NR_BUFS], PKT_LEN);//复制分组到帧内
		dbg_frame("Send DATA %d %d, ID %d\n", s.seq, s.ack, *(short *)s.data);
        put_frame((unsigned char *)&s, 3 + PKT_LEN);   //加入校验和
		start_timer(frame_nr%NR_BUFS, DATA_TIMER);     //启动定时器
	}
	else if(fk == FRAME_NAK)
	{
		nak = 1;
	    put_frame((unsigned char *)&s, 3);            
	}
	else if(fk == FRAME_ACK)
	{
		dbg_frame("Send ACK  %d\n", s.ack);
        put_frame((unsigned char *)&s, 3);
	}
	phl_ready = 0;
	stop_ack_timer();                          //没有必要启动ack定时器
}

/*
void main(int argc, char **argv)
{
	int event, arg;
    struct FRAME f;
    int len = 0;
    int i;
	static unsigned char ack_expected=0, next_frame_to_send=0;
	static unsigned char frame_expected=0, too_far=NR_BUFS;
    static unsigned char nbuffered;
	int arrived[NR_BUFS];
	static unsigned char out_buf[NR_BUFS][PKT_LEN], in_buf[NR_BUFS][PKT_LEN];

    protocol_init(argc, argv); 
    lprintf("Designed by XXHH(2020211409-罗帅), build: " __DATE__"  "__TIME__"\n");

	for (i = 0; i < NR_BUFS; i++)   //标记接收方的缓冲区是满还是空
	{
		arrived[i] = 0;
	}
    enable_network_layer();

	while(1)
	{
		event = wait_for_event(&arg);
		switch (event)
		{
			case NETWORK_LAYER_READY:
				nbuffered++;                                                          //悬而未决帧数目++
				get_packet(out_buf[next_frame_to_send % NR_BUFS]);                    //得到分组存入缓冲区内
				send_data_frame(FRAME_DATA,next_frame_to_send,frame_expected,out_buf);//发送数据帧
				next_frame_to_send=(next_frame_to_send + 1) % ( MAX_SEQ + 1);         //序号进行模运算加法
				break;

			case PHYSICAL_LAYER_READY:
				phl_ready = 1;
				break;

			case FRAME_RECEIVED:
				len = recv_frame((unsigned char *)&f, sizeof f);
                if (len < 5 || crc32((unsigned char *)&f, len) != 0)
				{	//校验和出错，就发送nak请求重传
					dbg_event("**** Receiver Error, Bad CRC Checksum\n");
					if (nak == 0)                                              //如果nak=0，表示没有发送nak帧则发送
					{
						send_data_frame(FRAME_NAK, 0, frame_expected, out_buf);//发送nak帧，ack为frame_expected
						dbg_event("Send NAK frame\n");
					}
				   	    
                    break;
				}
				if (f.kind == FRAME_DATA)
			    {
					if ((f.seq != frame_expected) && nak == 0)                 //如果序列号错误，并且没有发送nak帧，则发送
					{
						send_data_frame(FRAME_NAK, 0, frame_expected, out_buf);
					}
					else
					{
						start_ack_timer(ACK_TIMER);
					}
					//如果序列号在接受窗口中并且，缓冲区为0，则将数据放入缓冲区
					if(between(frame_expected, f.seq, too_far)==1&&arrived[f.seq % NR_BUFS] == 0)
					{
						dbg_frame("Recv DATA %d %d, ID %d\n", f.seq, f.ack, *(short *)f.data);
						arrived[f.seq % NR_BUFS] = 1;
						memcpy(in_buf[f.seq%NR_BUFS], f.data, len-4-3);
                        //把小于当前序列号的帧顺次发送到网络层
						while(arrived[frame_expected%NR_BUFS])
						{
							put_packet(in_buf[frame_expected % NR_BUFS], len-7);
							nak=0;
							arrived[frame_expected%NR_BUFS]=0;//清空缓冲区标志位

							frame_expected=(frame_expected+1) % (MAX_SEQ + 1);
							too_far=(too_far + 1)% (MAX_SEQ + 1);//接收方窗口上限+1
							start_ack_timer(ACK_TIMER);
						}
					}
			    }
				if((f.kind==FRAME_NAK)&&between(ack_expected,(f.ack+1) % (MAX_SEQ + 1), next_frame_to_send)==1)
				{
					//发送方接到nak,重传丢失（错误）的帧
					dbg_frame("Recv NAK %d\n", f.ack);
					send_data_frame(FRAME_DATA,(f.ack + 1)%(MAX_SEQ+1), frame_expected, out_buf);
				}
				while(between(ack_expected,f.ack,next_frame_to_send)==1)//确认帧已经到达
				{
					nbuffered--;
					stop_timer(ack_expected % NR_BUFS);//关闭帧的定时器
					ack_expected=(ack_expected + 1) % (MAX_SEQ + 1);//窗口上限加一
				}
				break;

			case DATA_TIMEOUT:
                dbg_event("---- DATA %d timeout\n", arg); 
                send_data_frame(FRAME_DATA,ack_expected, frame_expected, out_buf);//超时后，重传
                break;
			case ACK_TIMEOUT:
				//无反向数据流，就单独发送ack确认帧
				dbg_event("---- ACK %d timeout\n", arg);
				send_data_frame(FRAME_ACK, 0, frame_expected, out_buf);
		}

        if (nbuffered < NR_BUFS && phl_ready)
            enable_network_layer();
        else
            disable_network_layer();
	}
}*/
