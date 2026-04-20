// INVARIANTS OF DATA STRUCTURES
// 1. In the routing table, all neighbouring networks come first
// 2. If active_router is high, then so is active_network
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "stdbool.h"
#include "arpa/inet.h"

#include "config.h"
/*###############################################################################
                                TYPES    
###############################################################################*/
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

/*###############################################################################
                  NUMERIC HANDLING OF ADDRESSES 
###############################################################################*/
void addrToBuffer(uint8_t *buf, uint32_t ip) {
    buf[0] = (ip >> 24) & 0xFF;
    buf[1] = (ip >> 16) & 0xFF;
    buf[2] = (ip >> 8)  & 0xFF;
    buf[3] = ip & 0xFF;
}

uint32_t addrAsNumber(uint8_t addr[4]) {
    return (addr[0] << 24) + (addr[1] << 16) + (addr[2] << 8) + addr[3];
}

uint32_t subnetMask(uint8_t mask) {
    return 0xFFFFFFFF << (32 - mask);
}

uint32_t addrGetBroadcast(NetAddr na) {
    return addrAsNumber(na.addr) | ~subnetMask(na.mask);
}

uint32_t getBroadcast(NetData *nd) {
    return addrGetBroadcast(nd->na);
}

void getNetwork (uint8_t *buf, uint32_t addr, uint8_t mask) {
    uint32_t net_addr = addr & subnetMask(mask);
    addrToBuffer(buf, net_addr);
}

