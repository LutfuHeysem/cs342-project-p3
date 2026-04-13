#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include "rsm.h"
#include <signal.h>

#define TRUE 1
#define FALSE 0

#define SHM_NAME "/rsm_shm"

typedef struct {
    int N; // number of processes
    int M; // number of resource types

    int ExistingV[MAX_RT]; // number of existing resources of each type
    int AvailV[MAX_RT]; // number of available resources of each type
    int AllocationM[MAX_PR][MAX_RT]; // number of resources of each type allocated to each process
    int RequestM[MAX_PR][MAX_RT]; // number of resources of each type requested by each process, only for detection

    //  avoidance
    int avoidance; // 1 for avoidance, 0 for detection
    int MaxM[MAX_PR][MAX_RT]; // number of max resources of each type claimed by each process, only for avoidance
    int NeedM[MAX_PR][MAX_RT]; // number of resources of each type needed by each process, only for avoidance
    
    int started_count; // processes that called process_started
    int claimCount; // number of processes that have made their claims, only for avoidance

    int blocked[MAX_PR]; // 1 if the process is currently blocked, 0 otherwise
    int active[MAX_PR]; // 1 if the process is currently active, 0 otherwise
    int apid_to_pid[MAX_PR]; // mapping from apid to actual process ID, set in rsm_process_started

    sem_t mutex; // for mutual exclusion when accessing shared data structures
    sem_t procWait[MAX_PR]; // for processes to wait when their requests cannot be granted immediately
    sem_t barrierMutex; // protects barrier counter
    sem_t barrierWait; //barrier: released when all ready 
} shared_data_t;

static shared_data_t *shared_data = NULL;
static int my_apid = -1; // the apid of the current process, set in rsm_process_started

// Helper to check if a request can be satisfied by current available resources
static int can_satisfy(int request[], int available[]) {
    for (int j = 0; j < shared_data->M; j++) {
        if (request[j] > available[j]) return FALSE;
    }
    return TRUE;
}

// Banker's Algorithm: Safety Check
static int is_safe_state() {
    int Work[MAX_RT];
    int Finish[MAX_PR];

    // Initialize Work = Available
    for (int j = 0; j < shared_data->M; j++) {
        Work[j] = shared_data->AvailV[j];
    }

    // Initialize Finish: if process is not active, treat as finished
    for (int i = 0; i < shared_data->N; i++) {
        Finish[i] = !shared_data->active[i];
    }

    int possible = TRUE;
    while (possible) {
        possible = FALSE;
        for (int i = 0; i < shared_data->N; i++) {
            if (!Finish[i]) {
                if (can_satisfy(shared_data->NeedM[i], Work)) {
                    // Simulate completion: release resources
                    for (int j = 0; j < shared_data->M; j++) {
                        Work[j] += shared_data->AllocationM[i][j];
                    }
                    Finish[i] = TRUE;
                    possible = TRUE;
                }
            }
        }
    }

    // If all processes can finish, state is safe
    for (int i = 0; i < shared_data->N; i++) {
        if (!Finish[i]) return FALSE;
    }
    return TRUE;
}

/**
 * called once by main process before forking children
 * creates and initializes shared memory and semaphores
 * @param p_count Number of processes
 * @param r_count Number of resource types
 * @param exist Array containing the number of existing resources of each type
 * @param avoid 1 for avoidance, 0 for detection
 * @return 0 on success, -1 on failure
 */
