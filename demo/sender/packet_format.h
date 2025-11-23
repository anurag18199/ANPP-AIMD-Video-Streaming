#ifndef PACKET_FORMAT_H
#define PACKET_FORMAT_H

#include <sys/time.h>

#pragma pack(push, 1)
struct Packet {
    int seq;
    double send_time;
    char data[1024];
};
#pragma pack(pop)

#endif
