// barbarian.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "dungeon_info.h"
#include "dungeon_settings.h"

static volatile sig_atomic_t go = 0;

// signal handler
void handle_signal(int sig) {
    (void)sig;
    go = 1;
}

int main(void) {
    // open shared memory (created by game)
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open failed");
        exit(1);
    }

    struct Dungeon *d = mmap(NULL, sizeof(struct Dungeon),
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (d == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    // set up signal handler
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(BARBARIAN_SIGNAL, &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL); // don’t crash if dungeon sends this

    printf("Barbarian ready! PID=%d\n", getpid());

    // main loop
    while (d->running) {
        if (!go) {
            usleep(1000); // wait for signal
            continue;
        }
        go = 0;

        // attack logic
        d->barbarian.attack = d->enemy.health;
        sleep(SECONDS_TO_ATTACK); // wait duration before dungeon checks
    }

    munmap(d, sizeof(struct Dungeon));
    return 0;
}

