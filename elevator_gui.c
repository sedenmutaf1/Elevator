#include "elevator.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Simulation sim;
    GtkWidget *status_grid;
    GtkWidget *average_label;
} GuiState;

static gboolean refresh_ui(gpointer user_data) {
    GuiState *state = (GuiState *)user_data;
    Simulation *sim = &state->sim;

    pthread_mutex_lock(&sim->stats_mutex);
    double average_wait = (sim->completed_trips == 0) ? 0.0 : sim->total_wait_time / sim->completed_trips;
    long trips = sim->completed_trips;
    pthread_mutex_unlock(&sim->stats_mutex);

    gchar buffer[128];
    g_snprintf(buffer, sizeof(buffer), "Completed trips: %ld | Avg wait: %.2f sec", trips, average_wait);
    gtk_label_set_text(GTK_LABEL(state->average_label), buffer);

    for (int i = 0; i < sim->elevator_count; i++) {
        Elevator *e = &sim->elevators[i];
        gchar text[128];
        const char *state_text = (e->state == ELEVATOR_IDLE) ? "Idle" : (e->state == ELEVATOR_MOVING ? "Moving" : "Door Open");
        g_snprintf(text, sizeof(text), "Elevator %d: Floor %d (target %d) - %s", e->id, e->current_floor, e->target_floor, state_text);
        GtkWidget *label = gtk_grid_get_child_at(GTK_GRID(state->status_grid), 0, i);
        if (label) {
            gtk_label_set_text(GTK_LABEL(label), text);
        }
    }

    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GuiState *state = (GuiState *)user_data;
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Elevator Simulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 480, 240);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_window_set_child(GTK_WINDOW(window), box);

    state->status_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(state->status_grid), 4);
    gtk_grid_set_row_spacing(GTK_GRID(state->status_grid), 4);

    for (int i = 0; i < state->sim.elevator_count; i++) {
        gchar text[64];
        g_snprintf(text, sizeof(text), "Elevator %d", i);
        GtkWidget *label = gtk_label_new(text);
        gtk_grid_attach(GTK_GRID(state->status_grid), label, 0, i, 1, 1);
    }

    state->average_label = gtk_label_new("Completed trips: 0 | Avg wait: 0.00 sec");

    gtk_box_append(GTK_BOX(box), state->status_grid);
    gtk_box_append(GTK_BOX(box), state->average_label);

    gtk_window_present(GTK_WINDOW(window));

    g_timeout_add(250, refresh_ui, state);
}

static void on_shutdown(GApplication *app, gpointer user_data) {
    (void)app;
    GuiState *state = (GuiState *)user_data;
    simulation_stop(&state->sim);
}

int main(int argc, char **argv) {
    GuiState state;
    simulation_init(&state.sim, 12, 3, 2, 30, false);
    simulation_start(&state.sim);

    GtkApplication *app = gtk_application_new("edu.ozyegin.cs350.elevator", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), &state);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    simulation_join(&state.sim);
    g_object_unref(app);
    return status;
}
