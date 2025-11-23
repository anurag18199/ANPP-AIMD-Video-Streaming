#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <pthread.h>
#include "aimd_controller.h"

// Do NOT expose logfile here. It stays private inside logger.c.

void logger_init();
void log_state(AIMD_Controller *ctrl);
void logger_close();

#endif
