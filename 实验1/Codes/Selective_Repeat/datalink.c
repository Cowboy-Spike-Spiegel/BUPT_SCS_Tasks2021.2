#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "datalink.h"

#define DATA_TIMER	3712	// 270*4+263*3+7=1856
#define ACK_TIMER	263
#define MAX_SEQ		15
#define NR_BUFS		((MAX_SEQ+1)/2)
#define inc(k) if (k<MAX_SEQ) k = k + 1; else k = 0;


// Typedef----------------------------------------------------------------------------------------V
/* Use PascalCase for class or typedef */
typedef unsigned char Kind;
typedef unsigned char Seq_nr;
typedef unsigned char Packet[PKT_LEN];
typedef unsigned char bool;

typedef struct FRAME {
	Kind	kind;			/* 1:FRAME_DATA - 2:FRAME_ACK - 3:FRAME_NAK  */
	Seq_nr	ack;			/* piggybacking */
	Seq_nr	seq;			/* frame_to_send */
	Packet	info;			/* in protocol.h: "#define PKT_LEN 256" */
	unsigned int padding;	/* 4bytes: solution of CRC32 */
} Frame;
// -----------------------------------------------------------------------------------------------^



// Global initialize------------------------------------------------------------------------------v
static int phl_ready = 0;	/* physical is not ready */
static int no_nak = 1;		/* no nak has been sent(bool cannot be used) */
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

/* Frame of Data, Ack or NAK */
static void send(Kind fk, Seq_nr frame_nr, Seq_nr frame_expected, Packet buffer[]) {
	Frame s;
	s.kind = fk;
	s.ack = (frame_expected+MAX_SEQ) % (MAX_SEQ+1);// ack is the former one
	
	if (fk == FRAME_DATA) {
		s.seq = frame_nr;
		memcpy(s.info, buffer[frame_nr % NR_BUFS], PKT_LEN);
		dbg_frame("Send DATA %d %d, ID %d\n", s.seq, s.ack, *(short*)s.info);
		put_frame((unsigned char*)&s, 3 + PKT_LEN);	// send with CRC32(4Bytes)
	}
	else if (fk == FRAME_ACK) {
		dbg_frame("Send ACK %d\n", s.ack);
		put_frame((unsigned char*)&s, 2);	// send with CRC32(4Bytes) to ovewrite begin seq
	}
	else if (fk == FRAME_NAK) {
		dbg_frame("Send NAK %d\n", (s.ack+1)%(MAX_SEQ+1));
		put_frame((unsigned char*)&s, 2);	// send with CRC32(4Bytes) to ovewrite begin seq
	}
}
// -----------------------------------------------------------------------------------------------^



