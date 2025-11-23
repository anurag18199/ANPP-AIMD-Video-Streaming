#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include "packet_format.h"

#define RECV_PORT 8080
#define VIDEO_FORWARD_PORT 10000   // forward to this port (ffplay should listen here)
#define VIDEO_PAYLOAD_SIZE sizeof(((struct Packet *)0)->data) // 1024

static int window_first_seq = -1;
static int window_last_seq = -1;
static int window_received = 0;
static int window_lost = 0;
static double window_total_delay = 0.0;

double now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

extern pthread_mutex_t fb_mutex;

void reset_window() {
    window_first_seq = -1;
    window_last_seq = -1;
    window_received = 0;
    window_lost = 0;
    window_total_delay = 0.0;
}

void *packet_receiver_thread(void *arg) {
    double *fb = (double *)arg;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(RECV_PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    printf("Receiver is listening on port %d\n", RECV_PORT);

    // --- Create video forwarder socket ONCE (not inside loop) ---
    int vid_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (vid_fd < 0) {
        perror("video forward socket");
        exit(1);
    }
    struct sockaddr_in vid_dest;
    memset(&vid_dest, 0, sizeof(vid_dest));
    vid_dest.sin_family = AF_INET;
    vid_dest.sin_port = htons(VIDEO_FORWARD_PORT);
    vid_dest.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct Packet pkt;
    reset_window();
    double start_time = now();

    while (1) {
        int n = recvfrom(sockfd, &pkt, sizeof(pkt), 0,
                         (struct sockaddr*)&cliaddr, &len);

        if (n != sizeof(pkt)) {
            // ignore partial/corrupted packets
            continue;
        }

        // Debug
        printf("RECV seq=%d pkt.send_time=%.6f\n", pkt.seq, pkt.send_time);
        fflush(stdout);

        // --- Forward RAW H264 payload to ffplay on a DIFFERENT port ---
        ssize_t sent = sendto(vid_fd,
                              pkt.data,
                              VIDEO_PAYLOAD_SIZE,   // or use actual valid byte count if you track it
                              0,
                              (struct sockaddr*)&vid_dest,
                              sizeof(vid_dest));
        if (sent < 0) {
            perror("video forward sendto");
        }

        double recv_time = now();
        double delay = recv_time - pkt.send_time;
        if (delay < 0) delay = 0;

        if (window_first_seq == -1) {
            window_first_seq = pkt.seq;
            window_last_seq = pkt.seq;
            window_received = 1;
            window_total_delay = delay;
        } else {
            if (pkt.seq > window_last_seq + 1) {
                window_lost += (pkt.seq - (window_last_seq + 1));
            }
            window_last_seq = pkt.seq;
            window_received++;
            window_total_delay += delay;
        }

        if (recv_time - start_time >= 0.2) {
            int expected = window_last_seq - window_first_seq + 1;
            if (expected < window_received + window_lost)
                expected = window_received + window_lost;

            double loss_rate = (expected > 0) ?
                               ((double)window_lost / expected) : 0.0;

            double avg_delay = (window_received > 0) ?
                               (window_total_delay / window_received) : 0.0;

            pthread_mutex_lock(&fb_mutex);
            fb[0] = loss_rate;
            fb[1] = avg_delay;
            pthread_mutex_unlock(&fb_mutex);

            printf("[WIN] seq %d..%d exp=%d rec=%d lost=%d delay=%.6f loss=%.6f\n",
                   window_first_seq, window_last_seq,
                   expected, window_received,
                   window_lost, avg_delay, loss_rate);

            reset_window();
            start_time = now();
        }
    }

    // cleanup (never reached in normal run)
    close(vid_fd);
    close(sockfd);
    return NULL;
}
