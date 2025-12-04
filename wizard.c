#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include "dungeon_info.h"
#include "dungeon_settings.h"

// If the settings header doesn't define these, define them here.
#ifndef WIZARD_SIGNAL
#define WIZARD_SIGNAL SIGUSR1
#endif

// SEMAPHORE_SIGNAL *is* defined in dungeon_settings.h

static struct Dungeon *g_dungeon = NULL;

static void decode_barrier(void) {
    if (!g_dungeon) return;

    char *encoded = g_dungeon->barrier.spell;
    char *out     = g_dungeon->wizard.spell;

    if (!encoded || !out) return;

    // If nothing there, clear wizard spell
    if (encoded[0] == '\0') {
        out[0] = '\0';
        return;
    }

    // First character is the key
    int shift = ((unsigned char)encoded[0]) % 26;

    size_t in_i;
    size_t out_i = 0;

    // Decode from encoded[1] into out[0..]
    for (in_i = 1; encoded[in_i] != '\0' && out_i < SPELL_BUFFER_SIZE - 1; ++in_i, ++out_i) {
        char c = encoded[in_i];

        if (c >= 'A' && c <= 'Z') {
            int idx = c - 'A';
            idx = (idx - shift) % 26;
            if (idx < 0) idx += 26;
            out[out_i] = (char)('A' + idx);
        } else if (c >= 'a' && c <= 'z') {
            int idx = c - 'a';
            idx = (idx - shift) % 26;
            if (idx < 0) idx += 26;
            out[out_i] = (char)('a' + idx);
        } else {
            // punctuation, spaces, etc. unchanged
            out[out_i] = c;
        }
    }

    out[out_i] = '\0';
}

static void wizard_handler(int sig) {
    if (!g_dungeon) return;

    if (sig == WIZARD_SIGNAL) {
        // Decode the barrier spell into wizard.spell
        decode_barrier();
    } else if (sig == SEMAPHORE_SIGNAL) {
        // Optional: wizard could help with levers here.
        // Leaving empty so we don't crash; only barbarian/rogue may need to act.
    }
}

int main(void) {
    // Attach to shared memory
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0660);
    if (fd == -1) {
        perror("wizard shm_open");
        return EXIT_FAILURE;
    }

    g_dungeon = mmap(NULL, sizeof(struct Dungeon),
                     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (g_dungeon == MAP_FAILED) {
        perror("wizard mmap");
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd);

    // Set up signal handlers
    struct sigaction sa;
    sa.sa_handler = wizard_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(WIZARD_SIGNAL, &sa, NULL) == -1) {
        perror("wizard sigaction WIZARD_SIGNAL");
        return EXIT_FAILURE;
    }
    if (sigaction(SEMAPHORE_SIGNAL, &sa, NULL) == -1) {
        perror("wizard sigaction SEMAPHORE_SIGNAL");
        return EXIT_FAILURE;
    }

    // Stay alive while the dungeon is running
    while (g_dungeon->running) {
        pause();
    }

    munmap(g_dungeon, sizeof(struct Dungeon));
    return EXIT_SUCCESS;
}

