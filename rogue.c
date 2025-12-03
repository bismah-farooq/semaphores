// rogue.c
// Rogue does a binary search on pick angle to open the lock.

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "dungeon_info.h"
#include "dungeon_settings.h"

#ifndef ROGUE_SIGNAL
#define ROGUE_SIGNAL     SIGTERM
#endif
#ifndef SEMAPHORE_SIGNAL
#define SEMAPHORE_SIGNAL SIGINT
#endif

static volatile sig_atomic_t got_signal = 0;

static void rogue_handle(int s) {
    (void)s;
    got_signal = 1;  // mainly to avoid crashing; we also poll
}

int main(void) {
    int fd = -1;
    for (int tries = 0; tries < 50 && fd == -1; ++tries) {
        fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
        if (fd == -1) usleep(100000);
    }
    if (fd == -1) {
        fprintf(stderr, "rogue: could not open /DungeonMem. run game first.\n");
        return 1;
    }

    struct Dungeon *d = mmap(NULL, sizeof(*d),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             fd,
                             0);
    close(fd);
    if (d == MAP_FAILED) {
        perror("rogue: mmap");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = rogue_handle;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(ROGUE_SIGNAL, &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    printf("rogue ready (pid=%d)\n", getpid());
    fflush(stdout);

    // State for current trap session
    bool in_session = false;
    double low = 0.0, high = 0.0;
    double pick = 0.0;

    while (d->running) {
        // wait a tiny bit between polls to be nice
        usleep(TIME_BETWEEN_ROGUE_TICKS);

        // If there is no active trap, reset and wait
        if (!d->trap.locked) {
            in_session = false;
            continue;
        }

        // New trap session: initialize search range
        if (!in_session) {
            in_session = true;
            low = 0.0;
            high = MAX_PICK_ANGLE;
            pick = (low + high) / 2.0;
            d->rogue.pick = (float)pick;
            // let the dungeon react
            // printf("rogue: starting trap, pick=%.3f\n", pick);
            continue;
        }

        // Now respond to dungeon's direction hints
        char dir = d->trap.direction;

        if (dir == 'u') {
            // need to go higher
            low = pick;
            pick = (low + high) / 2.0;
            d->rogue.pick = (float)pick;
            // printf("rogue: dir=u, pick=%.3f\n", pick);
        } else if (dir == 'd') {
            // need to go lower
            high = pick;
            pick = (low + high) / 2.0;
            d->rogue.pick = (float)pick;
            // printf("rogue: dir=d, pick=%.3f\n", pick);
        } else if (dir == '-') {
            // success!
            printf("rogue: lock opened at pick=%.3f\n", pick);
            d->trap.locked = false;
            in_session = false;
        } else {
            // 'w' or 't' or whatever: dungeon is thinking, just wait
            continue;
        }
    }

    munmap(d, sizeof(*d));
    return 0;
}

