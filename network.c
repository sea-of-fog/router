// C standard library
#include "stdint.h"
#include "stdio.h"
#include "arpa/inet.h"
#include "netinet/ip.h"
#include "unistd.h"
#include "poll.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"

// My libraries
#include "config.h"
#include "data.h"

static int sockfd;

_Noreturn static void ERROR(const char* str) {
    fprintf(stderr, "%s: %s\n", str, strerror(errno));  // NOLINT(*-err33-c)
    exit(EXIT_FAILURE);
}

void openSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        ERROR("socket()");

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) != 0)
        ERROR("setsockopt() broadcast");
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) != 0)
        ERROR("setsockopt() reuseport");

    struct sockaddr_in server_address = {0};
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(54321);
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int is_bound = bind (
        sockfd,
        (struct sockaddr*)&server_address,
        sizeof(server_address)
    );
    if (is_bound != 0)
        ERROR("bind(): ");
}

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
        return (NetData*) 0;
    }
    else
       return parseDatagram(buffer, ntohl(sender.sin_addr.s_addr));
}

// returns 1 if there are packets to read, 0 if there are none, crashes the program on error
int set_poll(uint64_t time) {
    struct pollfd ps;
        ps.fd = sockfd;
        ps.events = POLLIN;
        ps.revents = 0;
    int events = poll(&ps, 1, time);
    if (events < 0)
        ERROR("poll()");
    else if (events > 0 && ((ps.revents & POLLIN) != 0))
        return 1;
    else return 0;
}

static void sendRecord(NetData *nd, NetData *tgt, RoutingTable rt, int turn) {
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

    if (sent != 9)
        markUnreachable(tgt, rt, turn, MK_NETWORK);
    else if (sent == 9)
        markReachable(tgt, turn, MK_NETWORK);
}

void sendTable(RoutingTable rt, int turn) {
    RoutingTable original = rt;
    for (; rt != RT_EMPTY; rt = getNext(rt)) {
        RoutingTable tgt = original; 
        for (; tgt != RT_EMPTY && getDirect(getNetData(tgt)); tgt = getNext(tgt))
            sendRecord(getNetData(rt), getNetData(tgt), original, turn);
    }
}

