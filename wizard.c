#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <string.h>
#include "dungeon_info.h"
#include "dungeon_settings.h"

// ---- Fallback signal defines (remove if your dungeon_settings.h already defines them) ----
#ifndef WIZARD_SIGNAL
#define WIZARD_SIGNAL    SIGUSR2
#endif
#ifndef SEMAPHORE_SIGNAL
#define SEMAPHORE_SIGNAL SIGINT
#endif
// -----------------------------------------------------------------------------------------

static volatile sig_atomic_t go = 0;

static void handle_signal(int sig) {
    (void)sig;
    go = 1;
}

static inline char shiftback(char c, int k_mod26) {
    if (c >= 'A' && c <= 'Z') {
        int base = 'A';
        return (char)(base + ((c - base - k_mod26) % 26 + 26) % 26);
    }
    if (c >= 'a' && c <= 'z') {
        int base = 'a';
        return (char)(base + ((c - base - k_mod26) % 26 + 26) % 26);
    }
    return c; // leave punctuation/space unchanged
}

int main(void) {
    // Open shared memory created by game
    int fd = -1;
    for (int i = 0; i < 50 && fd == -1; ++i) {
        fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
        if (fd == -1) usleep(100000); // wait for game to create /DungeonMem
    }
    if (fd == -1) { fprintf(stderr, "wizard: shared memory not found. Start game first.\n"); return 1; }

    struct Dungeon* d = mmap(NULL, sizeof *d, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (d == MAP_FAILED) { perror("mmap"); return 1; }

    // Set up signal handlers
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(WIZARD_SIGNAL, &sa, NULL) == -1) perror("sigaction WIZARD_SIGNAL");
    if (sigaction(SEMAPHORE_SIGNAL, &sa, NULL) == -1) perror("sigaction SEMAPHORE_SIGNAL");

    printf("Wizard ready! PID=%d\n", getpid());
    fflush(stdout);

    while (d->running) {
        if (!go) { usleep(1000); continue; }
        go = 0;

        // Read cipher from barrier.spell. First char is the key.
        const char *in = d->barrier.spell;              // size SPELL_BUFFER_SIZE+1
        char *out = d->wizard.spell;                    // size SPELL_BUFFER_SIZE
        if (!in || !out) continue;

        // If empty, nothing to do
        if (in[0] == '\0') { out[0] = '\0'; continue; }

        int key = (unsigned char)in[0];                 // key is ASCII value of first char
        int k26 = ((key % 26) + 26) % 26;

        // Decode starting at in[1], write into out[0..]
        size_t j = 0;
        for (size_t i = 1; in[i] != '\0' && j + 1 < SPELL_BUFFER_SIZE; ++i) {
            out[j++] = shiftback(in[i], k26);
        }
        out[j] = '\0';

        // Optional logging:
        // printf("Wizard decoded: \"%s\"\n", out);
        // fflush(stdout);

        // The dungeon will wait SECONDS_TO_GUESS_BARRIER and compare out to the expected answer.
    }

    munmap(d, sizeof *d);
    return 0;
}

