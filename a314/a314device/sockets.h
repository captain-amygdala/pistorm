/*
 * Copyright 2020-2021 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/lists.h>

// Used to store received data until application asks for it using a A314_READ.
struct QueuedData
{
	struct QueuedData *next;
	UWORD length;
	UBYTE data[];
};

// Socket flags, these are bit masks, can have many of them.
#define SOCKET_RCVD_EOS_FROM_APP	0x0004
#define SOCKET_RCVD_EOS_FROM_RPI	0x0008
#define SOCKET_SENT_EOS_TO_APP		0x0010
#define SOCKET_SENT_EOS_TO_RPI		0x0020
#define SOCKET_CLOSED			0x0040
#define SOCKET_SHOULD_SEND_RESET	0x0080
#define SOCKET_IN_SEND_QUEUE		0x0100

struct Socket
{
	struct MinNode node;

	void *sig_task;
	ULONG socket;

	UBYTE stream_id;
	UBYTE pad1;

	UWORD flags;

	struct A314_IORequest *pending_connect;
	struct A314_IORequest *pending_read;
	struct A314_IORequest *pending_write;

	struct Socket *next_in_send_queue;
	UWORD send_queue_required_length;

	// Data that is received on the stream, but the application didn't read yet.
	struct QueuedData *rq_head;
	struct QueuedData *rq_tail;
};

extern struct Socket *send_queue_head;
extern struct Socket *send_queue_tail;

extern void init_sockets();

extern struct Socket *create_socket(struct Task *task, ULONG id);
extern void delete_socket(struct Socket *s);

extern struct Socket *find_socket(void *sig_task, ULONG socket);
extern struct Socket *find_socket_by_stream_id(UBYTE stream_id);

extern void add_to_send_queue(struct Socket *s, UWORD required_length);
extern void remove_from_send_queue(struct Socket *s);
