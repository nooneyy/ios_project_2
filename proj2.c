#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>

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

/**
  @brief Funkce pro konverzi command-line argumentu na cislo.
  @param arg Retezec urciteho argumentu
  @param dest Ukazatel na promennou typu int, kde ma byt ulozen vysledek
  @param min Minimalni dovolena hodnota, kterou muze cislo nabyvat
  @param max Maximalni dovolena hodnota, kterou muze cislo nabyvat
  @return true pri uspechu konverze, jinak false
 */
bool parse_arg(char *arg, int *dest, int min, int max) {
  // Osetreni chyb, co se muzou stat
  // ale pravdepodobne nenastanou :P
  if (arg == NULL || dest == NULL || max < min)
    return false;

  // Pointer na posledni rozpoznany znak
  char *end;
  // Prevadime string na long
  long result = strtol(arg, &end, 10);

  // Je konvertovane cislo v dovolenem rozsahu?
  if (min > result || result > max)
    return false;

  // Doslo pri konverzi k chybe? (errno priznak nebo zadny rozpoznany znak nebo
  // posledni rozpoznany znak neni \0 terminator)
  if (errno != 0 || end == arg || *end != '\0')
    return false;

  // Vysledek muzeme castnout na int, protoze nam to bezpecne dovoluje rozsah
  *dest = (int)result;
  return true;
}

/**
  @brief Funkce pro alokaci sdilene pameti
  @param length Velikost pameti, co chceme alokovat v bytech
  @return NULL pri neuspesne alokaci, jinak ukazatel na pamet
 */
void *smalloc(size_t length) {
  int prot = PROT_READ | PROT_WRITE; // Procesy budou moct readovat + writovat do sdilene pameti
  int flags = MAP_SHARED | MAP_ANONYMOUS; // Pamet bude sdilena, ale anonymni - nebude se zapisovat do zadneho souboru

  // addr je NULL - tedy vybira kernel
  // fd - file descriptor je -1, nepouzivame sdilene soubory
  void *res = mmap(NULL, length, prot, flags, -1, 0);

  // Napodobujeme chovani mallocu pri neuspesne alokaci pameti
  if (res == MAP_FAILED)
    return NULL;

  return res;
}

/**
  @brief Funkce pro inicializaci sdileneho semaforu
  @param value Pocatecni hodnota semaforu (value >= 0)
  @return NULL pri nepovedene inicializaci, jinak ukazatel na semafor
 */
sem_t *semaphore_init(unsigned value) {
  sem_t *mem = smalloc(sizeof(sem_t));

  if (mem == NULL)
    return NULL; // Nepovedla se alokace sdilene pameti

  // pshared je 1 => semafor bude sdileny mezi procesy, ne mezi vlakny
  int res = sem_init(mem, 1, value);
  
  if (res == -1)
    return NULL; // Nepovedla se inicializace semaforu, TODO: dealokace pameti???

  return mem;
}

/**
  @brief Funkce pro uvolneni sdileneho semaforu
  @param sem Ukazatel na semafor
 */
void semaphore_free(sem_t *sem) {
  // Zatim zadna kontrola return hodnot, tzv. DON'T CARE jako rika doc. Fucik
  sem_destroy(sem);
  munmap(sem, sizeof(sem_t));
}


int main(int argc, char **argv) {
  if (argc != CORRECT_ARGC_COUNT) {
    fprintf(stderr, "err: incorrect argument count\n");
    return ERROR_EXIT_CODE;
  }

  int cart_count, visitor_count, cart_capacity, ride_length, queue_time,
      cart_gap;

  if (!parse_arg(argv[CART_COUNT_IDX], &cart_count, CART_COUNT_MIN,
                 CART_COUNT_MAX) ||
      !parse_arg(argv[VISITOR_COUNT_IDX], &visitor_count, VISITOR_COUNT_MIN,
                 VISITOR_COUNT_MAX) ||
      !parse_arg(argv[CART_CAPACITY_IDX], &cart_capacity, CART_CAPACITY_MIN,
                 CART_CAPACITY_MAX) ||
      !parse_arg(argv[RIDE_LENGTH_IDX], &ride_length, RIDE_LENGTH_MIN,
                 RIDE_LENGTH_MAX) ||
      !parse_arg(argv[QUEUE_TIME_IDX], &queue_time, QUEUE_TIME_MIN,
                 QUEUE_TIME_MAX) ||
      !parse_arg(argv[CART_GAP_IDX], &cart_gap, CART_GAP_MIN, CART_GAP_MAX)) {
    fprintf(stderr, "err: failed to parse arg\n");
    return ERROR_EXIT_CODE;
  }

  sem_t *s1 = semaphore_init(0);
  sem_t *s2 = semaphore_init(0);

  if (s1 == NULL || s2 == NULL) {
    fprintf(stderr, "err: failed to initialize semaphores\n");
    return ERROR_EXIT_CODE;
  }

  int ret = fork();

  /* Test nasledujici posloupnosti zprav:
    D1
    H1
    D2
  */ 
  if (ret == 0) {
    // Dispecer
    printf("D1\n");
    sem_post(s1);
    sem_wait(s2);
    printf("D2\n");
  } else {
    // Hlavni proces
    sem_wait(s1);
    printf("H1\n");
    sem_post(s2);
    wait(NULL);
    semaphore_free(s1);
    semaphore_free(s2);
  }

  return 0;
}
