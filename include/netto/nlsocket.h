#ifndef _NETTO_NLSOCKET_H_
#define _NETTO_NLSOCKET_H_

#include <linux/netlink.h>
#include <masc/io.h>


#define NLSOCKET_PORT_AUTO 0
#define NLSOCKET_GROUP_NONE 0


typedef struct {
	Io;
	struct sockaddr_nl addr;
} NlSocket;


extern const io_class *NlSocketCls;


void nlsocket_init(NlSocket *self, int bus);

void nlsocket_destroy(NlSocket *self);

bool nlsocket_connect(NlSocket *self, int port, int groups);

ssize_t nlsocket_read(NlSocket *self, void *data, size_t size);

ssize_t nlsocket_write(NlSocket *self, const void *data, size_t size);


#endif /* _NETTO_NLSOCKET_H_ */
