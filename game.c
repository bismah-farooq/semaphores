// game.c
// Main launcher for CECS 326 Lab 2
// Starts shared memory, spawns Barbarian/Wizard/Rogue, then runs the dungeon.

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>

#include "dungeon_info.h"
#include "dungeon_settings.h"

// ---------------------------------------------------------
// Helper to create shared memory for Dungeon struct
// ---------------------------------------------------------
struct Dungeon* create_shared_dungeon() {
    int fd = shm_open(dungeon_shm_name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(fd, sizeof(struct Dungeon)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    struct Dungeon *d =
        mmap(NULL, sizeof(struct Dungeon),
             PROT_READ | PROT_WRITE,
             MAP_SHARED,
             fd,
             0);

    close(fd);

    if (d == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    memset(d, 0, sizeof(struct Dungeon));
    d->running = true;

    return d;
}

// ---------------------------------------------------------
// Helper to start one process
// ---------------------------------------------------------
pid_t start_process(const char *name, const char *path) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        execl(path, name, NULL);
        perror("execl failed");
        exit(1);
    }
    return pid;
}

int main(void) {

    // 1) Shared memory
    struct Dungeon *d = create_shared_dungeon();

    // 2) Create semaphores for treasure room
    sem_t *lever1 = sem_open(dungeon_lever_one, O_CREAT, 0666, 1);
    sem_t *lever2 = sem_open(dungeon_lever_two, O_CREAT, 0666, 1);

    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // 3) Start each character
    pid_t barbarian_pid = start_process("barbarian", "./barbarian");
    pid_t wizard_pid    = start_process("wizard",    "./wizard");
    pid_t rogue_pid     = start_process("rogue",     "./rogue");

    printf("Game: spawned processes\n");
    printf("  Barbarian: %d\n", barbarian_pid);
    printf("  Wizard:    %d\n", wizard_pid);
    printf("  Rogue:     %d\n", rogue_pid);

    // 4) Run the dungeon
    // ORDER = RunDungeon(wizard, rogue, barbarian)
    RunDungeon(wizard_pid, rogue_pid, barbarian_pid);

    // 5) Dungeon is finished → tell processes to shut down
    d->running = false;

    kill(barbarian_pid, SIGTERM);
    kill(wizard_pid, SIGTERM);
    kill(rogue_pid, SIGTERM);
	
    sleep(1);
    munmap(d, sizeof(struct Dungeon));
    shm_unlink(dungeon_shm_name);

    printf("Game finished. Clean exit.\n");
    return 0;
}
