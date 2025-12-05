// rogue.c
// Rogue: pick locks during dungeon rounds, then pull semaphores
// for the treasure room when signaled.

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

// We’ll handle both DUNGEON_SIGNAL (normal trap rounds)
// and SEMAPHORE_SIGNAL (treasure room).
// If your instructor gave a different ROGUE_SIGNAL, we still
// install handlers for SIGUSR1 and SIGUSR2 explicitly.

static struct Dungeon *dungeon = NULL;
static volatile sig_atomic_t last_sig = 0;

// ------------------ Lock picking logic ------------------

// Sweep the pick through [0, 100] in steps until trap unlocks.
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

// ------------------ Treasure room / semaphores ------------------

// When the dungeon sends SEMAPHORE_SIGNAL, the treasure door is open.
// Down both levers, copy the treasure, then release the levers.
static void handle_treasure_room(void) {
    if (!dungeon) return;

    // Open existing semaphores created in game.c
    sem_t *lever1 = sem_open(dungeon_lever_one, 0);
    sem_t *lever2 = sem_open(dungeon_lever_two, 0);

    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("rogue: sem_open");
        return;
    }

    // Down both levers (sem_wait -> value goes to 0)
    if (sem_wait(lever1) == -1) {
        perror("rogue: sem_wait lever1");
    }
    if (sem_wait(lever2) == -1) {
        perror("rogue: sem_wait lever2");
    }

    // Copy treasure out while door is "open"
    memcpy(dungeon->spoils, dungeon->treasure, sizeof(dungeon->spoils));

    // Release the levers (post back up) before time expires
    if (sem_post(lever1) == -1) {
        perror("rogue: sem_post lever1");
    }
    if (sem_post(lever2) == -1) {
        perror("rogue: sem_post lever2");
    }

    sem_close(lever1);
    sem_close(lever2);
}

// ------------------ Signal handler ------------------

static void rogue_handler(int sig) {
    last_sig = sig;   // Just record which signal we got
}

// ------------------ main ------------------

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

    // Set up signal handlers.
    struct sigaction sa;
    sa.sa_handler = rogue_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Normal dungeon signal for trap rounds
    if (sigaction(DUNGEON_SIGNAL, &sa, NULL) == -1) {
        perror("rogue sigaction(DUNGEON_SIGNAL)");
    }
    // Semaphore signal for treasure room
    if (sigaction(SEMAPHORE_SIGNAL, &sa, NULL) == -1) {
        perror("rogue sigaction(SEMAPHORE_SIGNAL)");
    }
    // Handle SIGTERM so we can exit cleanly when game is done
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("rogue sigaction(SIGTERM)");
    }

    // Main loop
    while (dungeon->running) {
        pause();  // wait for a signal

        if (!dungeon->running) {
            break;
        }

        if (last_sig == DUNGEON_SIGNAL) {
            // Trap rounds
            if (dungeon->trap.locked) {
                pick_lock();
            }
        } else if (last_sig == SEMAPHORE_SIGNAL) {
            // Treasure room logic (semaphores + copying treasure)
            handle_treasure_room();
        } else if (last_sig == SIGTERM) {
            // Game told us to die
            break;
        }
    }

    munmap(dungeon, sizeof(*dungeon));
    return EXIT_SUCCESS;
}

