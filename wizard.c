// barbarian.c 
// barbarian copies the enemy health and pulls the treasure levers using semaphores 
#define _DEFAULT_SOURCE  // enables POSIX extention

#include <stdio.h> // standard IO functions
#include <stdlib.h> // memory allocation
#include <string.h> // string
#include <fcntl.h> // file control
#include <sys/mman.h> // memory mapping 
#include <sys/stat.h> //for constants
#include <signal.h> // signal handling 
#include <unistd.h> // for pause(), sleep() etc

#include "dungeon_info.h" // contains structs and memory names 
#include "dungeon_settings.h" // contains config. + constraints

#ifndef WIZARD_SIGNAL // default wizard signal
#define WIZARD_SIGNAL SIGUSR1
#endif

static struct Dungeon *g_dungeon = NULL;

static void decode_barrier(void) {
    if (!g_dungeon) return; // abort if not initialized 

    char *encoded = g_dungeon->barrier.spell; // pointer to encoded spell text 
    char *out     = g_dungeon->wizard.spell; // output buffer for decoded spell 

    if (!encoded || !out) return; // ensure pointer exist 

    if (encoded[0] == '\0') {
        out[0] = '\0';
        return;
    }

    int shift = ((unsigned char)encoded[0]) % 26; 

    size_t in_i;
    size_t out_i = 0;

   for (in_i = 1; encoded[in_i] != '\0' && out_i < SPELL_BUFFER_SIZE - 1; ++in_i, ++out_i) {
        char c = encoded[in_i]; // character to decode 

        if (c >= 'A' && c <= 'Z') {  // uppercase letter decoding 
            int idx = c - 'A';
            idx = (idx - shift) % 26;
            if (idx < 0) idx += 26;
            out[out_i] = (char)('A' + idx);
        } else if (c >= 'a' && c <= 'z') { // lowercase letter decoding 
            int idx = c - 'a';
            idx = (idx - shift) % 26;
            if (idx < 0) idx += 26;
            out[out_i] = (char)('a' + idx) 
        } else {
            out[out_i] = c;
        }
    }

    out[out_i] = '\0'; // null terminate the decode 
}

static void wizard_handler(int sig) {
    if (!g_dungeon) return; //safety

    if (sig == WIZARD_SIGNAL) {
        decode_barrier();
    } else if (sig == SEMAPHORE_SIGNAL) {
    }
}

int main(void){
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0660);
    if (fd == -1) {
        perror("wizard shm_open"); // print why it failed 
        return EXIT_FAILURE; // exit with failure code 
    }

    g_dungeon = mmap(NULL, sizeof(struct Dungeon),
                     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // we need to write/read, shared with other processes 
    if (g_dungeon == MAP_FAILED) {
        perror("wizard mmap");
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd); // no need to fix 

    struct sigaction sa;
    sa.sa_handler = wizard_handler;  // to handle signlas 
    sigemptyset(&sa.sa_mask); // no signal blocked during handler 
    sa.sa_flags = 0; // default behaviour

    if (sigaction(WIZARD_SIGNAL, &sa, NULL) == -1) {
        perror("wizard sigaction WIZARD_SIGNAL");
        return EXIT_FAILURE;
    }
    if (sigaction(SEMAPHORE_SIGNAL, &sa, NULL) == -1) {
        perror("wizard sigaction SEMAPHORE_SIGNAL");
        return EXIT_FAILURE;
    }

    while (g_dungeon->running) {
        pause(); // sleep until any signal arrive 
    }

    munmap(g_dungeon, sizeof(struct Dungeon)); //unmap shared memory 
    return EXIT_SUCCESS; // exit 
}
