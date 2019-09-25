#ifndef _NETTO_IPADDR_H_
#define _NETTO_IPADDR_H_

#include <stdint.h>
#include <masc/map.h>

#include <netto/nlsocket.h>


Map *ipaddr_get(NlSocket *sock, uint8_t family);


#endif /* _NETTO_IPADDR_H_ */
