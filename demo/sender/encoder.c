// encoder.c
// Reads encoded H.264 from ffmpeg stdout, packetizes into struct Packet and sends via UDP.
// Assumes packet_format.h uses packed struct (1036 bytes total).
#define _POSIX_C_SOURCE 200809L

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "aimd_controller.h"
#include "packet_format.h"

#define PAYLOAD_SIZE sizeof(((struct Packet *)0)->data) // 1024
#define PKT_SIZE sizeof(struct Packet)                  // 1036 (packed)
#define ENCODER_POLL_MS 100                             // check lambda every 100 ms
#define FFMPEG_BUFFER_READ (PAYLOAD_SIZE)               // read chunk size

// helper to get current time in seconds (double)
static double now_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

// Start ffmpeg with given rate (kbps) and return FILE* to stdout (rb).
// The command uses -f h264 to write raw H.264 stream to stdout.
static FILE *start_ffmpeg_pipe(double rate_kbps) {
    char cmd[512];
    // Use -loglevel quiet so ffmpeg doesn't flood stderr; adjust if debugging
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -re -i input.mp4 -c:v libx264 -b:v %.0fk -maxrate %.0fk -bufsize %.0fk -preset veryfast -tune zerolatency -f h264 - 2>/dev/null",
        rate_kbps, rate_kbps, rate_kbps * 2);
    // popen opens a read stream from ffmpeg stdout
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("popen(ffmpeg)");
        return NULL;
    }
    return fp;
}

// Kill FFmpeg child started by popen: pclose() will reap it.
// Also try pkill to be safe (silently).
static void stop_ffmpeg_pipe(FILE *fp) {
    if (!fp) return;
    pclose(fp);
    // best-effort kill of lingering ffmpeg processes (silence errors)
    system("pkill -9 ffmpeg > /dev/null 2>&1");
}

void *encoder_thread(void *arg) {
    AIMD_Controller *ctrl = (AIMD_Controller *)arg;

    // UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(8080);
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Start ffmpeg with initial rate
    double last_rate = -1.0;
    FILE *ff = NULL;

    // Buffer to hold chunk read from ffmpeg and packet
    unsigned char buf[FFMPEG_BUFFER_READ];
    struct Packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    uint32_t seq = 0;

    while (1) {
        // read current rate from ctrl under mutex
        pthread_mutex_lock(&ctrl->lock);
        double current_rate = ctrl->lambda;
        pthread_mutex_unlock(&ctrl->lock);

        // (Re)start ffmpeg only when rate changes or ff is not running
        if (ff == NULL || current_rate != last_rate) {
            if (ff) {
                stop_ffmpeg_pipe(ff);
                usleep(150 * 1000); // give some time for cleanup
            }
            ff = start_ffmpeg_pipe(current_rate);
            if (!ff) {
                fprintf(stderr, "[Encoder] Failed to start ffmpeg at %.0f kbps\n", current_rate);
                // wait and retry
                usleep(500 * 1000);
                continue;
            }
            printf("[Encoder] FFmpeg started at %.0f kbps\n", current_rate);
            fflush(stdout);
            last_rate = current_rate;
        }

        // Non-blocking-ish read: attempt to read a chunk from ffmpeg stdout.
        // fread will block until data is available. To keep responsiveness to rate changes,
        // we read small chunks (1024 bytes) and check lambda between reads.
        size_t r = fread(buf, 1, FFMPEG_BUFFER_READ, ff);
        if (r == 0) {
            // EOF or no data (ffmpeg may have died). Restart ffmpeg.
            if (feof(ff) || ferror(ff)) {
                fprintf(stderr, "[Encoder] ffmpeg pipe EOF/error; restarting\n");
                stop_ffmpeg_pipe(ff);
                ff = NULL;
                usleep(200 * 1000);
            }
            // Sleep a little before retrying to avoid busy-loop
            usleep(50 * 1000);
            continue;
        }

        // Build packet (always send fixed-size struct to match receiver)
        memset(&pkt, 0, PKT_SIZE);     // zero padding
        pkt.seq = (int)seq++;
        pkt.send_time = now_seconds();
        // copy r bytes into pkt.data (r <= PAYLOAD_SIZE)
        memcpy(pkt.data, buf, r);

        // Send packet (full struct)
        ssize_t sent = sendto(sock, &pkt, PKT_SIZE, 0,
                              (struct sockaddr *)&dest, sizeof(dest));
        if (sent != PKT_SIZE) {
            perror("[Encoder] sendto");
        }

        // Small sleep to avoid hogging CPU; keep it low because data generation is real-time.
        // Also, check for lambda change frequently (every loop).
        usleep(2000); // 2 ms
    }

    // Cleanup (never reached under normal operation)
    if (ff) stop_ffmpeg_pipe(ff);
    close(sock);
    return NULL;
}
