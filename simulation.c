#define _POSIX_C_SOURCE 200809L
#include "elevator.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double timespec_to_seconds(const struct timespec *ts) {
    return (double)ts->tv_sec + (double)ts->tv_nsec / 1e9;
}

static void sleep_millis(long millis) {
    struct timespec ts;
    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (millis % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static struct timespec now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts;
}

static void queue_init(RequestQueue *queue) {
    memset(queue, 0, sizeof(*queue));
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

static bool queue_push(RequestQueue *queue, ElevatorRequest req) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->count == REQUEST_QUEUE_SIZE) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }
    queue->buffer[queue->tail] = req;
    queue->tail = (queue->tail + 1) % REQUEST_QUEUE_SIZE;
    queue->count++;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    return true;
}

static bool queue_pop(RequestQueue *queue, ElevatorRequest *out, bool running) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0 && running) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }
    *out = queue->buffer[queue->head];
    queue->head = (queue->head + 1) % REQUEST_QUEUE_SIZE;
    queue->count--;
    pthread_mutex_unlock(&queue->mutex);
    return true;
}

static int queue_count(RequestQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    int value = queue->count;
    pthread_mutex_unlock(&queue->mutex);
    return value;
}

static int choose_floor_with_bias(int floors) {
    int roll = rand() % 100;
    if (roll < 50) {
        return 0; // Ground floor more popular
    }
    return rand() % floors;
}

static void *passenger_thread(void *arg) {
    Simulation *sim = (Simulation *)arg;
    while (sim->running) {
        int from = choose_floor_with_bias(sim->floors);
        int to = rand() % sim->floors;
        while (to == from) {
            to = rand() % sim->floors;
        }

        ElevatorRequest req = {
            .from_floor = from,
            .to_floor = to,
            .created_at = now(),
        };
        queue_push(&sim->queue, req);

        sleep_millis(150 + (rand() % 350));
    }
    return NULL;
}

static void update_stats(Simulation *sim, const ElevatorRequest *req, const struct timespec *handled_at) {
    double created = timespec_to_seconds(&req->created_at);
    double handled = timespec_to_seconds(handled_at);
    double wait = handled - created;
    pthread_mutex_lock(&sim->stats_mutex);
    sim->completed_trips++;
    sim->total_wait_time += wait;
    pthread_mutex_unlock(&sim->stats_mutex);
}

static void sleep_for_floor(void) {
    sleep_millis(120); // simulate travel time per floor
}

static void *elevator_thread(void *arg) {
    Elevator *elevator = (Elevator *)arg;
    Simulation *sim = elevator->sim;

    while (1) {
        pthread_mutex_lock(&sim->queue.mutex);
        bool running = sim->running;
        int pending = sim->queue.count;
        pthread_mutex_unlock(&sim->queue.mutex);

        if (!running && pending == 0) {
            break;
        }

        ElevatorRequest req;
        bool has_request = queue_pop(&sim->queue, &req, running);
        if (!has_request) {
            continue;
        }

        elevator->target_floor = req.from_floor;
        elevator->state = ELEVATOR_MOVING;
        while (elevator->current_floor != elevator->target_floor) {
            elevator->current_floor += (elevator->target_floor > elevator->current_floor) ? 1 : -1;
            sleep_for_floor();
        }

        elevator->state = ELEVATOR_DOOR_OPEN;
        elevator->door_open = true;
        struct timespec reached_pickup = now();
        update_stats(sim, &req, &reached_pickup);
        sleep_millis(180); // loading time

        elevator->target_floor = req.to_floor;
        elevator->state = ELEVATOR_MOVING;
        elevator->door_open = false;
        while (elevator->current_floor != elevator->target_floor) {
            elevator->current_floor += (elevator->target_floor > elevator->current_floor) ? 1 : -1;
            sleep_for_floor();
        }

        elevator->state = ELEVATOR_DOOR_OPEN;
        elevator->door_open = true;
        sleep_millis(180);

        elevator->door_open = false;
        elevator->state = ELEVATOR_IDLE;
    }
    return NULL;
}

void simulation_init(Simulation *sim, int floors, int elevator_count, int passenger_threads, int runtime_seconds, bool verbose) {
    if (floors > MAX_FLOORS) floors = MAX_FLOORS;
    if (elevator_count > MAX_ELEVATORS) elevator_count = MAX_ELEVATORS;

    sim->floors = floors;
    sim->elevator_count = elevator_count;
    sim->passenger_thread_count = passenger_threads;
    sim->runtime_seconds = runtime_seconds;
    sim->verbose = verbose;
    sim->completed_trips = 0;
    sim->total_wait_time = 0.0;
    sim->running = false;

    pthread_mutex_init(&sim->stats_mutex, NULL);
    queue_init(&sim->queue);

    for (int i = 0; i < sim->elevator_count; i++) {
        sim->elevators[i].id = i;
        sim->elevators[i].current_floor = 0;
        sim->elevators[i].target_floor = 0;
        sim->elevators[i].state = ELEVATOR_IDLE;
        sim->elevators[i].door_open = false;
        sim->elevators[i].sim = sim;
    }
}

void simulation_start(Simulation *sim) {
    sim->running = true;
    srand((unsigned int)time(NULL));

    int passengers = sim->passenger_thread_count;
    if (passengers > MAX_PASSENGER_THREADS) {
        passengers = MAX_PASSENGER_THREADS;
    }
    for (int i = 0; i < passengers; i++) {
        pthread_create(&sim->passenger_threads[i], NULL, passenger_thread, sim);
    }
    for (int i = 0; i < sim->elevator_count; i++) {
        pthread_create(&sim->elevators[i].thread, NULL, elevator_thread, &sim->elevators[i]);
    }
}

void simulation_stop(Simulation *sim) {
    sim->running = false;
    pthread_mutex_lock(&sim->queue.mutex);
    pthread_cond_broadcast(&sim->queue.cond);
    pthread_mutex_unlock(&sim->queue.mutex);
}

void simulation_join(Simulation *sim) {
    int passengers = sim->passenger_thread_count;
    if (passengers > MAX_PASSENGER_THREADS) {
        passengers = MAX_PASSENGER_THREADS;
    }
    for (int i = 0; i < passengers; i++) {
        pthread_join(sim->passenger_threads[i], NULL);
    }
    for (int i = 0; i < sim->elevator_count; i++) {
        pthread_join(sim->elevators[i].thread, NULL);
    }
}

void simulation_print_status(Simulation *sim) {
    pthread_mutex_lock(&sim->stats_mutex);
    double average_wait = (sim->completed_trips == 0) ? 0.0 : sim->total_wait_time / sim->completed_trips;
    pthread_mutex_unlock(&sim->stats_mutex);

    printf("\n=== Elevator Status ===\n");
    for (int i = 0; i < sim->elevator_count; i++) {
        Elevator *e = &sim->elevators[i];
        const char *state = (e->state == ELEVATOR_IDLE) ? "IDLE" : (e->state == ELEVATOR_MOVING ? "MOVING" : "DOOR_OPEN");
        printf("Elevator %d: floor %d, target %d, state %s%s\n", e->id, e->current_floor, e->target_floor, state, e->door_open ? " (door open)" : "");
    }
    printf("Requests waiting: %d\n", queue_count(&sim->queue));
    printf("Completed trips: %ld | Avg wait: %.2f sec\n", sim->completed_trips, average_wait);
}
