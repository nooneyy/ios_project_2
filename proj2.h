#include <errno.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
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
#define CART_GAP_IDX 6
#define CART_GAP_MIN 1
#define CART_GAP_MAX 100

#define CORRECT_ARGC_COUNT 7
#define ERROR_EXIT_CODE 1

#define SEMAPHORE_INIT_SUCCESS 0

#define FILE_NAME "proj2.out"
#define FILE_OPEN_MODE "w+"

// Informace souvisejici s jednotlivym vozikem
typedef struct {
  sem_t is_available; // Je vozik dostupny k zavolani dispecerem (tj. prave ted
                      // nejede ani z nej nevystupuji navstevnici)?
  sem_t is_ready; // Signalizace dispecera pro vozik znacici povoleni k jizde
  sem_t can_leave_station; // Signalizace od predchoziho voziku, ze tento vozik
                           // muze dojet do vystupni stanice

  sem_t unboard_signal;  // Signalizace pro n navstevniku, ze muzou vystoupit
  sem_t unboarding_done; // Signalizace od posledniho navstevnika, ze vsichni
                         // vystoupili

  int visitor_count;      // Pocet navstevniku jedouci vozikem (n)
  int unboarded_visitors; // Pocet navstevniku, kteri jiz vystoupili
} CartInfo;

typedef struct {
  int output_line_count; // Pocet radku ve vystupnim souboru (zaciname od 0)
  sem_t output_mutex;    // Mutex chranici pocet radku ve vystupnim souboru a
                         // moznost zapisovat radek

  int visitors_left;         // Pocet navstevniku, co zatim ceka na svezeni
  sem_t visitors_left_mutex; // Mutex chranici pocet navstevniku cekajici na
                             // svezeni

  int boarding_cart_idx; // Index voziku, do ktereho prave ted navstevnici
                         // nastupuji
  int boarded_visitors;  // Pocet navstevniku, kteri jiz nastoupili do voziku

  sem_t boarding_mutex; // Mutex chranici hodnoty souvisejici s nastupem
  sem_t board_signal;   // Signalizace pro n navstevniku, ze mohou nastoupit
  sem_t boarding_done;  // Signalizace od posledniho navstevnika, ze vsichni
                        // nastoupili
  sem_t cart_departed;  // Signalizace pro dispecera, ze vozik odjel na trat

  bool closing;        // Hodnota true znaci, ze dispecer uzavrel atrakci
  sem_t closing_mutex; // Mutex chranici hodnotu uzavreni atrakce

  CartInfo *carts; // Pole informaci o jednotlivych vozicich
} Shared;