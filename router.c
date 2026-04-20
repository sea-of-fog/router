// C standard library
#include "stdio.h"
#include "stdint.h"
#include "time.h"

// My libraries
#include "config.h"
#include "data.h"
#include "network.h"

uint64_t timedelta (struct timespec t1, struct timespec t2) {
    return (t1.tv_sec - t2.tv_sec)*1000 + (t1.tv_nsec - t2.tv_nsec)/1000000;
}

uint64_t nextTurn (struct timespec curr, struct timespec last) {
    return (timedelta(curr,last) < TURN_MS) ? TURN_MS - timedelta(curr,last) : 0;
}

int main(int argc, char **argv) {

    int n;
    RoutingTable routing = RT_EMPTY;

    scanf("%d", &n);
    while (n--) {
        NetData* nd = scanNeighbour();
        routing = addNeighbour(nd, routing); 
    }
    printRoutingTable(routing, 0);

    openSocket();
    sendTable(routing);

    struct timespec last_sent, curr_time;
    clock_gettime(CLOCK_REALTIME, &last_sent);

    unsigned int turn = 1;
    while (true) {

        clock_gettime(CLOCK_REALTIME, &curr_time);
        int packets = watch(nextTurn(curr_time, last_sent));

        if (packets > 0) {
            NetData *nd;
            while (nd = receivePacket() != 0) 
                updateDistance(nd, routing);
        }
        else if (packets == 0) {
            sendTable(routing);
            printRoutingTable(routing, turn);
            deleteNotSeen(routing, turn);
            clock_gettime(CLOCK_REALTIME, &last_sent);
            turn++;
        }
        else if (packets < 0) {
            printf("Poll error: %d", packets);
            return packets;
        }

    }

    return 0;
}
