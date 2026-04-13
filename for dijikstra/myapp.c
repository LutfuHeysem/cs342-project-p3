#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "rsm.h"

// System Configuration: 5 Processes, 5 Resources
#define NUMR 5
#define NUMP 5

int AVOID = 1;

// Exist Vector: all 5 resources meaningfully used
// R0=10, R1=5, R2=7, R3=4, R4=3
int exist[NUMR] = {10, 5, 7, 4, 3};

void pr (int apid, char astr[], int m, int r[]) {
    printf ("process %d, %s, [", apid, astr);
    for (int i = 0; i < m; ++i) {
        printf ("%d%s", r[i], (i == m-1) ? "" : ",");
    }
    printf ("]\n");
}

void setarray (int r[MAX_RT], int m, ...) {
    va_list valist;
    va_start (valist, m);
    for (int i = 0; i < m; i++) r[i] = va_arg(valist, int);
    va_end(valist);
}

// Initial Allocations
// Total allocated: R0=7, R1=2, R2=5, R3=3, R4=2
// Available after alloc: R0=3, R1=3, R2=2, R3=1, R4=1
int allocs[NUMP][5] = {
    {0, 1, 0, 1, 0},  // P0
    {2, 0, 0, 0, 1},  // P1
    {3, 0, 2, 0, 0},  // P2
    {2, 1, 1, 0, 1},  // P3
    {0, 0, 2, 2, 0},  // P4
};

// Max Demands (used in AVOID=1 mode)
// Need = Max - Allocation:
//   P0: [7,4,3,1,1]
//   P1: [1,2,2,1,1]
//   P2: [6,0,0,2,0]
//   P3: [0,1,1,1,1]
//   P4: [4,3,1,1,1]
// Safe sequence with Available=[3,3,2,1,1]: P3 -> P1 -> P4 -> P0 -> P2
int max_claims[NUMP][5] = {
    {7, 5, 3, 2, 1},  // P0
    {3, 2, 2, 1, 2},  // P1
    {9, 0, 2, 2, 0},  // P2
    {2, 2, 2, 1, 2},  // P3
    {4, 3, 3, 3, 1},  // P4
};

void run_process(int apid) {
    int req[MAX_RT], claim[MAX_RT];
    rsm_process_started(apid);

    // Initial claim phase (AVOID=1 only)
    for(int i = 0; i < NUMR; i++) claim[i] = max_claims[apid][i];
    if (AVOID) rsm_claim(claim);

    // Step 1: Initial Allocation
    for(int i = 0; i < NUMR; i++) req[i] = allocs[apid][i];
    if (req[0] || req[1] || req[2] || req[3] || req[4]) {
        pr(apid, "REQ (Initial Allocation)", NUMR, req);
        rsm_request(req);
    }

    // Barrier: let everyone grab initial resources
    sleep(2);

    // Step 2: Conflict Phase
    if (AVOID == 0) {
        // --- DETECTION MODE: Guaranteed Deadlock (Circular Wait) ---
        // Available after init: [3, 3, 2, 1, 1]
        
        if (apid == 1) {
            // P1 Drains R2 and R4 -> Requests [0,0,2,0,1]
            setarray(req, NUMR, 0, 0, 2, 0, 1);
            sleep(1);
            pr(apid, "REQ (Drain R2+R4)", NUMR, req);
            rsm_request(req);

            // P1 Waits for R0 (Held by P0)
            setarray(req, NUMR, 1, 0, 0, 0, 0);
            sleep(5);
            pr(apid, "REQ (Wants R0, Circular Wait -> DEADLOCK)", NUMR, req);
            rsm_request(req);
        }
        else if (apid == 0) {
            // P0 Drains R0 and R1 -> Requests [3,3,0,0,0]
            setarray(req, NUMR, 3, 3, 0, 0, 0);
            sleep(2);
            pr(apid, "REQ (Drain R0+R1)", NUMR, req);
            rsm_request(req);

            // P0 Waits for R3 (Held by P4)
            setarray(req, NUMR, 0, 0, 0, 1, 0);
            sleep(5);
            pr(apid, "REQ (Wants R3, Circular Wait -> DEADLOCK)", NUMR, req);
            rsm_request(req);
        }
        else if (apid == 4) {
            // P4 Drains R3 -> Requests [0,0,0,1,0]
            setarray(req, NUMR, 0, 0, 0, 1, 0);
            sleep(3);
            pr(apid, "REQ (Drain R3)", NUMR, req);
            rsm_request(req);

            // P4 Waits for R2 (Held by P1)
            setarray(req, NUMR, 0, 0, 1, 0, 0);
            sleep(5);
            pr(apid, "REQ (Wants R2, Circular Wait -> DEADLOCK)", NUMR, req);
            rsm_request(req);
        }
        else if (apid == 2 || apid == 3) {
            // P2 and P3 just go to sleep and eventually ask for impossible amounts to block cleanly
            setarray(req, NUMR, 1, 1, 1, 1, 1);
            sleep(6);
            pr(apid, "REQ (Impossible, Blocks)", NUMR, req);
            rsm_request(req);
        }
    } else {
        // --- AVOIDANCE MODE: Banker's Algorithm demo ---
        //
        // P3 requests [0,1,1,1,1]:
        //   Need=[0,1,1,1,1] <= Available=[3,3,2,1,1] -> SAFE, granted
        //   After grant: Available=[3,2,1,0,0]
        //   Safe sequence still exists: P3->P1->P4->P0->P2
        //
        // P0 requests [3,0,2,0,0]:
        //   Would make Available=[0,2,1,0,0]
        //   P2 Need=[6,0,0,2,0] can never be satisfied -> UNSAFE, BLOCKED

        if (apid == 3) {
            setarray(req, NUMR, 0, 1, 1, 1, 1);
            sleep(1);
            pr(apid, "REQ (Safe, granted by Banker's)", NUMR, req);
            rsm_request(req);
        }
        else if (apid == 0) {
            setarray(req, NUMR, 3, 0, 2, 0, 0);
            sleep(2);
            pr(apid, "REQ (Unsafe, BLOCKED by Banker's)", NUMR, req);
            rsm_request(req);
        }
    }

    // Hold resources before ending
    sleep(15);
    rsm_process_ended();
    exit(0);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: ./myapp avoidflag\n");
        exit(1);
    }

    AVOID = atoi(argv[1]);
    rsm_init(NUMP, NUMR, exist, AVOID);

    for (int i = 0; i < NUMP; i++) {
        if (fork() == 0) run_process(i);
    }

    int deadlock_found = 0;
    for (int count = 0; count < 10; count++) {
        sleep(1);
        rsm_print_state("Monitoring System State...");

        int ret = rsm_detection();
        if (ret > 0 && AVOID == 0) {
            printf("\n>>> MAIN: %d processes DEADLOCKED! <<<\n", ret);
            rsm_print_state("State after deadlock detected");
            deadlock_found = 1;
            break;
        }
    }

    if (deadlock_found) {
        printf("Terminating due to deadlock.\n");
        rsm_destroy();
        kill(0, SIGTERM);
        exit(0);
    } else {
        printf("Simulation finished safely without deadlock.\n");
        kill(0, SIGTERM);
    }

    rsm_destroy();
    return 0;
}