int rsm_init(int p_count, int r_count, int exist[],  int avoid)
{
    //parameter checking
    if (p_count <= 0 || p_count > MAX_PR){ return -1;}
    if (r_count <= 0 || r_count > MAX_RT){ return -1;}

    //remove any stale segment from previous caches
    shm_unlink(SHM_NAME); 

    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if(fd < 0 ){
        perror("shm_open");
        return -1;
    }

    //set the size of the shared memory segment to fit shared data structure
    if(ftruncate(fd, sizeof(shared_data_t)) < 0){
        perror("ftruncate");
        close(fd);
        return -1;
    }

    //map the shared memory segment into the process's address space
    //PROT_READ | PROT_WRITE allows both reading and writing to the shared memory
    //MAP_SHARED allows changes to the shared memory to be visible to other processes t
    shared_data = (shared_data_t *)mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    //there is mapping between shared memory and the process. thne no need for fd anymore
    close(fd);

    if (shared_data == MAP_FAILED)
    {
        perror("mmap");
        return -1;
    }

    //initialize shared data structure to 0( needed for avoid garbage values)
    memset(shared_data, 0, sizeof(shared_data_t)); // initialize shared data to zero

    shared_data->N = p_count;
    shared_data->M = r_count;
    shared_data->avoidance = avoid;

    //initialize resource vectors of ExistingV and AvailV arrays based on parameter
    for (int i = 0; i < r_count; i++) {
        shared_data->ExistingV[i] = exist[i];
        shared_data->AvailV[i] = exist[i];
    }

    
    sem_init(&shared_data->mutex, 1, 1); // initialize global mutex semaphore 1 for shared between process and 1 for initial value which is nlocked
    sem_init(&shared_data->barrierMutex, 1, 1);// need to protect barrier counter
    sem_init(&shared_data->barrierWait, 1, 0); //process wait here until all are ready 

    //blocks semaphores for each process, initialized to 0 (locked) and will be released when the process can proceed
    for (int i = 0; i < p_count; i++) {
        sem_init(&shared_data->procWait[i], 1, 0); // initialize process-specific semaphores for waiting, initially locked
    }

    return 0;
}

/**
 * called by the maib process 
 * kills all child processes, destroy semaphores, and removes the shm
 */
int rsm_destroy()
{
    if (shared_data == NULL) {
        return -1; // shared data not initialized
    }

    //unblock all blocked processes before destroying semaphores and shared memory
    for (int i = 0; i < shared_data->N; i++) {
        if(shared_data->blocked[i] == TRUE){
            sem_post(&shared_data->procWait[i]); // unblock any waiting processes
        }
    }

    sem_destroy(&shared_data->mutex);
    sem_destroy(&shared_data->barrierMutex);
    sem_destroy(&shared_data->barrierWait);

    for (int i = 0; i < shared_data->N; i++) {
        sem_destroy(&shared_data->procWait[i]);
    }

    munmap(shared_data, sizeof(shared_data_t)); // unmap the shared memory segment
    shared_data = NULL;
    shm_unlink(SHM_NAME); // remove the shared memory segment

    // kill(0,SIGTERM);
    // exit(0);

    return 0;
}

/**
 * called by each child process just after it begin
 * records the process as acticve, then waits at the barries until ALL N
 * children have registered (so no one races ahead with request before the others have even set up their claims)
 *
 * @param pid Process ID
 * @return 0 on success, -1 on failure
 */
int rsm_process_started(int apid)
{
    if(shared_data == NULL){
        return -1; // shared data not initialized
    }
    if(apid < 0 || apid >= shared_data->N){
        return -1; // invalid apid
    }
    
    my_apid = apid; // set the global variable for the current process's apid

    //critical section!!!!!!!!
    sem_wait(&shared_data->mutex);
    shared_data->active[apid] = TRUE; // mark the process as active
    shared_data->apid_to_pid[apid] = (int)getpid(); // store the actual process ID for this apid
    shared_data->started_count++; // increment the count of started processes
    int all_started = (shared_data->started_count == shared_data->N); // check if all processes have started
    sem_post(&shared_data->mutex);
    // end crtiticl section

    //barrier logic begins
    // barries works only when avoidance is 0
    if (shared_data->avoidance)
    {
        /* when avoidance is on do not block here
        the barries is inside rsm_claim, every process will block there
        until all N processes have submitted their claims
        we just return so the process can proceed*/
        (void)all_started; // avoid unused variable warning
    }else{
        // no avoidance: barries on startedCount
        if (all_started) {
            // if not all processes have started, wait at the barrier
            for(int i = 0; i < shared_data->N; i++){
                sem_post(&shared_data->procWait[i]); // last process to arrive, relese everyone
            }
        }else {
            sem_wait(&shared_data->procWait[apid]); // wait until all processes have started
        }
    }
    
    return 0;
}

/**
 * called by each child process just before it ends
 */
