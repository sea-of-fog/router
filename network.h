#include "data.h"
#include "stdint.h"

#ifndef NETWORK_H
#define NETWORK_H

void openSocket ();
int watch (uint64_t time);
void sendTable (RoutingTable rt, int turn);
NetData* receivePacket ();

#endif
