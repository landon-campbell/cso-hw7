#define _XOPEN_SOURCE 700
#include "life.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


// define a global struct for thread data
typedef struct  {
    pthread_barrier_t *barrier;
    LifeBoard *state;
    LifeBoard *next_state;
    int steps;
    int id;
    int threads;
} ThreadData;

void *thread_function(void *arg) {

    ThreadData *data = (ThreadData *) arg;
    LifeBoard *state = data->state;
    LifeBoard *next_state = data->next_state;
    int steps = data->steps;
    int id = data->id;
    int threads = data->threads;
    pthread_barrier_t *barrier = data->barrier;

    // calculate the number of rows each thread will process
    int rows_per_thread = (state->height - 2) / threads;
    int extra_rows = (state->height - 2) % threads;

    int start_row = 1 + id * rows_per_thread + (id < extra_rows ? id : extra_rows);
    int end_row = start_row + rows_per_thread + (id < extra_rows ? 0 : -1);

    for (int step = 0; step < steps; step += 1) {

        /* We use the range [1, width - 1) here instead of
         * [0, width) because we fix the edges to be all 0s.
         */
        for (int y = start_row; y <= end_row; y += 1) {
            for (int x = 1; x < state->width - 1; x += 1) {
                
                /* For each cell, examine a 3x3 "window" of cells around it,
                 * and count the number of live (true) cells in the window. */
                int live_in_window = 0;
                for (int y_offset = -1; y_offset <= 1; y_offset += 1){
                    for (int x_offset = -1; x_offset <= 1; x_offset += 1){
                        if (x_offset == 0 && y_offset == 0)
                            continue;
                        if (LB_get(state, x + x_offset, y + y_offset))
                            live_in_window += 1;
                    }
                }
                /* Cells with 3 live neighbors remain or become live.
                   Live cells with 2 live neighbors remain live. */
                LB_set(next_state, x, y,
                    live_in_window == 3 /* dead cell with 3 neighbors or live cell with 2 */ ||
                    (live_in_window == 2 && LB_get(state, x, y)) /* live cell with 3 neighbors */
                );
            }
        }

        // wait for all threads to finish before moving to the next step
        pthread_barrier_wait(barrier);
        
        /* now that we computed next_state, make it the current state */
        if (id == 0)
            LB_swap(state, next_state);

        // wait for all threads to finish before moving to the next step
        pthread_barrier_wait(barrier);
    }
    return NULL;
}

void simulate_life_parallel(int threads, LifeBoard *state, int steps) {
    LifeBoard *next_state = LB_new(state->width, state->height);

    pthread_t id[threads];
    pthread_barrier_t barrier;

    // initialize barrier and check that it was successful
    if (pthread_barrier_init(&barrier, NULL, threads) != 0) {
        fprintf(stderr, "Error initializing barrier\n");
        exit(1);
    }

    // create thread data struct
    ThreadData thread_data[threads];

    // initialize thread data struct
    for (int i = 0; i < threads; i += 1) {
        thread_data[i].barrier = &barrier;
        thread_data[i].state = state;
        thread_data[i].next_state = next_state;
        thread_data[i].steps = steps;
        thread_data[i].id = i;
        thread_data[i].threads = threads;

        // create threads and check that they were created successfully
        if(pthread_create(&id[i], NULL, thread_function, &thread_data[i]) != 0){
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }
    }

    // join threads
    for (int i = 0; i < threads; i += 1) {
        if(pthread_join(id[i], NULL) != 0){
            fprintf(stderr, "Error joining thread\n");
            exit(1);
        }
    }

    // destroy barrier
    pthread_barrier_destroy(&barrier);

    LB_del(next_state);
}
