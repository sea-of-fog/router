// ASSUMPTIONS ABOUT DATA STRUCTURES
// 1. In the routing table, all neighbouring networks come first
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "stdbool.h"
#include "arpa/inet.h"

#include "config.h"

typedef struct net_addr {
    uint8_t addr[4];
    uint8_t mask;
} NetAddr;

typedef struct net_data {
    NetAddr  na;
    NetAddr  next;
    bool direct;
    bool active_network;
    bool active_router;
    uint32_t d;
    uint32_t direct_d;
    int  last_seen;
} NetData;

typedef struct routing_node {
    NetData* nd;
    struct routing_node* next;
} RoutingNode;
typedef RoutingNode* RoutingTable;

const RoutingTable RT_EMPTY = (RoutingTable) NULL;

uint32_t addrAsNumber(uint8_t addr[4]) {
    return (addr[0] << 24) + (addr[1] << 16) + (addr[2] << 8) + addr[3];
}

void getNetAddr(uint8_t addr[4], uint8_t mask) {
    return;
}

uint32_t addrGetBroadcast (NetAddr na) {
    return ( (na.addr[0]<<24)
           + (na.addr[1]<<16)
           + (na.addr[2]<<8) 
           + (na.addr[3]) )
           | ((1 << (32 - na.mask)) - 1);
}

// TODO: check if last_seen and current turn are close enough
bool shouldSend(NetData *nd, int turn) {
    return true;
}

void propagateInfinity (NetData *nd, RoutingTable rt, int turn) {
    for (; rt != RT_EMPTY; rt = rt->next) {
        if (addrAsNumber(rt->nd->next.addr) == addrAsNumber(nd->na.addr)) {
            rt->nd->d = INF;
            rt->nd->last_seen = turn;
        }
    }
}

// type = true means unreachable because of network
void markUnreachable (NetData *nd, RoutingTable rt, int turn, bool type) {
    if (nd->active_router) {
        nd->last_seen = turn;
        propagateInfinity(nd, rt, turn);
    }
    if (type)
        nd->active_network = false;
    nd->active_router  = false;
    nd->d = INF;
}

void markReachableRouter (NetData *nd, int turn) {
    nd->active_router = true; 
    nd->active_network = true;
    nd->last_seen = turn;
    if (nd->direct_d < nd->d)
        nd->d = nd->direct_d;
}

void markReachableNetwork (NetData *nd) {
    nd->active_network = true;
}

uint32_t getBroadcast (NetData *nd) {
    return addrGetBroadcast(nd->na);
}

// For direct connections, always print the direct connection status
// If an indirect connection is better (or the only one possible),
// also print the indirect connection
// TODO: refactor with sprintf() repeat less
void printNetData(NetData *nd) {
    char addr[25];
    sprintf (
        addr, 
        "%u.%u.%u.%u/%u",
        nd->na.addr[0], 
        nd->na.addr[1], 
        nd->na.addr[2], 
        nd->na.addr[3], 
        nd->na.mask
    );

    char dist[25];
    sprintf (
        dist,
        "distance %u",
        nd->d
    );

    char via[25];
    sprintf (via, "via %u.%u.%u.%u.\n",
        nd->next.addr[0],
        nd->next.addr[1],
        nd->next.addr[2],
        nd->next.addr[3]
    );
    if (nd->direct) {
        if(nd->active_router)
            printf (
                "%s distance %u connected directly\n",
                addr,
                nd->direct_d
            );
        else
            printf ("%s unreachable connected directly\n", addr);
        if(nd->direct_d > nd->d || (nd->d < INF && !nd->active_router))
            printf ("%s %s %s", addr, dist, via);
    }
    else if (nd->d >= INF)
        printf ("%s unreachable\n", addr);
    else 
        printf ( "%s %s %s", addr, dist, via);
    printf("last seen %d\n", nd->last_seen);
}

void printRoutingTable (RoutingTable rt, unsigned int turn) {
    printf("######################################\n");
    printf("#              TURN %d                \n", turn);
    printf("######################################\n");
    while (rt != RT_EMPTY) {
        printNetData(rt->nd);
        rt = rt->next;
    }
}

RoutingTable addNeighbour(NetData *nd, RoutingTable rt) {
    RoutingTable original = rt;
    RoutingNode *rn = malloc(sizeof(RoutingNode));
        rn->nd = nd;
        rn->next = RT_EMPTY;
    if (rt == RT_EMPTY)
        return rn;
    while (rt->next != RT_EMPTY)
        rt = rt->next;
    rt->next = rn;
    return original;  
}

