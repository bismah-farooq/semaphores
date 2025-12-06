// game.c
// Main launcher for thsi lab 
// Starts shared memory, spawns Barbarian/Wizard/Rogue, then runs the dungeon.

#define _POSIX_C_SOURCE 200809L // extension for POSIX 

#include <stdio.h> // printf()
#include <stdlib.h>//exit()
#include <unistd.h>//fork()
#include <fcntl.h>//flags 
#include <signal.h>// kill()
#include <sys/mman.h>//map
#include <sys/stat.h>//memset
#include <string.h>// semaphore 
#include <semaphore.h>

#include "dungeon_info.h" // sared memory struct and semaphore 
#include "dungeon_settings.h" // gameplay constants 

// Helper to create shared memory for Dungeon struct
struct Dungeon* create_shared_dungeon() {
    int fd = shm_open(dungeon_shm_name, O_CREAT | O_RDWR, 0666);//create if missing open read and write , permission for everyone 
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(fd, sizeof(struct Dungeon)) == -1) { // resize shared memory to fit our dungeon struct 
        perror("ftruncate");
        exit(1);
    }
// map shared memory into the game process 
    struct Dungeon *d =
        mmap(NULL, sizeof(struct Dungeon),
             PROT_READ | PROT_WRITE, // read and write access 
             MAP_SHARED, // visible to child process 
             fd,
             0);

    close(fd); // fd no longer needed after mmap()

    if (d == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
//initizalze the dungeon stuct to zeros 
    memset(d, 0, sizeof(struct Dungeon));
    d->running = true; // set running flag so other known dungeon is active 

    return d;
}

// Helper to start one process
pid_t start_process(const char *name, const char *path) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {// cild process 
        execl(path, name, NULL);
        perror("execl failed"); // if exelcl returns, something failed 
        exit(1);
    } // parent process 
    return pid;
}

int main(void) {

    // Shared memory
    struct Dungeon *d = create_shared_dungeon();

    // Create semaphores for treasure room
    sem_t *lever1 = sem_open(dungeon_lever_one, O_CREAT, 0666, 1);
    sem_t *lever2 = sem_open(dungeon_lever_two, O_CREAT, 0666, 1);

    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Start each character
    pid_t barbarian_pid = start_process("barbarian", "./barbarian");
    pid_t wizard_pid    = start_process("wizard",    "./wizard");
    pid_t rogue_pid     = start_process("rogue",     "./rogue");
   // print PIDs for debugging 
    printf("Game: spawned processes\n");
    printf("  Barbarian: %d\n", barbarian_pid);
    printf("  Wizard:    %d\n", wizard_pid);
    printf("  Rogue:     %d\n", rogue_pid);

    //  Run the dungeon
    // ORDER = RunDungeon(wizard, rogue, barbarian)
    RunDungeon(wizard_pid, rogue_pid, barbarian_pid);

    //  Dungeon is finished → tell processes to shut down
    d->running = false;

    kill(barbarian_pid, SIGTERM);
    kill(wizard_pid, SIGTERM);
    kill(rogue_pid, SIGTERM);
// give processes time to exit cleanly 	
    sleep(1);
    munmap(d, sizeof(struct Dungeon)); // cleanup shared memory and semaphores 
    shm_unlink(dungeon_shm_name);

    printf("Game finished. Clean exit.\n");
    return 0;
}
