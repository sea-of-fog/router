#include "stdint.h"
#include "stdio.h"
#include "arpa/inet.h"
#include "netinet/ip.h"
#include "unistd.h"
#include "poll.h"
#include "errno.h"
#include "string.h"

#include "config.h"
#include "data.h"

static int sockfd;

void error(char *where) {
    printf("Error in %s: %s", where, strerror(errno));
}

// TODO add error handling
void openSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));

    struct sockaddr_in server_address = {0};
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(54321);
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    bind (
        sockfd,
        (struct sockaddr*)&server_address,
        sizeof(server_address)
    );
}

// TODO: fix error handling; errors are now expected
// TODO: If I receive a packet, will it be marked as reachable? Mark reachable
NetData* receivePacket() {

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    uint8_t buffer[IP_MAXPACKET+1];

    ssize_t packet_len = recvfrom(
        sockfd,
        buffer,
        IP_MAXPACKET,
        MSG_DONTWAIT,
        (struct sockaddr*) &sender,
        &sender_len
    );

    if (packet_len < 0) {
        // error("receivePacket");
        return (NetData*) 0;
    }
    else
       return parseDatagram(buffer, ntohl(sender.sin_addr.s_addr));
}

// TODO: handle errors better
int watch (uint64_t time) {
    struct pollfd ps;
        ps.fd = sockfd;
        ps.events = POLLIN;
        ps.revents = 0;
    int events = poll(&ps, 1, time);
    if (events == 0)
        return 0;
    else if (events > 0 && (ps.revents & POLLIN != 0))
        return 1;
}

// TODO: add error handling
void sendRecord(NetData *nd, NetData *tgt, int turn) {
    uint8_t msg[9];
    NetDataToBuffer(nd, msg);

    struct sockaddr_in address = { 0 };
        address.sin_family = AF_INET;
        address.sin_port   = htons(54321);
        address.sin_addr.s_addr   = htonl(getBroadcast(tgt));

    if (!shouldSend(nd, turn))
        return;

    ssize_t sent = sendto(
        sockfd, 
        msg, 
        9, 
        0, 
        (struct sockaddr*) &address, 
        sizeof(address)
    );

    if (sent != 9) {
        if (PRINT_SEND_ERRORS) error("sendTable");
        markUnreachable(tgt);
    }
    else if (sent == 9)
        markReachableNetwork(tgt);
}

void sendTable(RoutingTable rt, int turn) {
    RoutingTable original = rt;
    for (; rt != RT_EMPTY; rt = getNext(rt)) {
        RoutingTable tgt = original; 
        for (; tgt != RT_EMPTY && getDirect(getNetData(tgt)); tgt = getNext(tgt))
            sendRecord(getNetData(rt), getNetData(tgt), turn);
    }
}

