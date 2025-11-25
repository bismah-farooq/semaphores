// rogue.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <math.h>
#include "dungeon_info.h"
#include "dungeon_settings.h"

#ifndef ROGUE_SIGNAL
#define ROGUE_SIGNAL     SIGTERM
#endif
#ifndef SEMAPHORE_SIGNAL
#define SEMAPHORE_SIGNAL SIGINT
#endif

static volatile sig_atomic_t go = 0;

static void handle_signal(int s) {
    (void)s;
    go = 1;
}

int main(void) {
    // open shared memory created by game
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
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (d == MAP_FAILED) {
        perror("rogue: mmap");
        return 1;
    }

    // set up signals
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(ROGUE_SIGNAL, &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    printf("rogue ready (pid=%d)\n", getpid());
    fflush(stdout);

    while (d->running) {
        if (!go) { usleep(1000); continue; }
        go = 0;

        // --- lock picking puzzle ---
        double low = 0.0;
        double high = MAX_PICK_ANGLE;  // from dungeon_settings.h
        double pick;
        double threshold = LOCK_THRESHOLD; // from dungeon_settings.h

        // mark that we’re starting
        d->trap.direction = 't';
        d->trap.locked = true;

        while (d->trap.locked && d->running) {
            pick = (low + high) / 2.0;
            d->rogue.pick = (float)pick;
            d->trap.direction = 't'; // tell dungeon we moved

            usleep(TIME_BETWEEN_ROGUE_TICKS * 2); // let dungeon update direction

            if (d->trap.direction == 'u') {
                low = pick; // need higher angle
            } else if (d->trap.direction == 'd') {
                high = pick; // need lower angle
            } else if (d->trap.direction == '-') {
                d->trap.locked = false;
                printf("rogue: lock opened!\n");
                break;
            }

            if (fabs(high - low) <= threshold) {
                printf("rogue: close enough, stopping search.\n");
                break;
            }
        }

        // after success, dungeon will move on to semaphore/treasure phase
        // we’ll just wait for that signal for now
    }

    munmap(d, sizeof(*d));
    return 0;
}
