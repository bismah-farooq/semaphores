// barbarian.c
// Barbarian copies enemy health into attack when signaled and pulls the treasure levers using semaphores.

#define _DEFAULT_SOURCE // enables POSIX extensions 

#include <stdio.h> // printf()
#include <stdlib.h>// exit codes 
#include <unistd.h>//sleep()
#include <signal.h>//signal handling 
#include <sys/mman.h> // shared memory
#include <fcntl.h> // flags 
#include <sys/stat.h>//permission mkdes 
#include <stdbool.h>// bool type //memset()
#include <string.h>//POSIX semaphores 
#include <semaphore.h>   // <-- semaphores

#include "dungeon_info.h" //sundeon struct definition and semaphore names 
#include "dungeon_settings.h" // game settings and signal numbers 

// Fallbacks if dungeon_settings.h doesn't define them
#ifndef BARBARIAN_SIGNAL // the signal used for barbarian attack rounds 
#define BARBARIAN_SIGNAL    SIGUSR1
#endif
#ifndef SEMAPHORE_SIGNAL
#define SEMAPHORE_SIGNAL    SIGINT
#endif

static volatile sig_atomic_t g_go        = 0;  // "do attack" flag
static volatile sig_atomic_t g_do_levers = 0;  // "pull levers" flag

// signal handler
static void handle_signal(int sig) {
    (void)sig;
    g_go = 1; //it resets when any signal is recieved 

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
// map the shared memory struct into this process 
    struct Dungeon *d = mmap(NULL, sizeof(*d),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             fd,
                             0);
    close(fd); // done with file descriptor 
    if (d == MAP_FAILED) {
        perror("barbarian: mmap");
        return 1;
    }

    // Set up signla handlers for barbarian signal and semaphore signal 
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal; // function to call when signal arrive 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // restart syscalls if interupted by a signal 

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
        if (g_go) { // of barbarian signal arrivs 
            g_go = 0; // reset attack signal flag 

            // When signaled, copy enemy health into attack
            d->barbarian.attack = d->enemy.health;

            // Dungeon will wait SECONDS_TO_ATTACK and then compare
            sleep(SECONDS_TO_ATTACK);
        }

        // Handle lever semaphores once, when dungeon tells us to
                if (g_do_levers && !levers_done) {
            g_do_levers = 0;
            levers_done = true;
//both open levers 
            sem_t *lever1 = sem_open(dungeon_lever_one, 0);
            sem_t *lever2 = sem_open(dungeon_lever_two, 0);
//error check for sem open 
            if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
                perror("barbarian: sem_open lever(s)");
                if (lever1 != SEM_FAILED && lever1 != NULL) sem_close(lever1);
                if (lever2 != SEM_FAILED && lever2 != NULL) sem_close(lever2);
            } else {
                printf("[Barbarian] Received SEMAPHORE_SIGNAL (holding both levers)\n");
                fflush(stdout);

                // Pull both levers down (door opens)
                if (sem_wait(lever1) == -1) {
                    perror("barbarian: sem_wait lever1");
                }
                if (sem_wait(lever2) == -1) {
                    perror("barbarian: sem_wait lever2");
                }

                printf("[Barbarian] Holding levers while Rogue gets treasure...\n");
                fflush(stdout);

                // Keep the door open long enough for the Rogue to read treasure
                sleep(TIME_TREASURE_AVAILABLE);

                // Release levers (door can close again)
                if (sem_post(lever2) == -1) {
                    perror("barbarian: sem_post lever2");
                }
                if (sem_post(lever1) == -1) {
                    perror("barbarian: sem_post lever1");
                }

                printf("[Barbarian] Released levers\n");
                fflush(stdout);
// close semaphore handlers 
                sem_close(lever1);
                sem_close(lever2);
            }
        }

        usleep(1000); //  avoid busy spin
    }
// unmap shared memory before exit
    munmap(d, sizeof(*d));
    return 0;
}
