// rogue.c
// Rogue: handles traps AND treasure/semaphore logic.

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>

#include "dungeon_info.h"
#include "dungeon_settings.h"

// If these are changed in dungeon_settings.h, we use those values:
#ifndef DUNGEON_SIGNAL
#define DUNGEON_SIGNAL   SIGUSR1   // regular dungeon ping (traps)
#endif

#ifndef SEMAPHORE_SIGNAL
#define SEMAPHORE_SIGNAL SIGUSR2   // treasure room / semaphore signal
#endif

static struct Dungeon *dungeon = NULL;

// Flags set by signal handler:
static volatile sig_atomic_t got_dungeon_signal   = 0;
static volatile sig_atomic_t got_semaphore_signal = 0;

// ---------------------------------------------------------
// Trap logic: sweep roulette-style 0..MAX_PICK_ANGLE
// ---------------------------------------------------------
static void pick_lock(void) {
    if (!dungeon) return;

    while (dungeon->running && dungeon->trap.locked) {
        float pick = dungeon->rogue.pick;

        // Move the pick forward by 1 each iteration, wrap at MAX_PICK_ANGLE -> 0
        pick += 1.0f;
        if (pick > (float)MAX_PICK_ANGLE) {
            pick = 0.0f;
        }

        dungeon->rogue.pick = pick;

        // Let the dungeon see the update
        usleep(TIME_BETWEEN_ROGUE_TICKS / 2);
    }
}

// ---------------------------------------------------------
// Treasure / semaphore logic
// ---------------------------------------------------------
static void handle_treasure_and_semaphores(void) {
    if (!dungeon) return;

    // Open the two lever semaphores created by the dungeon/game.
    sem_t *lever1 = sem_open(dungeon_lever_one, 0);
    sem_t *lever2 = sem_open(dungeon_lever_two, 0);

    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("rogue: sem_open");
        if (lever1 != SEM_FAILED && lever1 != NULL) sem_close(lever1);
        if (lever2 != SEM_FAILED && lever2 != NULL) sem_close(lever2);
        return;
    }

    // "Down" both levers (wait). Initial value is 1, so this should be quick.
    if (sem_wait(lever1) == -1) {
        perror("rogue: sem_wait lever1");
    }
    if (sem_wait(lever2) == -1) {
        perror("rogue: sem_wait lever2");
    }

    // Copy the treasure into spoils so the dungeon can score and print it.
    memcpy(dungeon->spoils, dungeon->treasure, sizeof(dungeon->spoils));

    // Allow the door to close again by posting both levers
    if (sem_post(lever1) == -1) {
        perror("rogue: sem_post lever1");
    }
    if (sem_post(lever2) == -1) {
        perror("rogue: sem_post lever2");
    }

    sem_close(lever1);
    sem_close(lever2);
}

// ---------------------------------------------------------
// Signal handler
// ---------------------------------------------------------
static void rogue_handler(int sig) {
    if (sig == DUNGEON_SIGNAL) {
        got_dungeon_signal = 1;
    } else if (sig == SEMAPHORE_SIGNAL) {
        got_semaphore_signal = 1;
    }
    // pause() will return after this
}

// ---------------------------------------------------------
// main
// ---------------------------------------------------------
int main(void) {
    // Attach to shared memory created by the dungeon/game.
    int fd = -1;
    for (int tries = 0; tries < 50 && fd == -1; ++tries) {
        fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
        if (fd == -1) {
            usleep(100000); // 0.1 s
        }
    }
    if (fd == -1) {
        perror("rogue shm_open");
        return EXIT_FAILURE;
    }

    dungeon = mmap(NULL, sizeof(*dungeon),
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   fd,
                   0);
    close(fd);

    if (dungeon == MAP_FAILED) {
        perror("rogue mmap");
        return EXIT_FAILURE;
    }

    // Set up signal handlers for both dungeon and semaphore signals.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = rogue_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(DUNGEON_SIGNAL, &sa, NULL) == -1) {
        perror("rogue sigaction(DUNGEON_SIGNAL)");
    }
    if (sigaction(SEMAPHORE_SIGNAL, &sa, NULL) == -1) {
        perror("rogue sigaction(SEMAPHORE_SIGNAL)");
    }

    // Main loop: respond to signals until dungeon stops running.
    while (dungeon->running) {
        pause();  // sleep until *some* signal arrives

        if (!dungeon->running) {
            break;
        }

        if (got_dungeon_signal) {
            got_dungeon_signal = 0;
            if (dungeon->trap.locked) {
                pick_lock();
            }
        }

        if (got_semaphore_signal) {
            got_semaphore_signal = 0;
            handle_treasure_and_semaphores();
        }
    }

    munmap(dungeon, sizeof(*dungeon));
    return EXIT_SUCCESS;
}

