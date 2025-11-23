#ifndef AIMD_CONTROLLER_H
#define AIMD_CONTROLLER_H

#include <pthread.h>

#define SCALE 1000.0   /* keep units explicit; change if you want different scaling */
#define ALPHA 1.0      /* additive step (units of SCALE per tick) */
#define BETA  0.5      /* multiplicative factor; tune */
#define DELTA_L 0.005  /* lower loss threshold (e.g. 0.5%) */
#define DELTA_H 0.01   /* upper loss threshold (e.g. 1%) */
#define MIN_RATE 100.0 /* kbps */
#define MAX_RATE 5000.0/* kbps */

typedef enum { INIT=0, INCREASE_RATE, DECREASE_RATE, STABLE } AIMD_State;

typedef struct {
    double lambda;      /* kbps */
    double loss_rate;   /* fraction 0..1 */
    double avg_delay;   /* seconds */
    double alpha;
    double beta;
    double delta_l;
    double delta_h;
    double min_rate;
    double max_rate;
    unsigned long iteration;
    AIMD_State state;
    pthread_mutex_t lock;
} AIMD_Controller;

void aimd_init(AIMD_Controller *ctrl);
double aimd_update(AIMD_Controller *ctrl);

#endif
