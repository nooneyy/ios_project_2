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
  int prot =
      PROT_READ |
      PROT_WRITE; // Procesy budou moct readovat + writovat do sdilene pameti
  int flags =
      MAP_SHARED | MAP_ANONYMOUS; // Pamet bude sdilena, ale anonymni - nebude
                                  // se zapisovat do zadneho souboru

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
    return NULL; // Nepovedla se inicializace semaforu, TODO: dealokace
                 // pameti???

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

void write_output(FILE *file_descriptor, sem_t *file_mutex, int *line_count,
                  char *line) {
  sem_wait(file_mutex);
  fprintf(file_descriptor, "%i: %s\n", ++*line_count, line);
  sem_post(file_mutex);
}

// void visitor(int idx, FILE *file_descriptor, sem_t *file_mutex,
// int *line_count) {
// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: N %i: started\n", idx, ++*line_count);
// fprintf(file_descriptor, "%i: N %i: queue\n", idx, ++*line_count);
// sem_post(file_mutex);
// enqueue();

// wait_for_boarding();
// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: N %i: boarding\n", idx, ++*line_count);
// sem_post(file_mutex);
// board() wait_for_unboard();

// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: N %i: leaving\n", idx, ++*line_count);
// sem_post(file_mutex);
//}

// void cart(int idx, FILE *file_descriptor, sem_t *file_mutex, int *line_count)
// { sem_wait(file_mutex); fprintf(file_descriptor, "%i: V %i: started\n", idx,
// ++*line_count); sem_post(file_mutex);

// while (true) {
// enqueue();
// wait_for_dispatcher();

// if (dispatcher_signal == board) {
// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: V %i: boarding started\n", idx,
//++*line_count);
// sem_post(file_mutex);

// tell_capacity_passengers_to_board();
// wait_boarded();

// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: V %i: boarding complete\n", idx,
//++*line_count);
// sem_post(file_mutex);

// sleep(ride_length);

// wait_for_leaving_freeup();

// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: V %i: leaving started\n", idx,
//++*line_count);
// sem_post(file_mutex);

// unboard();

// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: V %i: leaving complete\n", idx,
//++*line_count);
// sem_post(file_mutex);
//} else {
// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: V %i: closed\n", idx, ++*line_count);
// sem_post(file_mutex);
// break;
//}
//}
//}

// void dispatcher(FILE *file_descriptor, sem_t *file_mutex, int *line_count) {
// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: D: started\n", ++*line_count);
// sem_post(file_mutex);

// while (!noVisitors) {
// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: D: next cart\n", ++*line_count);
// sem_post(file_mutex);

// next_cart_go();
// sleep(cart_gap);
//}

// sem_wait(file_mutex);
// fprintf(file_descriptor, "%i: D: closing\n", ++*line_count);
// sem_post(file_mutex);

// carts_die();
//}

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
    fprintf(stderr, "err: failed to parse one or more args\n");
    return ERROR_EXIT_CODE;
  }

  FILE *file_descriptor = fopen(FILE_NAME, FILE_OPEN_MODE);
  int *line_count = smalloc(sizeof(int));

  if (file_descriptor == NULL) {
    fprintf(stderr, "err: could not open/create proj2.out\n");
    return ERROR_EXIT_CODE;
  }

  if (line_count == NULL) {
    fprintf(stderr,
            "err: failed to create shared memory for output line counter\n");
    return ERROR_EXIT_CODE;
  }

  *line_count = 0;

  sem_t *file_mutex = semaphore_init(1);

  if (file_mutex == NULL) {
    fprintf(stderr, "err: failed to initialize file mutex\n");
    return ERROR_EXIT_CODE;
  }

  for (int idx = 0; idx <= cart_count + visitor_count; idx++) {
    pid_t fork_return = fork();

    if (fork_return == 0) {
      // Dite

      if (idx == 0) {
        // dispecer
        printf("dispecer test \n");
      } else if (idx <= cart_count) {
        // vozik
        printf("vozik test\n");
      } else {
        // navstevnik
        printf("navstevnik test\n");
      }

      exit(0);
    } else if (fork_return == -1) {
      printf("neco se mooc pokazilo\n");
    }
  };

  for (int idx = 0; idx <= cart_count + visitor_count; idx++) {
    wait(NULL);
  }

  // if (ret == 0) {
  //   // Dispecer
  //   sem_wait(file_mutex);
  //   fprintf(file_descriptor, "%i: D: started\n", ++line_count);
  //   sem_post(file_mutex);
  //   sem_post(writing_done);
  //   exit(0);
  // } else {
  //   // Hlavni proces
  //   sem_wait(writing_done);
  //   printf("Main process\n");
  //   wait(NULL);
  // }

  semaphore_free(file_mutex);
  munmap(line_count, sizeof(int));
  fclose(file_descriptor);
  return 0;
}
