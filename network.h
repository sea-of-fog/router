// Krzysztof Szymański nr ind 338068
#include "data.h"
#include "stdint.h"

#ifndef NETWORK_H
#define NETWORK_H

void openSocket();
void sendTable(RoutingTable rt, int turn);
int set_poll(uint64_t time);
NetData* receivePacket();
#endif