// main-------------------------------------------------------------------------------------------V
int main(int argc, char** argv)
{
	/* basic imformation initialize */
	int event, arg;				/* event(1 2 3 4 5); arg: for debug */
	int len = 0;				/* Length of Frame from network layer */
	protocol_init(argc, argv);
	lprintf("selective repeat: ACK_TIMER=%dms, DATA_TIMER=%dms NAK enabled\n", ACK_TIMER, DATA_TIMER);
	lprintf("Designed by MTC\n");


	/* local imformation with initialization */
	Seq_nr ack_expected = 0;		/* As a sender, window's lower bound - '[' */
	Seq_nr next_frame_to_send = 0;	/* As a sender, window's upper bound - ')' */
	Packet bufferSend[NR_BUFS];		/* sender window */

	Seq_nr frame_expected = 0;		/* As a receiver, window's lower bound - '[' */
	Seq_nr too_far = NR_BUFS;		/* As a receiver, window's upper bound - ')' */
	Packet bufferReceive[NR_BUFS];	/* receiver window */

	Seq_nr nbuffered = 0;	/* Buffer is nothing */
	int arrived[NR_BUFS];	/* uses of bufferReceive */
	for (int i = 0; i < NR_BUFS; i++)
		arrived[i] = 0;

	Frame obj;
	while (1) {
		/* buffer is full or physical layer is working */
		if (nbuffered < NR_BUFS && phl_ready)
			enable_network_layer();
		else
			disable_network_layer();

		event = wait_for_event(&arg);
		switch (event) {

			// network is already thus, get packet from network ************************
			case NETWORK_LAYER_READY:
				/* get packet from network layer for sending */
				nbuffered++;				// buffer has added one
				get_packet(bufferSend[next_frame_to_send % NR_BUFS]);
				send(FRAME_DATA, next_frame_to_send, frame_expected, bufferSend);
				start_timer(next_frame_to_send, DATA_TIMER);
				stop_ack_timer();	// ack will be sent

				inc(next_frame_to_send);	// Sliding Window
				break;

			// physical layer waiting data <= 50 bytes *********************************
			case PHYSICAL_LAYER_READY:
				phl_ready = 1;
				break;

			// has received a frame: frame/ack/nak *************************************
			case FRAME_RECEIVED:
				len = recv_frame((unsigned char*)&obj, sizeof(obj));	// get frame to obj ;

				/* Error */
				if (len < 6 || crc32((unsigned char*)&obj, len) != 0) {
					dbg_event("**** Receiver Error, Bad CRC Checksum\n");
					if (no_nak) {
						send(FRAME_NAK, 0, frame_expected, bufferSend);
						no_nak = 0;	// nak reset
					}
					break;
				}

				/* received a data frame */
				if (obj.kind == FRAME_DATA) {
					dbg_frame("Receive DATA %d, ID %d --%dbytes\n", obj.seq, *(short*)obj.info, len);

					/* want the lower frame; if not, send back nak */
					if ((obj.seq != frame_expected) && no_nak) {
						dbg_event("** Recveive frame is not lower bound, NAK sent back\n");
						send(FRAME_NAK, 0, frame_expected, bufferSend);
						no_nak = 0;	// nak reset
					}
					else
						start_ack_timer(ACK_TIMER);

					/* Frame_data can be stored into bufferReceive */
					if (between(frame_expected, obj.seq, too_far) && (arrived[obj.seq%NR_BUFS] == 0)) {
						arrived[obj.seq%NR_BUFS] = 1;		// symbolize in bufferReceive
						memcpy(bufferReceive[obj.seq%NR_BUFS], obj.info, PKT_LEN);

						/* send frames continuously to network_layer */
						while (arrived[frame_expected % NR_BUFS]) {
							dbg_event("Put packet to network layer seq:%d, ID: %d\n", frame_expected, *(short*)(bufferReceive[frame_expected % NR_BUFS]));
							put_packet(bufferReceive[frame_expected%NR_BUFS], PKT_LEN);
							start_ack_timer(ACK_TIMER);				// start waiting for piggyback
							arrived[frame_expected%NR_BUFS] = 0;	// symbolize 0 for having sent
							no_nak = 1;		// this frame has no nak
							
							inc(frame_expected);	// incline receive lower bound
							inc(too_far);			// incline receive upper bound
						}
					}
				}

				/* Nak arrived! Send back the mistake one */
				if (obj.kind == FRAME_NAK && between(ack_expected, (obj.ack+1)%(MAX_SEQ+1), next_frame_to_send)) {
					dbg_frame("Recv NAK  %d --%dbyte\n", (obj.ack+1)%(MAX_SEQ+1), len);
					/* mistake is the ack's latter one, because ack shows lateset correct one; send back it */
					send(FRAME_DATA, (obj.ack+1)%(MAX_SEQ+1), frame_expected, bufferSend);
					start_timer((obj.ack + 1) % (MAX_SEQ + 1), DATA_TIMER);
					stop_ack_timer();	// ack will be sent
				}

				/* Ack received! Delete buffer before obj.ack */
				dbg_frame("Recvive ACK %d --%dbyte\n", obj.ack, len);
				while (between(ack_expected, obj.ack, next_frame_to_send)) {
					nbuffered--;
					stop_timer(ack_expected);	// close timer
					inc(ack_expected);			// slide Ws lower bound - '['
				}
				break;

			// resent all buffer *******************************************************
			case DATA_TIMEOUT:
				dbg_event("---- DATA %d timeout\n", arg);	// arg represents which is timeout
				send(FRAME_DATA, arg, frame_expected, bufferSend);
				start_timer(arg, DATA_TIMER);
				stop_ack_timer();	// ack will be sent
				break;

			// Timeout: no frame to send can piggyback ack *****************************
			case ACK_TIMEOUT:
				send(FRAME_ACK, 0, frame_expected, bufferSend);
				break;
		}
	}
}