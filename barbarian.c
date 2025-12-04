// barbarian.c
// Barbarian copies enemy health into attack when signaled
// and pulls the treasure levers using semaphores.

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <semaphore.h>   // <-- semaphores

#include "dungeon_info.h"
#include "dungeon_settings.h"

// Fallbacks if dungeon_settings.h doesn't define them:
#ifndef BARBARIAN_SIGNAL
#define BARBARIAN_SIGNAL    SIGUSR1
#endif
#ifndef SEMAPHORE_SIGNAL
#define SEMAPHORE_SIGNAL    SIGINT
#endif

static volatile sig_atomic_t g_go        = 0;  // "do attack" flag
static volatile sig_atomic_t g_do_levers = 0;  // "pull levers" flag

// simple signal handler
static void handle_signal(int sig) {
    // For compatibility with your original working version:
    // ANY signal we care about will cause an attack.
    (void)sig;
    g_go = 1;

    // Specifically remember when the semaphore signal arrives
    if (sig == SEMAPHORE_SIGNAL) {
        g_do_levers = 1;
    }
}

int main(void) {
    // Try to open shared memory created by game
    int fd = -1;
    for (int tries = 0; tries < 50 && fd == -1; ++tries) {
        fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
        if (fd == -1) {
            usleep(100000);   // 0.1 s
        }
    }
    if (fd == -1) {
        fprintf(stderr, "barbarian: could not open /DungeonMem. run game first.\n");
        return 1;
    }

    struct Dungeon *d = mmap(NULL, sizeof(*d),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             fd,
                             0);
    close(fd);
    if (d == MAP_FAILED) {
        perror("barbarian: mmap");
        return 1;
    }

    // Set up sigaction
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(BARBARIAN_SIGNAL, &sa, NULL) == -1) {
        perror("barbarian: sigaction BARBARIAN_SIGNAL");
    }
    if (sigaction(SEMAPHORE_SIGNAL, &sa, NULL) == -1) {
        // just don't crash if dungeon sends this
        perror("barbarian: sigaction SEMAPHORE_SIGNAL");
    }

    printf("barbarian ready (pid=%d)\n", getpid());
    fflush(stdout);

    bool levers_done = false;

    // Main loop
    while (d->running) {
        if (g_go) {
            g_go = 0;

            // When signaled, copy enemy health into attack
            d->barbarian.attack = d->enemy.health;

            // Dungeon will wait SECONDS_TO_ATTACK and then compare
            sleep(SECONDS_TO_ATTACK);
        }

        // Handle lever semaphores once, when dungeon tells us to
        if (g_do_levers && !levers_done) {
            g_do_levers = 0;
            levers_done = true;

            // Try to open and down the first lever semaphore
            sem_t *lever1 = sem_open(dungeon_lever_one, 0);
            if (lever1 != SEM_FAILED) {
                sem_wait(lever1);   // pull lever one
                sem_close(lever1);
            } else {
                perror("barbarian: sem_open lever one");
            }

            // Try to open and down the second lever semaphore
            sem_t *lever2 = sem_open(dungeon_lever_two, 0);
            if (lever2 != SEM_FAILED) {
                sem_wait(lever2);   // pull lever two
                sem_close(lever2);
            } else {
                perror("barbarian: sem_open lever two");
            }
        }

        usleep(1000); // 1ms; avoid busy spin
    }

    munmap(d, sizeof(*d));
    return 0;
}

