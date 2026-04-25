#include "proj2.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

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

void dispatcher(Writer writer, Shared shared, int cart_gap) {
  sem_wait(writer.mutex);
  fprintf(writer.file_descriptor, "%i: D: started\n", ++*(writer.line_count));
  sem_post(writer.mutex);

  while (true) {
    sem_wait(shared.visitor_count_mutex);
    if (*shared.visitor_count > 0) {
      sem_wait(writer.mutex);
      fprintf(writer.file_descriptor, "%i: D: next cart\n",
              ++*writer.line_count);
      sem_post(writer.mutex);
      // next_cart_go();
      // sem_post(shared.dispatcher_signal);
    } else {
      sem_post(shared.visitor_count_mutex);
      break;
    }
    sem_post(shared.visitor_count_mutex);
    usleep(cart_gap);
  }

  sem_wait(writer.mutex);
  fprintf(writer.file_descriptor, "%i: D: closing\n", ++*writer.line_count);
  sem_post(writer.mutex);

  *shared.dispatcher_kill_sig = true;
  exit(0);
}

void visitor(Writer writer, Shared shared, int idx, int queue_time,
             int cart_capacity) {
  sem_wait(writer.mutex);
  fprintf(writer.file_descriptor, "%i: N %i: started\n", ++*writer.line_count,
          idx);
  sem_post(writer.mutex);

  usleep(queue_time);

  sem_wait(writer.mutex);
  fprintf(writer.file_descriptor, "%i: N %i: queue\n", ++*writer.line_count,
          idx);
  sem_post(writer.mutex);

  sem_wait(shared.board_signal);

  sem_wait(writer.mutex);
  fprintf(writer.file_descriptor, "%i: N %i: boarding\n", ++*writer.line_count,
          idx);
  sem_post(writer.mutex);

  sem_wait(shared.boarded_count_mutex);
  sem_wait(shared.visitor_count_mutex);
  if (++(*shared.boarded_count) == cart_capacity ||
      *(--shared.visitor_count) == 0) {
    sem_post(shared.go_signal);
    *shared.boarded_count = 0;
  }
  sem_post(shared.boarded_count_mutex);

  sem_wait(shared.unboard_signal);

  sem_wait(writer.mutex);
  fprintf(writer.file_descriptor, "%i: N %i: leaving\n", ++*writer.line_count,
          idx);
  sem_post(writer.mutex);

  sem_wait(shared.unboarded_count_mutex);
  if (++*shared.unboarded_count == 20) {
    sem_post(shared.finished_signal);
    *shared.unboarded_count = 0;
  }
  sem_post(shared.unboarded_count_mutex);

  exit(0);
}

void cart(Writer writer, Shared shared, int idx, int cart_capacity,
          int ride_length, int cart_count) {
  sem_wait(writer.mutex);
  fprintf(writer.file_descriptor, "%i: V %i: started\n", ++*writer.line_count,
          idx);
  sem_post(writer.mutex);

  while (!shared.dispatcher_kill_sig) {
    sem_wait(shared.ready_cart_sems[idx]);
    sem_wait(writer.mutex);
    fprintf(writer.file_descriptor, "%i: V %i: boarding started\n",
            ++*writer.line_count, idx);
    sem_post(writer.mutex);

    for (unsigned idx = 0; idx < cart_capacity; idx++)
      sem_post(shared.board_signal);

    sem_wait(shared.boarding_done);

    sem_wait(writer.mutex);
    fprintf(writer.file_descriptor, "%i: V %i: boarding complete\n",
            ++*writer.line_count, idx);
    sem_post(writer.mutex);

    usleep(ride_length);

    sem_wait(shared.finished_cart_sems[idx]);

    sem_wait(writer.mutex);
    fprintf(writer.file_descriptor, "%i: V %i: leaving started\n",
            ++*writer.line_count, idx);
    sem_post(writer.mutex);

    for (unsigned idx = 0; idx < cart_capacity; idx++)
      sem_post(shared.unboard_signal);

    sem_wait(shared.unboarding_done);

    sem_wait(writer.mutex);
    fprintf(writer.file_descriptor, "%i: V %i: leaving complete\n",
            ++*writer.line_count, idx);
    sem_post(writer.mutex);
  }

  sem_wait(writer.mutex);
  fprintf(writer.file_descriptor, "%i: V %i: closed\n", ++*writer.line_count,
          idx);
  sem_post(writer.mutex);

  exit(0);
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
    fprintf(stderr, "err: failed to parse one or more args\n");
    return ERROR_EXIT_CODE;
  }

  Writer writer;

  FILE *file_descriptor = fopen(FILE_NAME, FILE_OPEN_MODE);
  int *line_count = smalloc(sizeof(int));

  if (file_descriptor == NULL) {
    fprintf(stderr, "err: could not open/create proj2.out\n");
    return ERROR_EXIT_CODE;
  }

  writer.file_descriptor = file_descriptor;

  if (line_count == NULL) {
    fprintf(stderr,
            "err: failed to create shared memory for output line counter\n");
    return ERROR_EXIT_CODE;
  }

  *line_count = 0;
  writer.line_count = line_count;

  sem_t *file_mutex = semaphore_init(1);

  if (file_mutex == NULL) {
    fprintf(stderr, "err: failed to initialize file mutex\n");
    return ERROR_EXIT_CODE;
  }

  writer.mutex = file_mutex;

  for (int idx = 0; idx <= cart_count + visitor_count; idx++) {
    pid_t fork_return = fork();

    if (fork_return == 0) {
      // Dite

      if (idx == 0) {
        // dispecer
        // dispatcher(writer, )
      } else if (idx <= cart_count) {
        // vozik
      } else {
        // navstevnik
      }

      exit(0);
    } else if (fork_return == -1) {
      printf("neco se mooc pokazilo\n");
    }
  };

  for (int idx = 0; idx <= cart_count + visitor_count; idx++)
    wait(NULL);

  semaphore_free(file_mutex);
  munmap(line_count, sizeof(int));
  fclose(file_descriptor);
  return 0;
}
