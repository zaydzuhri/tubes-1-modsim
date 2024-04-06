/* External definitions for car-rental and airport terminals model. */
// Consider a car-rental system shown in Fig.2.72, with all distances given in miles.
// People arrive at location i (where i = 1, 2, 3) with independent exponential interarrival times at respective rates of 14, 10, and 24 per hour.
// Each location has a FIFO queue with unlimited capacity. There is one bus with a capacity of 20 people and a speed of 30 miles per hour.
// The bus is initially at location 3(car rental), and leaves immediately in a counterclockwise direction.
// All people arriving at a terminal want to go to the car rental.All people arriving at the car rental want to go to terminals I and 2 with respective probabilities 0.583 and 0.417.
// When a bus arrives at a location, the following rules apply:
// ㆍPeople are first unloaded in a FIFO manner. The time to unload one person is distributed uniformly between 16 and 24 seconds.
// ㆍPeople are then loaded on the bus up to its capacity, with a loading time per person that is distributed uniformly between 15 and 25 seconds.
// ㆍThe bus always spends at least 5 minutes at each location. If no loading or unloading is in process after 5 minutes, the bus will leave immediately.
// Run a simulation for 80 hours and gather statistics

#include "simlib.h" /* Required for use of simlib.c. */

#define RENTAL_ID 3                       /* Location number for the car rental. */
#define TERMINAL_1_ID 1                   /* Location number for terminal 1. */
#define TERMINAL_2_ID 2                   /* Location number for terminal 2. */
#define BUS_ID 4                          /* Location number for the bus. */
#define EVENT_PERSON_ARRIVAL_RENTAL 1     /* Event type for arrival of a person to the car rental. */
#define EVENT_PERSON_ARRIVAL_TERMINAL_1 2 /* Event type for arrival of a person to terminal 1. */
#define EVENT_PERSON_ARRIVAL_TERMINAL_2 3 /* Event type for arrival of a person to terminal 2. */
#define EVENT_BUS_ARRIVAL 4               /* Event type for arrival of a bus in a location. */
#define EVENT_BUS_DEPARTURE 5             /* Event type for departure of a bus from a location. */
#define EVENT_UNLOAD_PERSON 6             /* Event type for loading a person to the bus. */
#define EVENT_LOAD_PERSON 7               /* Event type for unloading a person from the bus. */
#define EVENT_END_SIMULATION 8            /* Event type for end of the simulation. */
#define STREAM_INTERARRIVAL_RENTAL 1      /* Random-number stream for interarrivals. */
#define STREAM_INTERARRIVAL_TERMINAL_1 2  /* Random-number stream for interarrivals. */
#define STREAM_INTERARRIVAL_TERMINAL_2 3  /* Random-number stream for interarrivals. */
#define STREAM_UNLOADING 4                /* Random-number stream for job types. */
#define STREAM_LOADING 5                  /* Random-number stream for service times. */
#define STREAM_DESTINATION 6              /* Random-number stream for determining the destination of a person from car rental. */

/* Declare non-simlib global variables. */
int bus_capacity = 20, current_bus_location = RENTAL_ID;
int unload_time_lower = 16, unload_time_upper = 24, load_time_lower = 15, load_time_upper = 25;                                              // in seconds
double bus_speed = 30.0 / 60.0 / 60.0;                                                                                                       // in miles per second
double terminal_1_arrival_rate = 14.0 / 60.0 / 60.0, terminal_2_arrival_rate = 10.0 / 60.0 / 60.0, rental_arrival_rate = 24.0 / 60.0 / 60.0; // in per second
double distance_rental_terminal_1 = 4.5, distance_terminal_1_terminal_2 = 1.0, distance_terminal_2_rental = 4.5;                             // in miles
double destination_terminal_1_probability = 0.583, destination_terminal_2_probability = 0.417;
double length_simulation = 80.0 * 60.0 * 60.0; // we observe clock in seconds
double last_bus_arrive_time = 0.0;             // Timer to keep track of bus stop time at a location.
double last_bus_at_rental = 0.0;               // Timer to keep track of bus stop time at rental.
double bus_wait_time = 5.0 * 60.0;
double current_bus_wait_time = 0.0;
int bus_arrived = 0;
int is_unloading = 0;
FILE *outfile;

