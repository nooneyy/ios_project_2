#include <errno.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

// Minimalni a maximalni pocet voziku, ktery program podporuje + idx v argv
#define CART_COUNT_IDX 1
#define CART_COUNT_MIN 1
#define CART_COUNT_MAX 9

// Minimalni a maximalni pocet navstevniku, ktery program podporuje + idx v argv
#define VISITOR_COUNT_IDX 2
#define VISITOR_COUNT_MIN 1
#define VISITOR_COUNT_MAX 9999

// Minimalni a maximalni kapacita, kterou muze mit vozik + idx v argv
#define CART_CAPACITY_IDX 3
#define CART_CAPACITY_MIN 4
#define CART_CAPACITY_MAX 40

// Minimalni a maximalni doba jizdy voziku + idx v argv
#define RIDE_LENGTH_IDX 4
#define RIDE_LENGTH_MIN 0
#define RIDE_LENGTH_MAX 1000

// Minimalni a maximalni nejdelsi doba, nez navstevnik dojde do rady na atrakci
// + idx v argv
#define QUEUE_TIME_IDX 5
#define QUEUE_TIME_MIN 0
#define QUEUE_TIME_MAX 1000

// Minimalni a maximalni nejmensi odstup, ktere musi od sebe voziky udrzovat +
// idx v argv
#define CART_GAP_IDX 5
#define CART_GAP_MIN 0
#define CART_GAP_MAX 100

#define CORRECT_ARGC_COUNT 7
#define ERROR_EXIT_CODE 1

#define FILE_NAME "proj2.out"
#define FILE_OPEN_MODE "w+"

typedef struct {
    FILE *file_descriptor;
    int *line_count;
    sem_t *mutex;
} Writer;

typedef struct {
    sem_t *go_signal;
    sem_t *finished_signal;
    sem_t *board_signal;
    sem_t *unboard_signal;
    int *visitor_count;
    sem_t *visitor_count_mutex;
    int *boarded_count;
    sem_t *boarded_count_mutex;
    int *unboarded_count;
    sem_t *unboarded_count_mutex;
    sem_t *boarding_done;
    sem_t *unboarding_done;
    sem_t **ready_cart_sems;
    sem_t **finished_cart_sems;
    bool *dispatcher_kill_sig;
} Shared;