/*###############################################################################
                       NETDATA TO/FROM RAW UDP DATA
###############################################################################*/
NetData* parseDatagram(uint8_t *buffer, uint32_t ip) {

    NetAddr next;
        addrToBuffer(next.addr, ip);

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

/*###############################################################################
                            REACHABILITY
###############################################################################*/
void propagateInfinity (NetData *nd, RoutingTable rt, int turn) {
    for (; rt != RT_EMPTY; rt = rt->next) {
        if (addrAsNumber(rt->nd->next.addr) == addrAsNumber(nd->na.addr)) {
            rt->nd->d = INF;
            rt->nd->last_seen = turn;
        }
    }
}

void markUnreachable (NetData *nd, RoutingTable rt, int turn, bool type) {
    // Propagate infinity only if something new happnens
    // This ensures that last_seen on records to be deleted is not constantly updated
    if (nd->active_router || (nd->active_network && (type == MK_NETWORK))) {
        propagateInfinity(nd, rt, turn);
    }
    if (type == MK_NETWORK) {
        nd->active_network = false;
        nd->d = INF;
    }
    nd->last_seen = turn;
    nd->active_router  = false;
}

void markReachable (NetData *nd, int turn, bool type) {
    if (type == MK_ROUTER) {
        nd->active_router = true; 
        nd->last_seen = turn;
    }
    nd->active_network = true;
    if (nd->direct_d < nd->d)
        nd->d = nd->direct_d;
}

/*###############################################################################
                                PRINTING
###############################################################################*/
// For direct connections, always print the direct connection status
// If an indirect connection is better (or the only one possible),
// also print the indirect connection
void printNetData(NetData *nd) {

    uint8_t net_addr[4];
    uint32_t tmp = addrAsNumber(nd->na.addr);
    getNetwork (net_addr, tmp, nd->na.mask);

    char addr[25];
    sprintf (
        addr, 
        "%u.%u.%u.%u/%u",
        net_addr[0], 
        net_addr[1], 
        net_addr[2], 
        net_addr[3], 
        nd->na.mask
    );

    char dist[25];
    sprintf (
        dist,
        "distance %u",
        nd->d
    );

    char via[25];
    sprintf (via, "via %u.%u.%u.%u\n",
        nd->next.addr[0],
        nd->next.addr[1],
        nd->next.addr[2],
        nd->next.addr[3]
    );
    if (nd->direct) { if(nd->active_network)
            printf (
                "%s distance %u connected directly\n",
                addr,
                nd->direct_d
            );
        else
            printf ("%s unreachable connected directly\n", addr);
        if(nd->direct_d > nd->d || (nd->d < INF && !nd->active_network))
            printf ("%s %s %s", addr, dist, via);
    }
    else if (nd->d >= INF)
        printf ("%s unreachable\n", addr);
    else 
        printf ( "%s %s %s", addr, dist, via);
    // printf("last seen %d\n", nd->last_seen);
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

/*###############################################################################
                    INITIAL ROUTING TABLE SETUP 
###############################################################################*/
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

/*###############################################################################
                    ROUTING TABLE UPDATE AND CLEANUP 
###############################################################################*/
// Iterate through the routing table to find the sender
// return 0 if not found (this will always happen from packets received from oneself!)
NetData* findSender (NetData *dgram, RoutingTable rt, int turn) {
    for (; rt != RT_EMPTY; rt = rt->next)
        if (addrAsNumber(rt->nd->na.addr) == addrAsNumber(dgram->next.addr))
            return rt->nd;
    return (NetData*) 0;
}

// There are generally two cases:
// 1. we find the net already in the table
// 2. the net has to be added to the table
//
// The dgram NetData struct contains the datagram sender address as next,
// the datagram distance as d
void updateDistance(NetData* dgram, RoutingTable rt, int turn) {
    NetData* sender_nd = findSender(dgram, rt, turn);
    if (sender_nd == 0)
        return;
    markReachable(sender_nd, turn, MK_ROUTER);
    for (; rt != RT_EMPTY; rt = rt->next) {
        if (getBroadcast(rt->nd) == getBroadcast(dgram)) {
            // If freshly after disconnect, 
            // do not accept information from neighbours
            if (rt->nd->direct && !rt->nd->active_network
               && turn - rt->nd->last_seen < TURNS_AFTER_INF + 2)
                break;
            // if infinite distance from next on path and the distance is not yet INF,
            // mark as unreachable
            if (dgram->d >= INF && rt->nd->d < INF
               && (addrAsNumber(rt->nd->next.addr) == addrAsNumber(sender_nd->na.addr))) {
                rt->nd->d = INF;
                rt->nd->last_seen = turn;
            }
            // Finally, if there are no infinities,
            // update the distance
            else if (rt->nd->d > sender_nd->direct_d + dgram->d) {
                rt->nd->d = dgram->d + sender_nd->d;
                rt->nd->next = sender_nd->na;
            }
            free(dgram);
            break;
        }
        // If we're at the end of the list, add the new network
        // ONLY IF the distance is not infinite
        else if (rt->next == RT_EMPTY) {
            if (dgram->d < INF) {
                RoutingNode *rn = malloc(sizeof(RoutingNode));
                dgram->d += sender_nd->direct_d;
                rn->nd = dgram;
                rt->next = rn;
                break;
            }    
        }
    }
    return; 
}

void deleteNotSeen(RoutingTable rt, int curr_turn) {
    
    RoutingNode tmp;
    tmp.next = rt;
    RoutingTable it = &tmp;

    while (it->next != RT_EMPTY) {
        RoutingTable next = it->next;
        if (next->nd->direct) {
            if (next->nd->active_network
                && (curr_turn - next->nd->last_seen) > TURNS_BEFORE_INF)
                markUnreachable(next->nd, rt, curr_turn, MK_ROUTER);
            it = it->next; 
        }
        else if (next->nd->d >= INF
                 && (curr_turn - next->nd->last_seen) > TURNS_AFTER_INF) {
            // printf("Deleting non-neighbouring network\n");
            it->next = next->next;
            free(next);
        } 
        else
            it = it->next;
    }
}

/*###############################################################################
                    GETTERS FOR ENCAPSULATION
###############################################################################*/
NetData* getNetData(RoutingTable rt) {
    return rt->nd;
}

RoutingTable getNext(RoutingTable rt) {
    return rt->next;
}

bool getDirect(NetData* nd) {
    return nd->direct;
}

bool shouldSend(NetData *nd, int turn) {
    return !nd->direct || nd->active_network || (turn - nd->last_seen < TURNS_AFTER_INF);
}
