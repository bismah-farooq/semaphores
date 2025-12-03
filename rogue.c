#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

#include "dungeon_info.h"

#ifndef ROGUE_SIGNAL
#define ROGUE_SIGNAL SIGUSR2
#endif

static struct Dungeon *dungeon = NULL;
// Move the rogue's pick toward the trap value in shared memory
static void pick_lock(void) {
    // trap.direction is a char; cast to unsigned char before using it as a number
    float target = (float)(unsigned char)dungeon->trap.direction;

    while (dungeon->running && dungeon->trap.locked) {
        float pick = dungeon->rogue.pick;

        // If we're close enough, snap to exact target and stop
        if (fabsf(pick - target) <= 0.5f) {
            dungeon->rogue.pick = target;
            break;
        }

        if (pick < target) {
            dungeon->rogue.pick = pick + 1.0f;
        } else {
            dungeon->rogue.pick = pick - 1.0f;
        }

        // give dungeon time to notice the update
        usleep(TIME_BETWEEN_ROGUE_TICKS / 2);
    }
}

// Signal handler called when the dungeon pings the rogue
static void rogue_handler(int sig) {
    if (dungeon == NULL) {
        return;
    }

    // Only react to the correct signal
    if (sig != ROGUE_SIGNAL) {
        return;
    }

    // If the dungeon is done, exit so success is registered
    if (!dungeon->running) {
        _exit(0);
    }

    // Work on the trap if it's still locked
    if (dungeon->trap.locked) {
        pick_lock();
    }
}

int main(void) {
    // Attach to shared memory created by the dungeon
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0);
    if (fd == -1) {
        perror("rogue shm_open");
        return EXIT_FAILURE;
    }

    dungeon = mmap(NULL, sizeof(*dungeon),
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   fd, 0);
    close(fd);

    if (dungeon == MAP_FAILED) {
        perror("rogue mmap");
        return EXIT_FAILURE;
    }

    // Set up signal handler for the rogue
    struct sigaction sa;
    sa.sa_handler = rogue_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(ROGUE_SIGNAL, &sa, NULL) == -1) {
        perror("rogue sigaction");
        return EXIT_FAILURE;
    }

    // Wait for the dungeon to ping us; exit when dungeon->running is false
    while (dungeon->running) {
        pause();
    }

    munmap(dungeon, sizeof(*dungeon));
    return EXIT_SUCCESS;
}