void person_arrive(int location) // Event function for arrival of a person to a location.
{
    int destination;
    // Schedule arrival of next person in this location and determine the destination of this person.
    switch (location) {
    case RENTAL_ID:
        event_schedule(sim_time + expon(1.0 / rental_arrival_rate, STREAM_INTERARRIVAL_RENTAL), EVENT_PERSON_ARRIVAL_RENTAL);
        destination = (uniform(0.0, 1.0, STREAM_DESTINATION) < destination_terminal_1_probability) ? TERMINAL_1_ID : TERMINAL_2_ID;
        break;
    case TERMINAL_1_ID:
        event_schedule(sim_time + expon(1.0 / terminal_1_arrival_rate, STREAM_INTERARRIVAL_TERMINAL_1), EVENT_PERSON_ARRIVAL_TERMINAL_1);
        destination = RENTAL_ID;
        break;
    case TERMINAL_2_ID:
        event_schedule(sim_time + expon(1.0 / terminal_2_arrival_rate, STREAM_INTERARRIVAL_TERMINAL_2), EVENT_PERSON_ARRIVAL_TERMINAL_2);
        destination = RENTAL_ID;
        break;
    }

    // Add the person to the queue of this location.
    transfer[1] = sim_time;
    transfer[2] = destination;
    transfer[3] = location;
    list_file(LAST, location);

    // If a bus is at this location, schedule loading of this person
    if (bus_arrived && !is_unloading && current_bus_location == location && list_size[BUS_ID] < bus_capacity && list_size[location] > 0) {
        event_schedule(sim_time + uniform(load_time_lower, load_time_upper, STREAM_LOADING), EVENT_LOAD_PERSON);
        // Cancel bus departure if it is scheduled.
        event_cancel(EVENT_BUS_DEPARTURE);
    }
}

void bus_arrive(int location) // Event function for arrival of a bus in a location. Because of the nature of unloading and loading, this event only starts unloading and loading processes and starts a timer.
{
    last_bus_arrive_time = sim_time;
    bus_arrived = 1;
    // If people on bus
    if (list_size[BUS_ID] > 0) {
        // Start unloading process.
        event_schedule(sim_time + uniform(unload_time_lower, unload_time_upper, STREAM_UNLOADING), EVENT_UNLOAD_PERSON);
        is_unloading = 1;
        // If no people on bus but people in queue at this location
    } else if (list_size[location] > 0 && list_size[BUS_ID] < bus_capacity) {
        // Start loading process.
        event_schedule(sim_time + uniform(load_time_lower, load_time_upper, STREAM_LOADING), EVENT_LOAD_PERSON);
    } else {
        // Make sure double departure never happens
        event_cancel(EVENT_BUS_DEPARTURE);
        // If no people in queue and no people on bus, schedule bus departure.
        current_bus_wait_time = (sim_time - last_bus_arrive_time > bus_wait_time) ? 0 : bus_wait_time - (sim_time - last_bus_arrive_time);
        event_schedule(sim_time + current_bus_wait_time, EVENT_BUS_DEPARTURE);
    }
}

void bus_depart(int location) // Event function for departure of a bus from a location. This function schedules the arrival of the bus to the next location.
{
    // Schedule arrival of the bus to the next location.
    double next_distance;
    switch (location) {
    case RENTAL_ID:
        next_distance = distance_rental_terminal_1;
        break;
    case TERMINAL_1_ID:
        next_distance = distance_terminal_1_terminal_2;
        break;
    case TERMINAL_2_ID:
        next_distance = distance_terminal_2_rental;
        break;
    }
    event_schedule(sim_time + (next_distance / bus_speed), EVENT_BUS_ARRIVAL);
    current_bus_location = (location == RENTAL_ID) ? TERMINAL_1_ID : (location == TERMINAL_1_ID) ? TERMINAL_2_ID
                                                                                                 : RENTAL_ID;
    bus_arrived = 0;
    // Record time the bus was at this location.
    sampst(sim_time - last_bus_arrive_time, location + 5);
    // Record lap time
    if (location == RENTAL_ID) {
        if (last_bus_at_rental != 0.0) {
            sampst(sim_time - last_bus_at_rental, 10);
        }
        last_bus_at_rental = sim_time;
    }
}

void person_unload(int location) // Event function for unloading a person from the bus. This function schedules the unloading of the next person if there are still people on the bus.
{
    if (bus_arrived) {
        // Make sure double departure never happens
        event_cancel(EVENT_BUS_DEPARTURE);
        // Only unload person whose destination is this location.
        // Need to go through list to find foremost person whose destination is this location.
        int i = list_size[BUS_ID];
        int found = 0;
        while (i > 0 && !found) {
            list_remove(FIRST, BUS_ID);
            if (transfer[2] == location) {
                found = 1;
                // Record time this person was in system.
                sampst(sim_time - transfer[1], transfer[3] + 10);
            } else {
                list_file(LAST, BUS_ID);
            }
            i--;
        }
        // If there are still people on the bus, schedule unloading of the next person.
        if (found && list_size[BUS_ID] > 0) {
            event_schedule(sim_time + uniform(unload_time_lower, unload_time_upper, STREAM_UNLOADING), EVENT_UNLOAD_PERSON);
        } else if (list_size[location] > 0 && list_size[BUS_ID] < bus_capacity) {
            // If people in queue at this location, start loading process.
            event_schedule(sim_time + uniform(load_time_lower, load_time_upper, STREAM_LOADING), EVENT_LOAD_PERSON);
            is_unloading = 0;
        } else {
            // If no people in queue and no people on bus, schedule bus departure.
            current_bus_wait_time = (sim_time - last_bus_arrive_time > bus_wait_time) ? 0 : bus_wait_time - (sim_time - last_bus_arrive_time);
            event_schedule(sim_time + current_bus_wait_time, EVENT_BUS_DEPARTURE);
            is_unloading = 0;
        }
    }
}

