#include "stdint.h"
#include "stdbool.h"

#ifndef DATA_H
#define DATA_H

typedef struct net_data NetData;
typedef struct routing_node* RoutingTable;

extern const RoutingTable RT_EMPTY;

uint32_t getBroadcast (NetData* nd);

NetData* parseDatagram(uint8_t *buffer, uint32_t ip);
void NetDataToBuffer(NetData *nd, uint8_t *buffer);

void markUnreachable(NetData* nd, RoutingTable rt, int turn, bool type);
void markReachable(NetData* nd, int turn, bool type);

void printNetData (NetData* nd);
void printRoutingTable (RoutingTable rt, unsigned int turn);

RoutingTable addNeighbour(NetData *nd, RoutingTable rt);
NetData* scanNeighbour();

void updateDistance (NetData* nd, RoutingTable rt, int turn);
void deleteNotSeen (RoutingTable rt, int turn);

NetData* getNetData (RoutingTable rt);
RoutingTable getNext (RoutingTable rt);
bool getDirect(NetData *nd);
bool shouldSend(NetData *nd, int turn);


#endif 
