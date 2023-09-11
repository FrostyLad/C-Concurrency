// --------------------------------------------------
// ---   159.341 Assignment 2 - Lift Simulator - PARTIAL SOLUTION    ---
// --------------------------------------------------
// Written by M. J. Johnson
// Updated by D. P. Playne - 2019
/** SUBMITTED BY BRADEN JOHNSTON 20005898**/
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lift.h"

// --------------------------------------------------
// Define Problem Size
// --------------------------------------------------
#define NLIFTS 4          // The number of lifts in the building
#define NFLOORS 20        // The number of floors in the building
#define NPEOPLE 20        // The number of people in the building
#define MAXNOINLIFT 10    // Maximum number of people in a lift


// --------------------------------------------------
// Define delay times (in milliseconds)
// --------------------------------------------------
#define SLOW
//#define FAST

#if defined(SLOW)
	#define LIFTSPEED 50      // The time it takes for the lift to move one floor
	#define GETINSPEED 50     // The time it takes to get into the lift
	#define GETOUTSPEED 50    // The time it takes to get out of the lift
	#define PEOPLESPEED 100   // The maximum time a person spends on a floor
#elif defined(FAST)
	#define LIFTSPEED 0       // The time it takes for the lift to move one floor
	#define GETINSPEED 0      // The time it takes to get into the lift
	#define GETOUTSPEED 0     // The time it takes to get out of the lift
	#define PEOPLESPEED 1     // The maximum time a person spends on a floor
#endif

// --------------------------------------------------
// Define lift directions (UP/DOWN)
// --------------------------------------------------
#define UP 1
#define DOWN (-1)

// --------------------------------------------------
// Information about a lift
// --------------------------------------------------
typedef struct {
    int no;                       // The lift number (id)
    int position;                 // The floor it is on
    int direction;                // Which direction it is going (UP/DOWN)
    int peopleinlift;             // The number of people in the lift
    int stops[NFLOORS];           // How many people are going to each floor
    semaphore stopsem[NFLOORS];   // People in the lift wait on one of these
} lift_info;

// --------------------------------------------------
// Information about a floor in the building
// --------------------------------------------------
typedef struct {
	int waitingtogoup;      // The number of people waiting to go up
	int waitingtogodown;    // The number of people waiting to go down
	semaphore up_arrow;     // People going up wait on this
	semaphore down_arrow;   // People going down wait on this
    bool waitinglifts[NLIFTS];
} floor_info;

// --------------------------------------------------
// Some global variables
// --------------------------------------------------
floor_info floors[NFLOORS];
lift_info *lifts[NLIFTS];
semaphore print_sem;
semaphore waiting_sem;
semaphore waitingup_sem;
semaphore waitingdown_sem;
semaphore peopleinlift_sem;
semaphore waitforfloor_sem;
semaphore check_buttonpress;
semaphore lift_editing;

// --------------------------------------------------
// Print a string on the screen at position (x,y)
// --------------------------------------------------
void print_at_xy(int x, int y, const char *s) {

    semaphore_wait(&print_sem);

	// Move cursor to (x,y)
	gotoxy(x,y);
	
	// Slow things down
	Sleep(1);

	// Print the string
	printf("%s", s);
	
	// Move cursor out of the way
	gotoxy(42, NFLOORS+2);

    semaphore_signal(&print_sem);

}

// --------------------------------------------------
// Function for a lift to pick people waiting on a
// floor going in a certain direction
// --------------------------------------------------
void get_into_lift(lift_info *lift, int direction) {
	// Local variables
	int *waiting;
	semaphore *s;

	// Check lift direction
	if(direction==UP) {
		// Use up_arrow semaphore
		s = &floors[lift->position].up_arrow;
        semaphore_wait(&waitingup_sem);
		// Number of people waiting to go up
		waiting = &floors[lift->position].waitingtogoup;
        semaphore_signal(&waitingup_sem);
	} else {
		// Use down_arrow semaphore
		s = &floors[lift->position].down_arrow;

        semaphore_wait(&waitingdown_sem);
		// Number of people waiting to go down
		waiting = &floors[lift->position].waitingtogodown;
        semaphore_signal(&waitingdown_sem);
	}

	// For all the people waiting
    while (true) {
        lift = lifts[lift->no];
        semaphore_wait(&waiting_sem);
        if (*waiting == 0) {
            semaphore_signal(&waiting_sem);
            break;
        }
        // Check if lift is empty
        if (lift->peopleinlift == 0) {
            // Set the direction of the lift
            lift->direction = direction;
        }
        // Check there are people waiting and lift isn't full
        if (lift->peopleinlift < MAXNOINLIFT && *waiting) {
            // Add person to the lift
            semaphore_wait(&peopleinlift_sem);
            semaphore_wait(&lift_editing);
            lift->peopleinlift++;
            lifts[lift->no] = lift;
            semaphore_signal(&peopleinlift_sem);
            semaphore_signal(&lift_editing);

            // Erase the person from the waiting queue
            print_at_xy(NLIFTS*4+floors[lift->position].waitingtogodown + floors[lift->position].waitingtogoup, NFLOORS-lift->position, " ");

            // One less person waiting
            (*waiting)--;
            floors[lift->position].waitinglifts[lift->no] = true;
            semaphore_signal(&waiting_sem);

            // Wait for person to get into lift
            Sleep(GETINSPEED);

            // Signal passenger to enter
            semaphore_signal(s);
        } else {
            semaphore_signal(&peopleinlift_sem);
            break;
        }
    }
}

