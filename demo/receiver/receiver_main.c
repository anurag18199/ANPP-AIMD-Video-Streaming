#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>

pthread_mutex_t fb_mutex = PTHREAD_MUTEX_INITIALIZER;

void* packet_receiver_thread(void* arg);
void* feedback_sender_thread(void* arg);

int main(){
    double feedback_values[2] = {0.0, 0.0};

    pthread_t recv_thread, send_thread;
    pthread_create(&recv_thread, NULL, packet_receiver_thread, feedback_values);
    pthread_create(&send_thread, NULL, feedback_sender_thread, feedback_values);

    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);
    return 0;
}