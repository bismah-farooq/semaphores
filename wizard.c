// wizard.c
// Wizard decodes Caesar cipher in barrier.spell into wizard.spell.

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>

#include "dungeon_info.h"
#include "dungeon_settings.h"

#ifndef WIZARD_SIGNAL
#define WIZARD_SIGNAL    SIGUSR2
#endif
#ifndef SEMAPHORE_SIGNAL
#define SEMAPHORE_SIGNAL SIGINT
#endif

static volatile sig_atomic_t got_signal = 0;

static void wizard_handle(int s) {
    (void)s;
    got_signal = 1;   // we don't rely on this, but it's nice to have
}

// shift one character back by k (mod 26), preserve case
static char shift_back(char c, int k26) {
    if (c >= 'A' && c <= 'Z') {
        int base = 'A';
        int off  = (c - base - k26) % 26;
        if (off < 0) off += 26;
        return (char)(base + off);
    }
    if (c >= 'a' && c <= 'z') {
        int base = 'a';
        int off  = (c - base - k26) % 26;
        if (off < 0) off += 26;
        return (char)(base + off);
    }
    return c; // spaces, punctuation, digits unchanged
}

int main(void) {
    // attach to shared memory
    int fd = -1;
    for (int tries = 0; tries < 50 && fd == -1; ++tries) {
        fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
        if (fd == -1) usleep(100000);
    }
    if (fd == -1) {
        fprintf(stderr, "wizard: could not open /DungeonMem. run game first.\n");
        return 1;
    }

    struct Dungeon *d = mmap(NULL, sizeof(*d),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             fd,
                             0);
    close(fd);
    if (d == MAP_FAILED) {
        perror("wizard: mmap");
        return 1;
    }

    // set up signals (mostly to avoid crashing)
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = wizard_handle;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(WIZARD_SIGNAL, &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    printf("wizard ready (pid=%d)\n", getpid());
    fflush(stdout);

    // keep track of last key so we can tell when a new spell appears
    char last_key = '\0';

    while (d->running) {
        // either we got a signal or we just poll periodically
        if (!got_signal) {
            usleep(1000); // 1 ms
        }
        got_signal = 0;

        const char *in  = d->barrier.spell;  // cipher, first char = key
        char       *out = d->wizard.spell;   // decoded answer

        if (!in || !out) continue;

        // if no spell, or same key and same text, just keep waiting
        if (in[0] == '\0') {
            continue;
        }

        // only decode when the spell actually changes (new puzzle)
        if (in[0] == last_key && strlen(in) == strlen(out) + 1) {
            // looks like we've already decoded this one
            continue;
        }

        last_key = in[0];

        int key_ascii = (unsigned char)in[0]; // e.g. 'x' = 120
        int k26 = key_ascii % 26;

        size_t j = 0;
        for (size_t i = 1; in[i] != '\0' && j + 1 < SPELL_BUFFER_SIZE; ++i) {
            out[j++] = shift_back(in[i], k26);
        }
        out[j] = '\0';

        // debug so you can see what's happening
        // (you can comment these out later)
        printf("wizard: encoded=\"%s\" decoded=\"%s\"\n", in, out);
        fflush(stdout);
    }

    munmap(d, sizeof(*d));
    return 0;
}

