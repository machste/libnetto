#ifndef _NETTO_NLMSG_H_
#define _NETTO_NLMSG_H_

#include <stdint.h>
#include <masc/object.h>


typedef struct {
	Object;
	uint8_t *data;
	size_t size;
} NlMsg;


extern const class *NlMsgCls;


void nlmsg_init(NlMsg *self, int bus);

void nlmsg_destroy(NlMsg *self);


#endif /* _NETTO_NLMSG_H_ */
