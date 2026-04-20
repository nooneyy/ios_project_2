#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
  // int retv = fork();

  // if (retv == 0) {
  //   // Dispecer
  //   printf("hello from dispatcher!\n");
  // } else if (retv == -1) {
  //   // Error
  //   printf("Fork failed\n");
  // } else {
  //   // Hlavni proces, retv = pid dispecera
  //   printf("hello from the main process!\n");
  //   waitpid(retv);
  // }
  return 0;
}