void person_load(int location) // Event function for loading a person to the bus. This function schedules the loading of the next person if there are still people in the queue.
{
    if (bus_arrived) {
        // Make sure double departure never happens
        event_cancel(EVENT_BUS_DEPARTURE);
        // If bus is not full
        if (list_size[BUS_ID] < bus_capacity && list_size[location] > 0) {
            // Load one person to the bus.
            list_remove(FIRST, location);
            // Record delay of this person.
            sampst(sim_time - transfer[1], location);
            // Add this person to the bus.
            list_file(LAST, BUS_ID);
            // If there are still people in the queue, schedule loading of the next person
            if (list_size[location] > 0 && list_size[BUS_ID] < bus_capacity) {
                event_schedule(sim_time + uniform(load_time_lower, load_time_upper, STREAM_LOADING), EVENT_LOAD_PERSON);
            } else {
                current_bus_wait_time = (sim_time - last_bus_arrive_time > bus_wait_time) ? 0 : bus_wait_time - (sim_time - last_bus_arrive_time);
                event_schedule(sim_time + current_bus_wait_time, EVENT_BUS_DEPARTURE);
            }
        } else {
            // If no people in queue and no people on bus, schedule bus departure.
            current_bus_wait_time = (sim_time - last_bus_arrive_time > bus_wait_time) ? 0 : bus_wait_time - (sim_time - last_bus_arrive_time);
            event_schedule(sim_time + current_bus_wait_time, EVENT_BUS_DEPARTURE);
        }
    }
}

void report(void) /* Report generator function. */
{
    // Report average and maximum queue length for each location through filest that reads from lists
    fprintf(outfile, "\n\nLocation     Average queue length     Maximum queue length");
    filest(RENTAL_ID);
    fprintf(outfile, "\n\nRental%27.3f%25.3f", transfer[1], transfer[2]);
    filest(TERMINAL_1_ID);
    fprintf(outfile, "\nTerminal 1%23.3f%25.3f", transfer[1], transfer[2]);
    filest(TERMINAL_2_ID);
    fprintf(outfile, "\nTerminal 2%23.3f%25.3f", transfer[1], transfer[2]);

    // Report average and maximum delay in each location through sampst that reads from sampst
    fprintf(outfile, "\n\n\n\nLocation           Average delay            Maximum delay");
    sampst(0.0, -RENTAL_ID);
    fprintf(outfile, "\n\nRental%26.3f%25.3f", transfer[1], transfer[3]);
    sampst(0.0, -TERMINAL_1_ID);
    fprintf(outfile, "\nTerminal 1%22.3f%25.3f", transfer[1], transfer[3]);
    sampst(0.0, -TERMINAL_2_ID);
    fprintf(outfile, "\nTerminal 2%22.3f%25.3f", transfer[1], transfer[3]);

    // Report average and maximum num of people in bus (which is just the queue length of bus)
    fprintf(outfile, "\n\n\n\nAverage number of people on bus      Maximum number of people on bus");
    filest(BUS_ID);
    fprintf(outfile, "\n\n%.3f%38.3f", transfer[1], transfer[2]);

    // Report average maximum minimum time bus is stopped at each location
    fprintf(outfile, "\n\n\n\nLocation     Average bus stop time        Maximum bus stop time       Minimum bus stop time");
    sampst(0.0, -(RENTAL_ID + 5));
    fprintf(outfile, "\n\nRental%28.3f%29.3f%28.3f", transfer[1], transfer[3], transfer[4]);
    sampst(0.0, -(TERMINAL_1_ID + 5));
    fprintf(outfile, "\nTerminal 1%24.3f%29.3f%28.3f", transfer[1], transfer[3], transfer[4]);
    sampst(0.0, -(TERMINAL_2_ID + 5));
    fprintf(outfile, "\nTerminal 2%24.3f%29.3f%28.3f", transfer[1], transfer[3], transfer[4]);

    // Report average maximum minimum time bus goes from rental to rental
    fprintf(outfile, "\n\n\n\nAverage bus lap time      Maximum bus lap time       Minimum bus lap time");
    sampst(0.0, -10);
    fprintf(outfile, "\n\n%.3f%26.3f%28.3f", transfer[1], transfer[3], transfer[4]);

    // Report average maximum minimum time in system for each location
    fprintf(outfile, "\n\n\n\nLocation     Average time in system     Maximum time in system       Minimum time in system");
    sampst(0.0, -(RENTAL_ID + 10));
    fprintf(outfile, "\n\nRental%29.3f%27.3f%29.3f", transfer[1], transfer[3], transfer[4]);
    sampst(0.0, -(TERMINAL_1_ID + 10));
    fprintf(outfile, "\nTerminal 1%25.3f%27.3f%29.3f", transfer[1], transfer[3], transfer[4]);
    sampst(0.0, -(TERMINAL_2_ID + 10));
    fprintf(outfile, "\nTerminal 2%25.3f%27.3f%29.3f", transfer[1], transfer[3], transfer[4]);
}