int rsm_process_ended()
{
    if (shared_data == NULL)
    {
        return -1; // shared data not initialized
    }

    //critical section
    sem_wait(&shared_data->mutex);
    if(my_apid >= 0){
        shared_data->active[my_apid] = FALSE; // mark the process as inactive
    }
    sem_post(&shared_data->mutex);
    //end critical section

    // exit(0);
    return 0;
}

/**
 * called by each child process to submit its claim of maximum resources needed (only for avoidance)
 */
int rsm_claim(int claim[]) {
    if (shared_data == NULL || !shared_data->avoidance) return -1;

    sem_wait(&shared_data->mutex);
    for (int j = 0; j < shared_data->M; j++) {
        if (claim[j] > shared_data->ExistingV[j]) {
            sem_post(&shared_data->mutex);
            return -1; // Claim exceeds system limits
        }
        shared_data->MaxM[my_apid][j] = claim[j];
        shared_data->NeedM[my_apid][j] = claim[j];
    }
    shared_data->claimCount++;
    int all_claimed = (shared_data->claimCount == shared_data->N);
    sem_post(&shared_data->mutex);

    // Barrier: Wait until all processes have registered their claims
    if (all_claimed) {
        for (int i = 0; i < shared_data->N; i++) {
            sem_post(&shared_data->procWait[i]);
        }
    } else {
        sem_wait(&shared_data->procWait[my_apid]);
    }
    return 0;
}


/**
 * called by each child process to request resources
 * blocks if the request cannot be granted immediately
 */
int rsm_request(int request[]) {
    if (shared_data == NULL) return -1;

    sem_wait(&shared_data->mutex);
    
    // Check if request is valid (doesn't exceed MaxDemand/Need)
    for (int j = 0; j < shared_data->M; j++) {
        if (shared_data->avoidance && request[j] > shared_data->NeedM[my_apid][j]) {
            sem_post(&shared_data->mutex);
            return -1;
        }
        shared_data->RequestM[my_apid][j] = request[j];
    }

    while (TRUE) {
        if (can_satisfy(request, shared_data->AvailV)) {
            int safe = TRUE;
            if (shared_data->avoidance) {
                // Tentative allocation for safety check
                for (int j = 0; j < shared_data->M; j++) {
                    shared_data->AvailV[j] -= request[j];
                    shared_data->AllocationM[my_apid][j] += request[j];
                    shared_data->NeedM[my_apid][j] -= request[j];
                }

                if (!is_safe_state()) {
                    // Rollback tentative allocation
                    for (int j = 0; j < shared_data->M; j++) {
                        shared_data->AvailV[j] += request[j];
                        shared_data->AllocationM[my_apid][j] -= request[j];
                        shared_data->NeedM[my_apid][j] += request[j];
                    }
                    safe = FALSE;
                }
            } else {
                // Non-avoidance: Just allocate if available
                for (int j = 0; j < shared_data->M; j++) {
                    shared_data->AvailV[j] -= request[j];
                    shared_data->AllocationM[my_apid][j] += request[j];
                }
            }

            if (safe) {
                for (int j = 0; j < shared_data->M; j++) shared_data->RequestM[my_apid][j] = 0;
                shared_data->blocked[my_apid] = FALSE;
                sem_post(&shared_data->mutex);
                return 0;
            }
        }

        // Block process if unsafe or unavailable
        shared_data->blocked[my_apid] = TRUE;
        sem_post(&shared_data->mutex);
        sem_wait(&shared_data->procWait[my_apid]); 
        sem_wait(&shared_data->mutex); // Re-acquire mutex to check again
    }
}


/**
 * called by each child process to release resources
 */
int rsm_release(int release[]) {
    if (shared_data == NULL) return -1;

    sem_wait(&shared_data->mutex);
    for (int j = 0; j < shared_data->M; j++) {
        if (release[j] > shared_data->AllocationM[my_apid][j]) {
            sem_post(&shared_data->mutex);
            return -1; // Releasing more than allocated
        }
        shared_data->AvailV[j] += release[j];
        shared_data->AllocationM[my_apid][j] -= release[j];
        if (shared_data->avoidance) {
            shared_data->NeedM[my_apid][j] += release[j];
        }
    }

    // Wake up all blocked processes to re-check their conditions
    for (int i = 0; i < shared_data->N; i++) {
        if (shared_data->blocked[i]) {
            sem_post(&shared_data->procWait[i]);
        }
    }
    sem_post(&shared_data->mutex);
    return 0;
}


