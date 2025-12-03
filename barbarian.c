// barbarian.c
// Barbarian copies enemy health into attack when signaled.

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

#include "dungeon_info.h"
#include "dungeon_settings.h"

// Fallbacks if dungeon_settings.h doesn't define them:
#ifndef BARBARIAN_SIGNAL
#define BARBARIAN_SIGNAL    SIGUSR1
#endif
#ifndef SEMAPHORE_SIGNAL
#define SEMAPHORE_SIGNAL    SIGINT
#endif

static volatile sig_atomic_t g_go = 0;

// simple signal handler: just set a flag
static void handle_signal(int sig) {
    (void)sig;
    g_go = 1;
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
        // we just don't want to crash if dungeon sends this
        perror("barbarian: sigaction SEMAPHORE_SIGNAL");
    }

    printf("barbarian ready (pid=%d)\n", getpid());
    fflush(stdout);

    // Main loop
    while (d->running) {
        if (!g_go) {
            usleep(1000); // 1ms; just spin until signaled
            continue;
        }
        g_go = 0;

        // When signaled, copy enemy health into attack
        d->barbarian.attack = d->enemy.health;

        // Dungeon will wait SECONDS_TO_ATTACK and then compare
        sleep(SECONDS_TO_ATTACK);
    }

    munmap(d, sizeof(*d));
    return 0;
}

