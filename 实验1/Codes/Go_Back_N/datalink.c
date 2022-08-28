#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "datalink.h"

#define DATA_TIMER	2000	// 7*263
#define ACK_TIMER	1100	// 7*263 -2*270 -263
#define MAX_SEQ		7
#define inc(k) if (k<MAX_SEQ) k = k + 1; else k = 0;


// Typedef----------------------------------------------------------------------------------------V
/* Use PascalCase for class or typedef */
typedef unsigned char Kind;
typedef unsigned char Seq_nr;
typedef unsigned char Packet[PKT_LEN];

typedef struct FRAME {
	Kind	kind;			/* 1:FRAME_DATA - 2:FRAME_ACK - 3:FRAME_NAK  */
	Seq_nr	seq_or_ack;		/* frame_to_send or piggybacking */
	Packet	info;			/* in protocol.h: "#define PKT_LEN 256" */
	unsigned int padding;	/* 4bytes: solution of CRC32 */
} Frame;
// -----------------------------------------------------------------------------------------------^



// Global initialize------------------------------------------------------------------------------v
Packet buffer[MAX_SEQ + 1];				// buffer of sender window
static int phl_ready = 0;				/* physical is not ready */
// -----------------------------------------------------------------------------------------------^



// Functions--------------------------------------------------------------------------------------V

/* Judge b is between a and c: [a,c) */
static unsigned char between(Seq_nr a, Seq_nr b, Seq_nr c) {
	return ((a <= b && b < c) || (c < a && b < c) || (c < a && a <= b));
}

/* Generate and Send Frame to physical */
static void put_frame(unsigned char* frame, int len)
{
	*(unsigned int*)(frame + len) = crc32(frame, len);	// add CRC32(4Bytes)
	send_frame(frame, len + 4);

	/* IMPORTANT: close physical layer until PHYSICAL_LAYER_READY coming! */
	phl_ready = 0;
}

/* Only Frame and Ack */
static void send_Data(Seq_nr frame_nr, Packet buffer[]) {
	Frame s;
	s.kind = FRAME_DATA;
	s.seq_or_ack = frame_nr;	// generate seq
	memcpy(s.info, buffer[frame_nr], PKT_LEN);	// generate frame.info from buffer

	dbg_frame("Send DATA %d, ID %d\n", s.seq_or_ack, *(short*)s.info);
	put_frame((unsigned char*)&s, 2 + PKT_LEN);	// send with CRC32(4Bytes)
}
static void send_Ack(Seq_nr frame_expected) {
	Frame s;
	s.kind = FRAME_ACK;
	s.seq_or_ack = frame_expected;	// generate ack

	dbg_frame("Send ACK  %d\n", s.seq_or_ack);
	/* IMPORTANT: CRC32(4Bytes) added behind Frame.ack to OVERWRITE! */
	put_frame((unsigned char*)&s, 2);
}
// -----------------------------------------------------------------------------------------------^


// main-------------------------------------------------------------------------------------------V
int main(int argc, char** argv)
{
	/* basic imformation initialize */
	int event, arg;				/* event(1 2 3 4 5); arg: for debug */
	int len = 0;				/* Length of Frame from network layer */
	protocol_init(argc, argv);
	lprintf(" Go-back-n: ACK_TIMER=%dms, DATA_TIMER=%dms NAK disabled\n", ACK_TIMER, DATA_TIMER);
	lprintf("Designed by MTC\n");

	/* local imformation with initialization */
	Seq_nr next_frame_to_send = 0;	/* As a sender, window's upper bound - ')' */
	Seq_nr ack_expected = 0;		/* As a sender, window's lower bound - '[' */
	Seq_nr frame_expected = 0;		/* As a receiver, seq should reply */
	Seq_nr nbuffered = 0;			/* Buffer is nothing */
	Frame obj;

	/* copy from guider's code:  */
	disable_network_layer();			

	while (1) {
		dbg_frame("nbuffered: %d\n", nbuffered);
		/* buffer is full or physical layer is working */
		if (nbuffered < MAX_SEQ && phl_ready)
			enable_network_layer();
		else
			disable_network_layer();
		
		event = wait_for_event(&arg);
		switch (event) {

			/* network is already thus, get packet from network */
			case NETWORK_LAYER_READY:
				/* get packet from network layer for sending */
				get_packet(buffer[next_frame_to_send]);
				send_Data(next_frame_to_send, buffer);
				start_timer(next_frame_to_send, DATA_TIMER);	// start timer for this Frame
				nbuffered++;				// buffer has added one
				inc(next_frame_to_send);	// Sliding Window
				break;

			/* physical layer waiting data <= 50 bytes */
			case PHYSICAL_LAYER_READY:
				phl_ready = 1;
				break;

			/* has received a frame: frame/ack (no nak) */
			case FRAME_RECEIVED:
				len = recv_frame((unsigned char*)&obj, sizeof(obj));	// get frame to obj ;
				/* Error: ignore */
				if (len < 6 || crc32((unsigned char*)&obj, len) != 0) {
					dbg_event("**** Receiver Error, Bad CRC Checksum\n");
					break;
				}
				/* received a frame and send back an ack */
				if (obj.kind == FRAME_DATA) {
					dbg_frame("Receive DATA %d, ID %d --%dbytes\n", obj.seq_or_ack, *(short*)obj.info, len);
					if (obj.seq_or_ack == frame_expected) {
						dbg_frame("Receive Frame %d and send back an ack.\n", obj.seq_or_ack);
						put_packet((unsigned char*)obj.info, len-6);	// send to network layer
						send_Ack(frame_expected);
						inc(frame_expected);			// Sliding Window
					}
					break;
				}
				/* Ack received! Delete buffer before obj.ack */
				if (obj.kind == FRAME_ACK) {
					dbg_frame("Recvive ACK %d --%dbyte\n", obj.seq_or_ack, len);
					while (between(ack_expected, obj.seq_or_ack, next_frame_to_send)) {
						nbuffered--;
						stop_timer(ack_expected);	// close timer
						inc(ack_expected);			// slide Ws lower bound - '['
					}
				}
				break;

			/* resent all buffer */
			case DATA_TIMEOUT:
				dbg_event("---- DATA %d timeout\n", arg);
				next_frame_to_send = ack_expected;	/* start from ack_expected */
				for (int i = 0; i < nbuffered; i++) {
					dbg_frame("timeout: resent %d\n", next_frame_to_send);
					send_Data(next_frame_to_send, buffer);
					start_timer(next_frame_to_send, DATA_TIMER);	// start timer for this Frame
					inc(next_frame_to_send);
				}
				break;
		}
	}
}