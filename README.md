# CECS 326 Lab 2: Concurrent Processing, Shared Memory, and Semaphores

## Overview

This project is a **multi-process role-playing game (RPG) dungeon simulation** written in **C** that demonstrates core **Unix/Linux systems programming concepts**, including:

- Concurrent processes (`fork`, `exec`)
- POSIX shared memory (`shm_open`, `mmap`)
- Signal handling (`sigaction`)
- Inter-process communication (IPC)
- Named POSIX semaphores
- Synchronization and race-condition avoidance

The program simulates a dungeon run by a party of three characters — a **Barbarian**, **Wizard**, and **Rogue** — each implemented as a **separate Unix process** that interacts with a shared `Dungeon` structure.

---

## Project Motivation

This project was built to explore **low-level concurrency and synchronization** in a realistic and interactive way.  
Instead of using abstract examples, the system models a dungeon game where:

- Processes must react to **signals**
- Shared memory must be accessed **safely**
- Semaphores must be used **correctly** to coordinate access to a critical section

The result is a compact but non-trivial demonstration of how **real operating systems manage concurrent processes**.

## Components

### 🧠 Game (`game.c`)
- Entry point of the program
- Creates shared memory
- Initializes named semaphores
- Launches character processes using `fork` and `exec`
- Calls `RunDungeon()` with character PIDs
- Cleans up shared resources on exit

---

### ⚔️ Barbarian (`barbarian.c`)
- Waits for a signal from the dungeon
- Copies the enemy’s health value into the attack field
- Sleeps for a configured time window
- Succeeds if the attack matches the enemy’s health

Demonstrates **signal handling** and **shared memory writes**.

---

### 🪄 Wizard (`wizard.c`)
- Receives a signal when a magical barrier is encountered
- Reads a Caesar-cipher–encoded spell from shared memory
- Uses the first character as a shift key
- Decodes the message while preserving:
  - Case
  - Punctuation
- Writes the decoded spell back to shared memory

Demonstrates **string processing**, **ASCII arithmetic**, and **IPC correctness**.

---

### 🗡️ Rogue (`rogue.c`)
Handles two major tasks:

#### Lock Picking (Binary Search)
- Dungeon selects a random floating-point lock angle
- Rogue guesses using binary search
- Dungeon provides feedback:
  - `'u'` → guess higher
  - `'d'` → guess lower
  - `'-'` → success

Demonstrates **real-time IPC feedback loops**.

#### Treasure Room (Semaphores)
- Two party members must hold levers
- Implemented using **named POSIX semaphores**
- Rogue waits until both are held
- Collects treasure characters one at a time
- Writes them into the `spoils` field
- Releases semaphores when done

Demonstrates **correct semaphore usage** and cleanup.

Compile
```test
make
```
or manually:
```text
gcc game.c barbarian.c wizard.c rogue.c dungeon.o -o dungeon_game -lrt -pthread
```
Rename the correct dungeon object file (dungeon_ARM64.o or dungeon_X86_64.o) to dungeon.o.

Run
```text
./dungeon_game
```


