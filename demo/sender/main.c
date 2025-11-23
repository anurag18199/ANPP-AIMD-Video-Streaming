#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "aimd_controller.h"
#include "logger.h"

// Correct prototypes
void *feedback_listener_thread(void *arg);
void *encoder_thread(void *arg);

int main() {
    AIMD_Controller ctrl;
    aimd_init(&ctrl);

    // initialize logger
    logger_init();

    pthread_t fb_thread, enc_thread;

    pthread_create(&fb_thread, NULL, feedback_listener_thread, &ctrl);
    pthread_create(&enc_thread, NULL, encoder_thread, &ctrl);

    pthread_join(fb_thread, NULL);
    pthread_join(enc_thread, NULL);

    logger_close();
    return 0;
}
