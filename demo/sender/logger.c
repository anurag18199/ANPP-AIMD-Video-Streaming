// logger.c - FINAL VERSION
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "logger.h"
#include "aimd_controller.h"

static FILE *logfile = NULL;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void logger_init() {
    logfile = fopen("sender_log.csv", "w");
    if (!logfile) {
        perror("Error opening sender_log.csv");
        exit(1);
    }
    fprintf(logfile, "Iteration,Rate(kbps),Loss,Delay(sec)\n");
    fflush(logfile);
}

void log_state(AIMD_Controller *ctrl) {
    if (!logfile) return;

    pthread_mutex_lock(&log_lock);

    pthread_mutex_lock(&ctrl->lock);  // protect AIMD state
    fprintf(logfile, "%lu,%.2f,%.4f,%.6f\n",
            ctrl->iteration,
            ctrl->lambda,
            ctrl->loss_rate,
            ctrl->avg_delay);
    pthread_mutex_unlock(&ctrl->lock);

    fflush(logfile);
    pthread_mutex_unlock(&log_lock);
}

void logger_close() {
    if (logfile) {
        fclose(logfile);
        logfile = NULL;
    }
}
