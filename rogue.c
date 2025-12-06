// rogue.c
// Rogue: handles traps AND treasure/semaphore logic.
#define _DEFAULT_SOURCE // enables POSIX extension 

#include <stdio.h> // libraries for printf()
#include <stdlib.h> // exit_failure 
#include <fcntl.h>// flags 
#include <sys/mman.h>// shared memory
#include <sys/stat.h>// mode constants 
#include <signal.h>// signal handling 
#include <unistd.h>// memcpy()
#include <string.h> // semaphores
#include <semaphore.h>

#include "dungeon_info.h"  //shared dungeon and semaphore names 
#include "dungeon_settings.h" // gameplay values and signal settings 

#ifndef DUNGEON_SIGNAL //dungeon uses it to send trap updates 
#define DUNGEON_SIGNAL   SIGUSR1   // regular dungeon ping (traps)
#endif

#ifndef SEMAPHORE_SIGNAL  
#define SEMAPHORE_SIGNAL SIGUSR2   // treasure room / semaphor signal
#endif

static struct Dungeon *dungeon = NULL; // pointer to the shared dungeon struct

// Flags set by signal handler:
static volatile sig_atomic_t got_dungeon_signal   = 0;
static volatile sig_atomic_t got_semaphore_signal = 0;

static void pick_lock(void) {
    if (!dungeon) return; // shared memory must be valid 

    while (dungeon->running && dungeon->trap.locked) { 
        float pick = dungeon->rogue.pick; // read current pick angle 

        // Move the pick forward by 1 each iteration, wrap at MAX_PICK_ANGLE
        pick += 1.0f;// advance pick by 1 degree 
        if (pick > (float)MAX_PICK_ANGLE) { // wrap around if exceeds max angle 
            pick = 0.0f;
        }

        dungeon->rogue.pick = pick; // write updated pick angle into shared memory 

        // Let the dungeon see the update
        usleep(TIME_BETWEEN_ROGUE_TICKS / 2);
    }
}

static void handle_treasure(void) { 
    if (!dungeon) return; // safety check 

    printf("[Rogue] Starting treasure collection...\n");
    fflush(stdout);

    // Give the dungeon time to fully populate the treasure.
    // Barbarian holds the door open for TIME_TREASURE_AVAILABLE seconds.
    // We wait until near the end, then copy all 4 chars at once.
    if (TIME_TREASURE_AVAILABLE > 2) {
        sleep(TIME_TREASURE_AVAILABLE - 2);  // e.g., 8 seconds if TIME_TREASURE_AVAILABLE = 10
    }

    // Copy all 4 treasure bytes into spoil buffer 
    memcpy(dungeon->spoils, dungeon->treasure, sizeof(dungeon->spoils));

// debug printing 
    printf("[Rogue] Final treasure values:\n");
    for (int i = 0; i < 4; ++i) {
        char c = dungeon->spoils[i];
        printf("  spoils[%d] = '%c'\n", i, c ? c : ' ');
    }
// print the final 4 character
    printf("[Rogue] Finished treasure: %c%c%c%c\n",
           dungeon->spoils[0],
           dungeon->spoils[1],
           dungeon->spoils[2],
           dungeon->spoils[3]);
    fflush(stdout);
}

//signla handler 
static void rogue_handler(int sig) {
    if (sig == DUNGEON_SIGNAL) {
        got_dungeon_signal = 1; // trap update signal 
    } else if (sig == SEMAPHORE_SIGNAL) {
        got_semaphore_signal = 1; // treasure signal 
    }
}

int main(void) {
    // Attach to shared memory created by the dungeon/game.
    int fd = -1;
    for (int tries = 0; tries < 50 && fd == -1; ++tries) {
        fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
        if (fd == -1) {
            usleep(100000); // 0.1 s
        }
    }
    if (fd == -1) {// failed after retries 
        perror("rogue shm_open");
        return EXIT_FAILURE;
    }
// map the shared memory into this process 
    dungeon = mmap(NULL, sizeof(*dungeon),
                   PROT_READ | PROT_WRITE, // need both read and write 
                   MAP_SHARED, // share updates with other processes 
                   fd,
                   0);
    close(fd);
//check map succeded 
    if (dungeon == MAP_FAILED) {
        perror("rogue mmap");
        return EXIT_FAILURE;
    }

    // Set up signal handlers for both dungeon and semaphore signals.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = rogue_handler; // the function we call 
    sigemptyset(&sa.sa_mask); // do no block other signals 
    sa.sa_flags = 0; // default behavious 

// handles dungeon trap signals 
    if (sigaction(DUNGEON_SIGNAL, &sa, NULL) == -1) {
        perror("rogue sigaction(DUNGEON_SIGNAL)");
    }
    if (sigaction(SEMAPHORE_SIGNAL, &sa, NULL) == -1) {
        perror("rogue sigaction(SEMAPHORE_SIGNAL)");
    }

    // Main loop: respond to signals until dungeon stops running.
    while (dungeon->running) {
        pause();  // sleep until signal arrives

        if (!dungeon->running) {
            break; // dungeon is shutting down 
        }

        if (got_dungeon_signal) {
            got_dungeon_signal = 0;
            if (dungeon->trap.locked) {
                pick_lock();
            }
        }
// if a treasure signal arrives 
        if (got_semaphore_signal) {
            got_semaphore_signal = 0;
            handle_treasure();
        }
    }
// cleanup unmap shared memory before existing 
    munmap(dungeon, sizeof(*dungeon));
    return EXIT_SUCCESS;
}
