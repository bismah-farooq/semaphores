// rogue.c
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include "dungeon_info.h"
#include "dungeon_settings.h"

#ifndef ROGUE_SIGNAL
#define ROGUE_SIGNAL SIGUSR2
#endif

static struct Dungeon *dungeon = NULL;

static void pick_lock(void) {
    if (!dungeon) return;

    while (dungeon->running && dungeon->trap.locked) {
        float pick = dungeon->rogue.pick;

        pick += 1.0f;
        if (pick > 100.0f) {
            pick = 0.0f;
        }

        dungeon->rogue.pick = pick;

        usleep(TIME_BETWEEN_ROGUE_TICKS / 2);
    }
}

static void rogue_handler(int sig) {
    (void)sig; 
}

int main(void) {
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

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("rogue sigaction(SIGUSR1)");
    }
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("rogue sigaction(SIGUSR2)");
    }

    while (dungeon->running) {
        pause();  

        if (!dungeon->running) {
            break; // dungeon is shutting down
        }

        if (dungeon->trap.locked) {
            pick_lock();
        }
    }

    munmap(dungeon, sizeof(*dungeon));
    return EXIT_SUCCESS;
}

