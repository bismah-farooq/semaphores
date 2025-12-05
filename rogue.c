// rogue.c
// Rogue: systematically sweep the pick across 0..100 until the trap unlocks.

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "dungeon_info.h"
#include "dungeon_settings.h"

// If dungeon_settings.h defines this, fine; otherwise default:
#ifndef ROGUE_SIGNAL
#define ROGUE_SIGNAL SIGUSR2
#endif

static struct Dungeon *dungeon = NULL;

// Sweep the pick through [0, 100] in steps until dungeon->trap.locked becomes false.
static void pick_lock(void) {
    if (!dungeon) return;

    while (dungeon->running && dungeon->trap.locked) {
        float pick = dungeon->rogue.pick;

        // Move the pick forward by 1 each iteration, wrap at 100 -> 0
        pick += 1.0f;
        if (pick > 100.0f) {
            pick = 0.0f;
        }

        dungeon->rogue.pick = pick;

        // Let the dungeon see the update
        usleep(TIME_BETWEEN_ROGUE_TICKS / 2);
    }
}

// Simple handler: just wake up pause()
static void rogue_handler(int sig) {
    (void)sig; // same response for all signals we install
    // pause() will return after a signal is caught
}

int main(void) {
    // Attach to shared memory created by the game/dungeon.
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

    // Set up signal handler for the rogue.
    struct sigaction sa;
    sa.sa_handler = rogue_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Install for the macro-defined signal…
    if (sigaction(ROGUE_SIGNAL, &sa, NULL) == -1) {
        perror("rogue sigaction(ROGUE_SIGNAL)");
    }
    // …and *also* for SIGUSR1 and SIGUSR2, in case the dungeon uses either.
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("rogue sigaction(SIGUSR1)");
    }
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("rogue sigaction(SIGUSR2)");
    }

    // Main loop: wait to be pinged; when trap is locked, sweep until it unlocks.
    while (dungeon->running) {
        pause();  // wait for a signal from the dungeon

        // If the dungeon is done, copy treasure -> spoils safely and exit.
        if (!dungeon->running) {
            // Copy up to 3 chars and ensure null-termination
            dungeon->spoils[0] = dungeon->treasure[0];
            dungeon->spoils[1] = dungeon->treasure[1];
            dungeon->spoils[2] = dungeon->treasure[2];
            dungeon->spoils[3] = '\0';
            _exit(0);
        }

        if (dungeon->trap.locked) {
            pick_lock();
        }
    }

    // Fallback cleanup (normally we exit from inside the loop).
    munmap(dungeon, sizeof(*dungeon));
    return EXIT_SUCCESS;
}

