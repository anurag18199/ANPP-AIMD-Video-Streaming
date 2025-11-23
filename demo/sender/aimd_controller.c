// aimd_controller.c
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include "aimd_controller.h"

/*
 * AIMD controller implementation.
 *
 * Notes:
 *  - lambda is in kbps.
 *  - ALPHA and BETA in aimd_controller.h should be chosen so that:
 *      lambda += ALPHA * SCALE (when increasing)
 *      lambda -= BETA  * SCALE * loss_rate (when decreasing)
 *    SCALE is 1000.0 if you want to convert from "units" to kbps as before.
 */

void aimd_init(AIMD_Controller *ctrl) {
    if (!ctrl) return;

    ctrl->lambda = 1000.0;
    ctrl->loss_rate = 0.0;
    ctrl->avg_delay = 0.0;
    ctrl->alpha = ALPHA;
    ctrl->beta = BETA;
    ctrl->delta_l = DELTA_L;
    ctrl->delta_h = DELTA_H;
    ctrl->min_rate = MIN_RATE;
    ctrl->max_rate = MAX_RATE;
    ctrl->iteration = 0;
    ctrl->state = INIT;
    pthread_mutex_init(&ctrl->lock, NULL);
}

/*
 * aimd_update:
 *  - updates ctrl->lambda under ctrl->lock
 *  - returns the new lambda (kbps)
 */
double aimd_update(AIMD_Controller *ctrl) {
    if (!ctrl) return 0.0;

    pthread_mutex_lock(&ctrl->lock);

    ctrl->iteration++;

    /* sanitize loss_rate */
    if (ctrl->loss_rate < 0.0) ctrl->loss_rate = 0.0;

    if (ctrl->loss_rate > ctrl->delta_h) {
        /* multiplicative decrease proportional to measured loss */
        double decrease = ctrl->beta * SCALE * ctrl->loss_rate; /* SCALE set in header */
        ctrl->lambda -= decrease;
        if (ctrl->lambda < ctrl->min_rate) ctrl->lambda = ctrl->min_rate;
        ctrl->state = DECREASE_RATE;
    } else if (ctrl->loss_rate < ctrl->delta_l) {
        /* additive increase */
        double increase = ctrl->alpha * SCALE;
        ctrl->lambda += increase;
        if (ctrl->lambda > ctrl->max_rate) ctrl->lambda = ctrl->max_rate;
        ctrl->state = INCREASE_RATE;
    } else {
        ctrl->state = STABLE;
    }

    double new_lambda = ctrl->lambda;
    pthread_mutex_unlock(&ctrl->lock);

    return new_lambda;
}
