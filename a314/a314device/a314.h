#ifndef DEVICES_A314_H
#define DEVICES_A314_H

#include <exec/io.h>

#define	A314_NAME			"a314.device"

#define A314_CONNECT			(CMD_NONSTD+0)
#define A314_READ			(CMD_NONSTD+1)
#define A314_WRITE			(CMD_NONSTD+2)
#define A314_EOS			(CMD_NONSTD+3)
#define A314_RESET			(CMD_NONSTD+4)

#define A314_CONNECT_OK			0
#define A314_CONNECT_SOCKET_IN_USE	1
#define A314_CONNECT_RESET		2
#define A314_CONNECT_UNKNOWN_SERVICE	3

#define A314_READ_OK			0
#define A314_READ_EOS			1
#define A314_READ_RESET			2

#define A314_WRITE_OK			0
#define A314_WRITE_EOS_SENT		1
#define A314_WRITE_RESET		2

#define A314_EOS_OK			0
#define A314_EOS_EOS_SENT		1
#define A314_EOS_RESET			2

#define A314_RESET_OK			0

#define MEMF_A314			(1<<7)

struct A314_IORequest
{
	struct IORequest a314_Request;
	ULONG		a314_Socket;
	STRPTR		a314_Buffer;
	WORD		a314_Length;
};

#endif