/**
 * called by each child process to detect deadlocks
 */
int rsm_detection() {
    if (shared_data == NULL) return -1;

    sem_wait(&shared_data->mutex);
    int Work[MAX_RT];
    int Finish[MAX_PR];

    for (int j = 0; j < shared_data->M; j++) Work[j] = shared_data->AvailV[j];
    for (int i = 0; i < shared_data->N; i++) {
        // Finish[i] is true if process has no allocation
        int has_allocation = FALSE;
        for (int j = 0; j < shared_data->M; j++) {
            if (shared_data->AllocationM[i][j] > 0) has_allocation = TRUE;
        }
        Finish[i] = !has_allocation;
    }

    int possible = TRUE;
    while (possible) {
        possible = FALSE;
        for (int i = 0; i < shared_data->N; i++) {
            if (!Finish[i] && can_satisfy(shared_data->RequestM[i], Work)) {
                for (int j = 0; j < shared_data->M; j++) Work[j] += shared_data->AllocationM[i][j];
                Finish[i] = TRUE;
                possible = TRUE;
            }
        }
    }

    int count = 0;
    for (int i = 0; i < shared_data->N; i++) {
        if (!Finish[i] && shared_data->active[i]) count++;
    }
    sem_post(&shared_data->mutex);
    return count;
}

/**
 * called by each child process to print the current state of the system
 */
void rsm_print_state (char hmsg[])
{
    if (shared_data == NULL) return;

    sem_wait(&shared_data->mutex);

    printf("###########################\n");
    printf("%s\n", hmsg);
    printf("###########################\n");

    // 1. Exist Vector
    printf("Exist:\n");
    for (int j = 0; j < shared_data->M; j++) printf("R%d ", j);
    printf("\n");
    for (int j = 0; j < shared_data->M; j++) printf("%d  ", shared_data->ExistingV[j]);
    printf("\n");

    // 2. Available Vector
    printf("Available:\n");
    for (int j = 0; j < shared_data->M; j++) printf("R%d ", j);
    printf("\n");
    for (int j = 0; j < shared_data->M; j++) printf("%d  ", shared_data->AvailV[j]);
    printf("\n");

    // 3. Allocation Matrix
    printf("Allocation:\n");
    for (int j = 0; j < shared_data->M; j++) printf("   R%d", j);
    for (int i = 0; i < shared_data->N; i++) {
        printf("\nP%d: ", i);
        for (int j = 0; j < shared_data->M; j++) printf("%d  ", shared_data->AllocationM[i][j]);
    }
    printf("\n");

    // 4. Request Matrix
    printf("Request:\n");
    for (int j = 0; j < shared_data->M; j++) printf("   R%d", j);
    for (int i = 0; i < shared_data->N; i++) {
        printf("\nP%d: ", i);
        for (int j = 0; j < shared_data->M; j++) printf("%d  ", shared_data->RequestM[i][j]);
    }
    printf("\n");

    // 5. MaxDemand Matrix
    printf("MaxDemand:\n");
    for (int j = 0; j < shared_data->M; j++) printf("   R%d", j);
    for (int i = 0; i < shared_data->N; i++) {
        printf("\nP%d: ", i);
        for (int j = 0; j < shared_data->M; j++) {
            // If avoidance is disabled, values should be zero
            printf("%d  ", shared_data->avoidance ? shared_data->MaxM[i][j] : 0);
        }
    }
    printf("\n");

    // 6. Need Matrix
    printf("Need:\n");
    for (int j = 0; j < shared_data->M; j++) printf("   R%d", j);
    for (int i = 0; i < shared_data->N; i++) {
        printf("\nP%d: ", i);
        for (int j = 0; j < shared_data->M; j++) {
            // If avoidance is disabled, values should be zero
            printf("%d  ", shared_data->avoidance ? shared_data->NeedM[i][j] : 0);
        }
    }
    printf("\n###########################\n");

    sem_post(&shared_data->mutex);
}