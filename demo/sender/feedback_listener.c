// feedback_listener.c
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "aimd_controller.h"
#include "logger.h"

#define FEEDBACK_PORT 9091

void *feedback_listener_thread(void *arg) {
    AIMD_Controller *ctrl = (AIMD_Controller *)arg;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(FEEDBACK_PORT);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("[Feedback] Listening on port %d\n", FEEDBACK_PORT);

    struct {
        double loss_rate;
        double avg_delay;
    } fb;

    while (1) {

        int n = recvfrom(
            sockfd,
            &fb,
            sizeof(fb),
            0,
            (struct sockaddr *)&cliaddr,
            &len
        );

        if (n != sizeof(fb)) continue;  // ignore corrupted packets

        printf("[Feedback] recv loss=%.4f delay=%.4f\n",
               fb.loss_rate, fb.avg_delay);

        // Update AIMD state safely
        pthread_mutex_lock(&ctrl->lock);
        ctrl->loss_rate = fb.loss_rate;
        ctrl->avg_delay = fb.avg_delay;
        pthread_mutex_unlock(&ctrl->lock);

        double new_lambda = aimd_update(ctrl);

        // Log after update
        log_state(ctrl);

        printf("[AIMD] Updated λ=%.2f kbps\n", new_lambda);
        fflush(stdout);
    }

    return NULL;
}
