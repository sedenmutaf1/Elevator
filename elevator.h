#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define MAX_ELEVATORS 16
#define MAX_FLOORS 64
#define REQUEST_QUEUE_SIZE 256
#define MAX_PASSENGER_THREADS 32

typedef enum {
    ELEVATOR_IDLE,
    ELEVATOR_MOVING,
    ELEVATOR_DOOR_OPEN
} ElevatorState;

typedef struct {
    int from_floor;
    int to_floor;
    struct timespec created_at;
} ElevatorRequest;

typedef struct {
    ElevatorRequest buffer[REQUEST_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} RequestQueue;

typedef struct Simulation Simulation;

typedef struct {
    int id;
    int current_floor;
    int target_floor;
    ElevatorState state;
    bool door_open;
    pthread_t thread;
    Simulation *sim;
} Elevator;

struct Simulation {
    int floors;
    int elevator_count;
    int passenger_thread_count;
    int runtime_seconds;
    bool verbose;

    Elevator elevators[MAX_ELEVATORS];
    RequestQueue queue;

    pthread_mutex_t stats_mutex;
    long completed_trips;
    double total_wait_time;

    pthread_t passenger_threads[MAX_PASSENGER_THREADS];
    bool running;
};

void simulation_init(Simulation *sim, int floors, int elevator_count, int passenger_threads, int runtime_seconds, bool verbose);
void simulation_start(Simulation *sim);
void simulation_stop(Simulation *sim);
void simulation_join(Simulation *sim);
void simulation_print_status(Simulation *sim);

#endif
