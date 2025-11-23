#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define FEEDBACK_PORT 9091

extern pthread_mutex_t fb_mutex;

void *feedback_sender_thread(void *arg) {
    double *fb = (double *)arg;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(FEEDBACK_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("FeedBackSender sending feedback to port %d\n", FEEDBACK_PORT);

    struct {
        double loss_rate;
        double avg_delay;
    } msg;

    while (1) {
        pthread_mutex_lock(&fb_mutex);
        msg.loss_rate = fb[0];
        msg.avg_delay = fb[1];
        pthread_mutex_unlock(&fb_mutex);

        sendto(sockfd, &msg, sizeof(msg), 0,
               (struct sockaddr *)&addr, sizeof(addr));

        printf("feedbackSender loss=%.4f delay=%.6f\n",
               msg.loss_rate, msg.avg_delay);

        usleep(200000); // 200ms
    }

    close(sockfd);
    return NULL;
}