int main() /* Main function. */
{
    /* Open output files. */

    outfile = fopen("carrental.out", "w");

    /* Write report heading and input parameters. */

    fprintf(outfile, "Car Rental Air Terminals model\n\n");

    /* Initialize simlib */

    init_simlib();

    /* Set maxatr = max(maximum number of attributes per record, 4) */

    maxatr = 4; /* NEVER SET maxatr TO BE SMALLER THAN 4. */

    /* Schedule arrival of the bus to the car rental. */

    event_schedule(0.0, EVENT_BUS_ARRIVAL);

    /* Schedule arrival of the first person to the car rental and terminals. */
    double test_interval = expon(1.0 / rental_arrival_rate, STREAM_INTERARRIVAL_RENTAL);
    event_schedule(expon(1.0 / rental_arrival_rate, STREAM_INTERARRIVAL_RENTAL), EVENT_PERSON_ARRIVAL_RENTAL);
    event_schedule(expon(1.0 / terminal_1_arrival_rate, STREAM_INTERARRIVAL_TERMINAL_1), EVENT_PERSON_ARRIVAL_TERMINAL_1);
    event_schedule(expon(1.0 / terminal_2_arrival_rate, STREAM_INTERARRIVAL_TERMINAL_2), EVENT_PERSON_ARRIVAL_TERMINAL_2);

    /* Schedule the end of the simulation.  (This is needed for consistency of
       units.) */

    event_schedule(length_simulation, EVENT_END_SIMULATION);

    /* Run the simulation until it terminates after an end-simulation event
       (type EVENT_END_SIMULATION) occurs. */

    do {

        /* Determine the next event. */

        timing();

        /* Invoke the appropriate event function. */
        // print lists and states for debugging, delete later
        // printf("------------------------------------------------------------\n");
        // printf("Time: %f, Next event type: %d\n", sim_time, next_event_type);
        // printf("Queue Lengths: Rental - %d, Terminal 1 - %d, Terminal 2 - %d, Bus - %d\n", list_size[RENTAL_ID], list_size[TERMINAL_1_ID], list_size[TERMINAL_2_ID], list_size[BUS_ID]);
        // printf("Bus location: %d\n", current_bus_location);
        // printf("Bus arrived: %d\n", bus_arrived);
        // printf("Last bus arrive time: %f\n", last_bus_arrive_time);

        switch (next_event_type) {
        case EVENT_PERSON_ARRIVAL_RENTAL:
            // printf("Person arrival at rental\n"); // Debugging delete later
            person_arrive(RENTAL_ID);
            break;
        case EVENT_PERSON_ARRIVAL_TERMINAL_1:
            // printf("Person arrival at terminal 1\n"); // Debugging delete later
            person_arrive(TERMINAL_1_ID);
            break;
        case EVENT_PERSON_ARRIVAL_TERMINAL_2:
            // printf("Person arrival at terminal 2\n"); // Debugging delete later
            person_arrive(TERMINAL_2_ID);
            break;
        case EVENT_BUS_ARRIVAL:
            // printf("Bus arrival at location %d\n", current_bus_location); // Debugging delete later
            bus_arrive(current_bus_location);
            break;
        case EVENT_BUS_DEPARTURE:
            // printf("Bus departure from location %d\n", current_bus_location); // Debugging delete later
            bus_depart(current_bus_location);
            break;
        case EVENT_UNLOAD_PERSON:
            // printf("Person unloading at location %d\n", current_bus_location); // Debugging delete later
            person_unload(current_bus_location);
            break;
        case EVENT_LOAD_PERSON:
            // printf("Person loading at location %d\n", current_bus_location); // Debugging delete later
            person_load(current_bus_location);
            break;
        case EVENT_END_SIMULATION:
            report();
            break;
        }

        /* If the event just executed was not the end-simulation event (type
           EVENT_END_SIMULATION), continue simulating.  Otherwise, end the
           simulation. */

    } while (next_event_type != EVENT_END_SIMULATION);

    fclose(outfile);

    return 0;
}