// --------------------------------------------------
// Function for the Lift Threads
// --------------------------------------------------
void* lift_thread(void *p) {
	// Local variables
    lift_info* lift = malloc(sizeof(lift_info));
	unsigned long long int no = (unsigned long long int)p;
	int i;
	
	// Set up Lift
	lift->no = no;           // Set lift number
	lift->position = 0;      // Lift starts on ground floor
	lift->direction = UP;    // Lift starts going up
	lift->peopleinlift = 0;  // Lift starts empty
    lifts[no] = lift;

	for(i = 0; i < NFLOORS; i++) {
		lift->stops[i]=0;                        // No passengers waiting
		semaphore_create(&lift->stopsem[i], 0);  // Initialise semaphores
	}

	// Randomise lift
	randomise();

	// Wait for random start up time (up to a second)
	Sleep(rnd(1000));

	// Loop forever
	while(TRUE) {
        semaphore_wait(&lift_editing);
        lift = lifts[no];
        semaphore_signal(&lift_editing);
		// Print current position of the lift
		print_at_xy(no*4+1, NFLOORS-lift->position, lf);

		// Wait for a while
		Sleep(LIFTSPEED);

		// Drop off passengers on this floor
		while (lift->stops[lift->position] != 0) {

            semaphore_wait(&peopleinlift_sem);
            semaphore_wait(&lift_editing);
            // One less passenger in lift
			lift->peopleinlift--;
            // One less waiting to get off at this floor
            lift->stops[lift->position]--;
            lifts[no] = lift;
            semaphore_signal(&peopleinlift_sem);
            semaphore_signal(&lift_editing);

			// Wait for exit lift delay
			Sleep(GETOUTSPEED);

			// Signal passenger to leave lift
            semaphore_signal(&lift->stopsem[lift->position]);
			// Check if that was the last passenger waiting for this floor
			if(!lift->stops[lift->position]) {
				// Clear the "-"
				print_at_xy(no*4+1+2, NFLOORS-lift->position, " ");
			}
		}
		// Check if lift is going up or is empty
        semaphore_wait(&peopleinlift_sem);
		if(lift->direction==UP || !lift->peopleinlift) {
            semaphore_signal(&peopleinlift_sem);
			// Pick up passengers waiting to go up
			get_into_lift(lift, UP);
            floors[lift->position].waitinglifts[no] = false;
		}
        else {
            semaphore_signal(&peopleinlift_sem);
        }
		// Check if lift is going down or is empty
        semaphore_wait(&peopleinlift_sem);
		if(lift->direction==DOWN || !lift->peopleinlift) {
            semaphore_signal(&peopleinlift_sem);
			// Pick up passengers waiting to go down
			get_into_lift(lift, DOWN);
            floors[lift->position].waitinglifts[no] = false;
		}
        else {
            semaphore_signal(&peopleinlift_sem);
        }

		// Erase lift from screen
		print_at_xy(no*4+1, NFLOORS-lift->position, (lift->direction + 1 ? " " : lc));

		// Move lift
		lift->position += lift->direction;

		// Check if lift is at top or bottom
		if(lift->position == 0 || lift->position == NFLOORS-1) {
			// Change lift direction
			lift->direction = -lift->direction;
		}
        semaphore_wait(&lift_editing);
        lifts[no] = lift;
        semaphore_signal(&lift_editing);
	}
	
	return NULL;
}

