#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

struct Packet {
    int seq;
    double send_time;
    char data[1024];
};

double now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

void send_packets() {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8080);

    struct Packet pkt;
    int seq = 0;

    while (1) {
        pkt.seq = seq++;
        pkt.send_time = now();
        memset(pkt.data, 'A', sizeof(pkt.data));

        sendto(sockfd, &pkt, sizeof(pkt), 0,
               (struct sockaddr*)&addr, len);
        usleep(10 * 1000); // 10ms between packets
    }
}
