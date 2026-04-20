#include "stdint.h"
#include "stdbool.h"

#ifndef DATA_H
#define DATA_H

typedef struct net_data NetData;
typedef struct routing_node* RoutingTable;

extern const RoutingTable RT_EMPTY;

NetData* scanNeighbour();
RoutingTable addNeighbour(NetData *nd, RoutingTable rt);
NetData* parseDatagram(uint8_t *buffer, uint32_t ip);
void NetDataToBuffer(NetData *nd, uint8_t *buffer);
void printRoutingTable (RoutingTable rt, unsigned int turn);
void printNetData (NetData* nd);
void updateDistance (NetData* nd, RoutingTable rt, int turn);
void deleteNotSeen (RoutingTable rt, int turn);

NetData* getNetData (RoutingTable rt);
uint32_t getBroadcast (NetData* nd);
RoutingTable getNext (RoutingTable rt);
bool getDirect(NetData *nd);
bool shouldSend(NetData *nd, int turn);

void markUnreachable(NetData* nd);
void markReachableNetwork(NetData* nd);
void markReachableRouter(NetData *nd);

#endif 