NetData* scanNeighbour() {
    uint32_t d;
    NetAddr na;
    scanf ("%hhu.%hhu.%hhu.%hhu/%hhu distance %u",
        &na.addr[0], 
        &na.addr[1], 
        &na.addr[2], 
        &na.addr[3], 
        &na.mask, 
        &d
    ); 
    NetData* nd = (NetData*) malloc(sizeof(NetData));
        nd->na        = na;
        nd->direct    = true;
        nd->active_network = true;
        nd->active_router  = true;
        nd->last_seen = 0;
        nd->d         = d;
        nd->direct_d  = d;
    return nd;
}

// Iterate through the routing table to find the sender
// return 0 if not found (this will always happen from packets received from oneself!)
NetData* findSender (NetData *dgram, RoutingTable rt, int turn) {
    for (; rt != RT_EMPTY; rt = rt->next) {
        if (addrAsNumber(rt->nd->na.addr) == addrAsNumber(dgram->next.addr)) {
            rt->nd->last_seen = turn;
            return rt->nd;
        }
    }
    return (NetData*) 0;
}

// There are generally two cases:
// 1. we find the net already in the table
// 2. the net has to be added to the table
//
// The dgram NetData struct contains the datagram sender address as next,
// the datagram distance as d
void updateDistance (NetData* dgram, RoutingTable rt, int turn) {
    NetData* sender_nd = findSender(dgram, rt, turn);
    if (sender_nd == 0)
        return;
    markReachableRouter(sender_nd, turn);
    for (; rt != RT_EMPTY; rt = rt->next) {
        if (getBroadcast(rt->nd) == getBroadcast(dgram)) {
            if (rt->nd->direct && !rt->nd->active_router
               && turn - rt->nd->last_seen < TURNS_AFTER_INF)
                continue;
            if (dgram->d >= INF 
               && (addrAsNumber(rt->nd->next.addr) == addrAsNumber(sender_nd->na.addr))) {
                rt->nd->d = INF;
                rt->nd->last_seen = turn;
            }
            else if (rt->nd->d > sender_nd->direct_d + dgram->d) {
                rt->nd->d = dgram->d + sender_nd->d;
                rt->nd->next = sender_nd->na;
            }
            free(dgram);
            break;
        }
        else if (rt->next == RT_EMPTY) {
            RoutingNode *rn = malloc(sizeof(RoutingNode));
            dgram->d += sender_nd->direct_d;
            rn->nd = dgram;
            rt->next = rn;
            break;
        }
    }
    return; 
}

// TODO: update last_seen to be able to check whether to send a packet
void deleteNotSeen (RoutingTable rt, int curr_turn) {
    
    RoutingNode tmp;
    tmp.next = rt;
    RoutingTable it = &tmp;

    for (; it->next != RT_EMPTY; it = it->next) {
        RoutingTable next = it->next;
        if (next->nd->direct) {
            if (next->nd->active_network
                && (curr_turn - next->nd->last_seen) > TURNS_BEFORE_INF)
                markUnreachable(next->nd, rt, curr_turn, false);
        }
        else if (next->nd->d >= INF
                && (curr_turn - next->nd->last_seen) > TURNS_AFTER_INF) {
            it->next = next->next;
            free(next);
        }
    }

}

NetData* parseDatagram(uint8_t *buffer, uint32_t ip ) {
    NetAddr next;
        next.addr[0] = (ip >> 24) & 0xFF;
        next.addr[1] = (ip >> 16) & 0xFF;
        next.addr[2] = (ip >> 8)  & 0xFF;
        next.addr[3] = ip & 0xFF;

    NetAddr na;
        for (int i = 0; i < 4; i++)
            na.addr[i] = buffer[i];
        na.mask = buffer[4];

    NetData *nd = (NetData*) malloc(sizeof(NetData));
        nd->next = next;
        nd->na = na;
        nd->direct = false;
        nd->d = 0;
        for (int i = 0; i < 4; i++) 
            nd->d += buffer[5 + i] << ((3-i) * 8);

    return nd;
}

void NetDataToBuffer (NetData* nd, uint8_t *buffer) {
    for (int i = 0; i < 4; i++)
        buffer[i] = nd->na.addr[i];
    buffer[4] = nd->na.mask;
    uint32_t temp = htonl(nd->d);
    for (int i = 0; i < 4; i++) 
        buffer[5 + i] = (temp & (0xFF << (8*i))) >> (8*i);
}

NetData* getNetData (RoutingTable rt) {
    return rt->nd;
}

RoutingTable getNext (RoutingTable rt) {
    return rt->next;
}

bool getDirect(NetData* nd) {
    return nd->direct;
}
