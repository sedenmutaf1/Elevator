#include "elevator.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -f <floors>      Number of floors (default 12)\n");
    printf("  -e <elevators>   Number of elevators (default 3)\n");
    printf("  -p <passengers>  Number of passenger generator threads (default 2)\n");
    printf("  -d <seconds>     Duration to run the simulation (default 20)\n");
    printf("  -q               Quiet mode (less console output)\n");
}

int main(int argc, char **argv) {
    int floors = 12;
    int elevators = 3;
    int passengers = 2;
    int duration = 20;
    bool verbose = true;

    int opt;
    while ((opt = getopt(argc, argv, "f:e:p:d:qh")) != -1) {
        switch (opt) {
            case 'f': floors = atoi(optarg); break;
            case 'e': elevators = atoi(optarg); break;
            case 'p': passengers = atoi(optarg); break;
            case 'd': duration = atoi(optarg); break;
            case 'q': verbose = false; break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    Simulation sim;
    simulation_init(&sim, floors, elevators, passengers, duration, verbose);
    simulation_start(&sim);

    for (int i = 0; i < duration; i++) {
        if (verbose) {
            simulation_print_status(&sim);
        }
        sleep(1);
    }

    simulation_stop(&sim);
    simulation_join(&sim);
    simulation_print_status(&sim);

    return 0;
}