// --------------------------------------------------
// Function for the Person Threads
// --------------------------------------------------
void* person_thread(void *p) {
	// Local variables
	int from = 0, to; // Start on the ground floor
	lift_info lift;
    int direction;
    unsigned long long int no = (unsigned long long int)p;

	// Randomise
	randomise();

	// Stay in the building forever
	while(1) {
		// Work for a while
		Sleep(rnd(PEOPLESPEED));
		do {
			// Randomly pick another floor to go to
			to = rnd(NFLOORS);
		} while(to == from);

		// Check which direction the person is going (UP/DOWN)
		if(to > from) {
            direction = UP;
			// One more person waiting to go up
            semaphore_wait(&waitingup_sem);
			floors[from].waitingtogoup++;
            semaphore_signal(&waitingup_sem);
			// Print person waiting
			print_at_xy(NLIFTS*4+ floors[from].waitingtogoup +floors[from].waitingtogodown,NFLOORS-from, pr);
			
			// Wait for a lift to arrive (going up)
            semaphore_wait(&floors[from].up_arrow);
		} else {
            direction = DOWN;
			// One more person waiting to go down
            semaphore_wait(&waitingdown_sem);
			floors[from].waitingtogodown++;
            semaphore_signal(&waitingdown_sem);
			// Print person waiting
			print_at_xy(NLIFTS*4+floors[from].waitingtogodown+floors[from].waitingtogoup,NFLOORS-from, pr);
			
			// Wait for a lift to arrive (going down)
            semaphore_wait(&floors[from].down_arrow);
		}

        // Which lift we are getting into
        int i, liftno;
        for(i = 0; i < NLIFTS; i++) {
            if(lifts[i]->position == from && lifts[i]->direction == direction) {
                lift = *lifts[i];
                liftno = lift.no;
                break;
            }
        }

        semaphore_wait(&waitforfloor_sem);
        semaphore_wait(&lift_editing);
        lift.stops[to]++;
        lifts[liftno] = &lift;
        semaphore_signal(&lift_editing);

        // Press button if we are the first
        if(lift.stops[to]==1) {
            // Print light for destination
            print_at_xy(lift.no*4+1+2, NFLOORS-to, "-");
        }
        semaphore_signal(&waitforfloor_sem);

        // Wait until we are at the right floor
        semaphore_wait(&lift.stopsem[to]);
/**
        if (lift.position==to) {
            // Exit the lift
            from = to;
        }
        else {
            semaphore_wait(&lift_editing);
            lift = *lifts[liftno];
            lift.peopleinlift++;
            lift.stops[to]++;
            lifts[liftno] = &lift;
            semaphore_signal(&lift_editing);
            semaphore_wait(&lift.stopsem[to]);
        }
        **/
    }
    return NULL;
}

// --------------------------------------------------
//	Print the building on the screen
// --------------------------------------------------
void printbuilding(void) {

	// Local variables
	int l,f;

	// Clear Screen
	system(clear_screen);
	
	// Print Roof
	printf("%s", tl);
	for(l = 0; l < NLIFTS-1; l++) {
		printf("%s%s%s%s", hl, td, hl, td);
	}
	printf("%s%s%s%s\n", hl, td, hl, tr);

	// Print Floors and Lifts
	for(f = NFLOORS-1; f >= 0; f--) {
		for(l = 0; l < NLIFTS; l++) {
			printf("%s%s%s ", vl, lc, vl);
			if(l == NLIFTS-1) {
				printf("%s\n", vl);
			}
		}
	}

	// Print Ground
	printf("%s", bl);
	for(l = 0; l < NLIFTS-1; l++) {
		printf("%s%s%s%s", hl, tu, hl, tu);
	}
	printf("%s%s%s%s\n", hl, tu, hl, br);

	// Print Message
	printf("\n\nLift Simulation - Press CTRL-Break to exit\n");

}

// --------------------------------------------------
// Main starts the threads and then waits.
// --------------------------------------------------
int main() {


	// Local variables
	unsigned long long int i, n;

	// Initialise Building
	for(i = 0; i < NFLOORS; i++) {
		// Initialise Floor
		floors[i].waitingtogoup = 0;
		floors[i].waitingtogodown = 0;
        for (n = 0; n < NLIFTS; n++) {floors[i].waitinglifts[n] = false;}
		semaphore_create(&floors[i].up_arrow, 0);
		semaphore_create(&floors[i].down_arrow, 0);
	}

	// --- Initialise any other semaphores ---
    semaphore_create(&print_sem, 1);
    semaphore_create(&waiting_sem, 1);
    semaphore_create(&waitingup_sem, 1);
    semaphore_create(&waitingdown_sem, 1);
    semaphore_create(&peopleinlift_sem, 1);
    semaphore_create(&lift_editing, 1);
    semaphore_create(&waitforfloor_sem, 1);
    semaphore_create(&check_buttonpress, 1);
	// Print Building
	printbuilding();

	// Create Lifts
	for(i = 0; i < NLIFTS; i++) {
		// Create Lift Thread
		create_thread(lift_thread, (void*)i);
	}

	// Create People
	for(i = 0; i < NPEOPLE; i++) {
		// Create Person Thread
		create_thread(person_thread, (void*)i);
	}

	// Go to sleep for 86400 seconds (one day)
	Sleep(86400000ULL);
}
