#ifndef BACNET_H
#define BACNET_H

#include <stdint.h>
#include "lwip/sockets.h"

// BACnet functions
void bacnet_send_i_am(struct sockaddr_in *dest_addr);
void bacnet_send_broadcast(void);

#endif // BACNET_H
