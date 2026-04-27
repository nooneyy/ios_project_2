#include "proj2.h"

/****************************
  IOS - PROJEKT 2
  autor: xkolart01
  datum: 20.04.2026
****************************/

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
  @brief Funkce pro zjednoduseni vypisu radku procesu do vystupniho souboru
  @param file_descriptor Ukazatel na vystupni soubor
  @param shared Ukazatel na struct sdilenych hodnot
  @param msg Formatovaci retezec zpravy (jako prvni argument printf)
  @param ... N dalsich hodnot, co dosadit do formatovacicho retezce
 */
void write_output(FILE *file_descriptor, Shared *shared, char *msg, ...) {
  sem_wait(
      &shared->output_mutex); // Cekame na mutex k psani do vystupniho souboru

  // Vypiseme prvni cast radku, tedy "{cislo_radku}: "
  fprintf(file_descriptor, "%i: ", ++shared->output_line_count);

  va_list args;
  va_start(args, msg); // Chceme vsechny argumenty po argumentu msg

  // Vypiseme "prostredni cast" radku, tedy dany formatovany retezec s
  // prislusnymi hodnotami
  vfprintf(file_descriptor, msg, args);

  va_end(args); // Uvolnime/vycistime args

  fprintf(file_descriptor, "\n"); // Radek ukoncime newlinem

  sem_post(&shared->output_mutex); // Uvolnime mutex
}

/**
  @brief Funkce generujici nahodne cislo z intervalu <min, max>
  @param min Dolni hranice intervalu (vcetne), kde min >= 0
  @param max Horni hranice intervalu (vcetne), kde max >= min
  @return Nahodne cislo z intervalu <min, max> nebo -1 pri nedodrzeni podminek
  funkce
 */
int rand_in_range(int min, int max) {
  if (min > max ||
      min < 0) // Tyto podminky v nasem programu asi nenastanou ¯\_(ツ)_/¯
    return -1;

  return rand() % (max - min + 1) + min;
}

/**
  @brief Funkce inicializujici hodnoty ve structu sdilenych promennych Shared
  @param output Ukazatel na vystupni struct sdilenych hodnot mezi procesy (jiz
  alokovany pomoci smalloc)
  @param visitor_count Pocet navstevniku atrakce
  @param cart_count Pocet voziku atrakce
  @return true pri uspesne alokaci, jinak false
 */
bool init_shared_values(Shared *output, int visitor_count, int cart_count) {
  // Inicializace citace radku ve vystupnim souboru (ochrana mutexem)
  output->output_line_count = 0;
  if (sem_init(&output->output_mutex, 1, 1) != SEMAPHORE_INIT_SUCCESS)
    return false;

  // Inicializace poctu zbyvajicich navstevniku (ti, co se jeste nesvezli,
  // ochrana taky mutexem)
  output->visitors_left = visitor_count;
  if (sem_init(&output->visitors_left_mutex, 1, 1) != SEMAPHORE_INIT_SUCCESS)
    return false;

  // Inicializace indexu prvniho voziku (toto cislo stejne bude prepsano)
  output->boarding_cart_idx = 0;

  // Inicializace poctu navstevniku, kteri jiz nastoupili do voziku, ktery ted
  // bude odjizdet (bude prepsano)
  output->boarded_visitors = 0;

  // Inicializace nekolika mutexu a signalu souvisejicich s odjezdem voziku
  // sem_init value 1... mutex, 0... semafor/signal
  if (sem_init(&output->boarding_mutex, 1, 1) != SEMAPHORE_INIT_SUCCESS)
    return false;
  if (sem_init(&output->board_signal, 1, 0) != SEMAPHORE_INIT_SUCCESS)
    return false;
  if (sem_init(&output->boarding_done, 1, 0) != SEMAPHORE_INIT_SUCCESS)
    return false;
  if (sem_init(&output->cart_departed, 1, 0) != SEMAPHORE_INIT_SUCCESS)
    return false;

  // Atrakce bude zpocatku otevrena (chraneno mutexem)
  output->closing = false;
  if (sem_init(&output->closing_mutex, 1, 1) != SEMAPHORE_INIT_SUCCESS)
    return false;

  // Alokace pole delky cart_count s informacemi o jednotlivych vozicich
  output->carts = smalloc(sizeof(CartInfo) * cart_count);

  if (output->carts == NULL)
    return false;

  for (int idx = 0; idx < cart_count; idx++) {
    // Inicializace priznaku, ze vozik je dostupny k volani dispecerem (tedy
    // nejede jiz po trati nebo nevyklada navstevniky)
    if (sem_init(&output->carts[idx].is_available, 1, 1) !=
        SEMAPHORE_INIT_SUCCESS)
      return false;
    // Inicializace signalu od dispecera, ze vozik muze zacit nabirat pasazery
    if (sem_init(&output->carts[idx].is_ready, 1, 0) != SEMAPHORE_INIT_SUCCESS)
      return false;
    // Inicializace signalu od predchoziho voziku, ze muze odjet
    if (sem_init(&output->carts[idx].can_leave_station, 1, 0) !=
        SEMAPHORE_INIT_SUCCESS)
      return false;
    if (sem_init(&output->carts[idx].unboard_signal, 1, 0) !=
        SEMAPHORE_INIT_SUCCESS)
      return false;
    if (sem_init(&output->carts[idx].unboarding_done, 1, 0) !=
        SEMAPHORE_INIT_SUCCESS)
      return false;

    // Kazdy vozik ma zpocatku 0 sedicich pasazeru a 0 pasazeru, co jiz
    // vystoupili
    output->carts[idx].visitor_count = 0;
    output->carts[idx].unboarded_visitors = 0;
  }

  // Prvnimu voziku umoznime hned opustit vykladaci stanici (nema zadneho
  // predchudce, ktery by mu stanici uvolnil)
  sem_post(&output->carts[0].can_leave_station);

  return true;
}

/**
  @brief Funkce uvolnujici struct sdilene pameti Shared a promennych v nem
  @param shared Ukazatel na struct sdilenych hodnot mezi procesy
  @param cart_count Pocet voziku atrakce
 */
void free_shared_values(Shared *shared, int cart_count) {
  for (int idx = 0; idx < cart_count; idx++) {
    // Pro kazdy vozik uvolnime semafory v jeho CartInfo structu
    sem_destroy(&shared->carts[idx].unboarding_done);
    sem_destroy(&shared->carts[idx].unboard_signal);
    sem_destroy(&shared->carts[idx].can_leave_station);
    sem_destroy(&shared->carts[idx].is_ready);
    sem_destroy(&shared->carts[idx].is_available);
  }

  // Uvolnime sdilenou pamet pole informaci o vozicich
  munmap(shared->carts, sizeof(CartInfo) * cart_count);

  // Uvolnime zbyle sdilene hodnoty
  sem_destroy(&shared->closing_mutex);
  sem_destroy(&shared->cart_departed);
  sem_destroy(&shared->boarding_done);
  sem_destroy(&shared->board_signal);
  sem_destroy(&shared->boarding_mutex);
  sem_destroy(&shared->visitors_left_mutex);
  sem_destroy(&shared->output_mutex);

  // Uvolnime sdilenou pamet celeho structu
  munmap(shared, sizeof(Shared));
}

/**
  @brief Zapouzdreni podminky pro provoz dispecera ve funkci
  @param shared Ukazatel na struct sdilenych hodnot mezi procesy
  @return true jestli stale cekaji navstevnici na atrakci, jinak false
 */
bool are_visitors_left(Shared *shared) {
  bool ret; // Hodnota, kterou budeme vracet

  sem_wait(&shared->visitors_left_mutex); // Cekame na mutex
  ret = shared->visitors_left > 0; // Pocet zbyvajicich navstevniku musi byt > 0
  sem_post(&shared->visitors_left_mutex); // Uvolnime mutex
  return ret;
}

/**
  @brief Funkce zivotniho cyklu procesu dispecera
  @param file_descriptor Ukazatel na vystupni soubor
  @param shared Ukazatel na struct sdilenych hodnot mezi procesy
  @param cart_gap Bezpecnostni odstup mezi voziky
  @param cart_count Pocet voziku v atrakci
 */
void dispatcher(FILE *file_descriptor, Shared *shared, int cart_gap,
                int cart_count) {
  write_output(file_descriptor, shared, "D: started");

  /*
    loop_count ... pocet iteraci for loopu, vyuzivame pro frontu cekajicich
    voziku

    podminka ... dispecer funguje, zatimco nam zbyvaji navstevnici cekajici na
    atrakci
  */
  for (int loop_count = 0; are_visitors_left(shared); loop_count++) {
    // Index voziku v poli informaci o vozicich
    int cart_idx = loop_count % cart_count;

    // Je vozik dostupny k prijezdu na nastup? (Prave nejede ci nevyklada
    // navstevniky)
    sem_wait(&shared->carts[cart_idx].is_available);

    write_output(file_descriptor, shared, "D: next cart");

    // Signalizujeme voziku, ze muze zacit nabirat navstevniky
    sem_post(&shared->carts[cart_idx].is_ready);

    // Cekame, nez vozik odjede
    sem_wait(&shared->cart_departed);

    // Cekame po dobu bezpecne vzdalenosti mezi voziky
    usleep(cart_gap);
  }

  write_output(file_descriptor, shared, "D: closing");

  // Uzavreme atrakci
  sem_wait(&shared->closing_mutex);
  shared->closing = true;
  sem_post(&shared->closing_mutex);

  // Vsem vozikum signalizujeme, ze muzou "odjet", normalne totiz cekaji na
  // pokyn od dispecera, tento pokyn jim dame, aby mohli ukoncit svou cinnost
  for (int idx = 0; idx < cart_count; idx++) {
    sem_post(&shared->carts[idx].is_ready);
  }
}

/**
  @brief Funkce zivotniho cyklu procesu navstevnika
  @param file_descriptor Ukazatel na vystupni soubor
  @param shared Ukazatel na struct sdilenych hodnot mezi procesy
  @param idx Index (tedy ID) navstevnika zacinajici od 1
  @param queue_time Maximalni doba zarazeni navstevnika do fronty na atrakci
 */
void visitor(FILE *file_descriptor, Shared *shared, int idx, int queue_time) {
  write_output(file_descriptor, shared, "N %i: started", idx);

  // Navstevnik se zaradi do fronty na atrakci v intervalu <0, queue_time>
  usleep(rand_in_range(0, queue_time));

  write_output(file_descriptor, shared, "N %i: queue", idx);

  // Cekame na pokyn k nastupu do voziku
  sem_wait(&shared->board_signal);

  // Ziskame index voziku, do ktereho nastupujeme
  sem_wait(&shared->boarding_mutex);
  int cart_idx = shared->boarding_cart_idx;
  sem_post(&shared->boarding_mutex);

  write_output(file_descriptor, shared, "N %i: boarding", idx);

  sem_wait(&shared->boarding_mutex);
  // Nastoupime, zvysime pocet navstevniku, co nastoupili na vozik
  if (++shared->boarded_visitors == shared->carts[cart_idx].visitor_count) {
    // Jsme-li poslednim nastupujicim, boarded_visitors vynulujeme pro dalsi
    // vozik
    shared->boarded_visitors = 0;
    // Signalizujeme, ze nastupovani bylo ukonceno
    sem_post(&shared->boarding_done);
  }
  sem_post(&shared->boarding_mutex);

  // Cekame na signalizaci od voziku, ze mame vystoupit
  sem_wait(&shared->carts[cart_idx].unboard_signal);

  write_output(file_descriptor, shared, "N %i: leaving", idx);

  sem_wait(&shared->boarding_mutex);
  // Vystoupime, zvysime pocet navstevniku, co vystoupili z voziku
  if (++shared->carts[cart_idx].unboarded_visitors ==
      shared->carts[cart_idx].visitor_count) {
    // Jsme-li poslednim vystupujicim, unboarded_visitors vynulujeme pro dalsi
    // vystupujici
    shared->carts[cart_idx].unboarded_visitors = 0;
    // Voziku signalizujeme, ze vystupovani bylo ukonceno
    sem_post(&shared->carts[cart_idx].unboarding_done);
  }
  sem_post(&shared->boarding_mutex);
}

/**
  @brief Funkce zivotniho cyklu procesu voziku
  @param file_descriptor Ukazatel na vystupni soubor
  @param shared Ukazatel na struct sdilenych hodnot mezi procesy
  @param idx Index (tedy ID) voziku zacinajici od 1
  @param cart_capacity Maximalni kapacita navstevniku, kteri se do voziku vlezou
  @param ride_length Doba jizdy voziku po draze
  @param cart_count Pocet voziku v atrakci
 */
void cart(FILE *file_descriptor, Shared *shared, int idx, int cart_capacity,
          int ride_length, int cart_count) {
  // Index voziku v poli informaci o vozicich
  int cart_idx = idx - 1;

  write_output(file_descriptor, shared, "V %i: started", idx);

  // Nekonecny cyklus
  while (true) {
    // Cekame na pokyn dispecera
    sem_wait(&shared->carts[cart_idx].is_ready);

    sem_wait(&shared->closing_mutex);
    // Atrakce se zavira? Konec procesu
    if (shared->closing) {
      sem_post(&shared->closing_mutex);
      break;
    }
    sem_post(&shared->closing_mutex);

    write_output(file_descriptor, shared, "V %i: boarding started", idx);

    sem_wait(&shared->boarding_mutex);
    sem_wait(&shared->visitors_left_mutex);
    // Pocet navstevniku, co se ma svezt ... pocet zbyvajicich navstevniku, co
    // ceka na svezeni nebo kapacita voziku, to, co je z nich mensi
    shared->carts[cart_idx].visitor_count =
        shared->visitors_left < cart_capacity ? shared->visitors_left
                                              : cart_capacity;
    // Odecteme navstevniky, co vozikem pojedou z tech, kteri cekaji na svezeni
    shared->visitors_left -= shared->carts[cart_idx].visitor_count;
    sem_post(&shared->visitors_left_mutex);

    // "Pojedeme" do stanice ke vstupu navstevniku
    shared->boarding_cart_idx = cart_idx;

    // Signalizujeme n navstevnikum, aby nastoupili (kde n je pocet navstevniku,
    // co se ma svezt)
    for (int visitor_idx = 0;
         visitor_idx < shared->carts[cart_idx].visitor_count; visitor_idx++)
      sem_post(&shared->board_signal);

    sem_post(&shared->boarding_mutex);

    // Cekame na to, nez vsichni navstevnici nastoupi
    sem_wait(&shared->boarding_done);

    write_output(file_descriptor, shared, "V %i: boarding complete", idx);

    // Signalizujeme dispecerovi, ze vozik odjel na drahu
    sem_post(&shared->cart_departed);

    // Po draze jedeme (cekame) v intervalu <ride_length / 2, ride_length>
    usleep(rand_in_range(ride_length / 2, ride_length));

    // Cekame na pokyn od voziku, co jel pred nami, ze muzeme zajet do stanice,
    // kde vystupuji navstevnici
    sem_wait(&shared->carts[cart_idx].can_leave_station);

    write_output(file_descriptor, shared, "V %i: leaving started", idx);

    // Signalizujeme n navstevnikum, aby vystoupili (n je pocet navstevniku, co
    // se svezli)
    for (int visitor_idx = 0;
         visitor_idx < shared->carts[cart_idx].visitor_count; visitor_idx++)
      sem_post(&shared->carts[cart_idx].unboard_signal);

    // Cekame na to, nez vsichni navstevnici vystoupili
    sem_wait(&shared->carts[cart_idx].unboarding_done);

    write_output(file_descriptor, shared, "V %i: leaving complete", idx);

    // Signalizujeme nasledujicimu voziku, ze muze zajet do vystupni stanice
    sem_post(&shared->carts[(cart_idx + 1) % cart_count].can_leave_station);
    // Tento vozik znovu zpristupnime dispecerovi k povolani k jizde
    sem_post(&shared->carts[cart_idx].is_available);
  }

  write_output(file_descriptor, shared, "V %i: closed", idx);
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

  FILE *file_descriptor = fopen(FILE_NAME, FILE_OPEN_MODE);

  if (file_descriptor == NULL) {
    fprintf(stderr, "err: could not open/create proj2.out\n");
    return ERROR_EXIT_CODE;
  }

  // Vypneme bufferovani u vystupniho souboru, abychom dodrzeli spravne poradi
  // vystupu
  setbuf(file_descriptor, NULL);

  Shared *shared = smalloc(sizeof(Shared));

  if (shared == NULL) {
    fprintf(stderr, "err: failed to initialize struct of shared values\n");
    fclose(file_descriptor);
    return ERROR_EXIT_CODE;
  }

  // Neuspesna alokace sdilenych hodnot
  if (!init_shared_values(shared, visitor_count, cart_count)) {
    fprintf(stderr, "err: failed to initialize shared values\n");
    fclose(file_descriptor);
    munmap(shared, sizeof(Shared));
    return ERROR_EXIT_CODE;
  }

  // Vytvorime (cart_count + visitor_count + 1) procesu
  for (int idx = 0; idx <= cart_count + visitor_count; idx++) {
    pid_t fork_return = fork();

    if (fork_return == 0) {
      // Dite

      // Pro kazdy proces vygenerujeme seed (id procesu) pro funkci rand()
      srand(getpid());

      if (idx == 0) {
        // Proces bude dispecer
        dispatcher(file_descriptor, shared, cart_gap, cart_count);
      } else if (idx <= cart_count) {
        // Proces bude vozik
        cart(file_descriptor, shared, idx, cart_capacity, ride_length,
             cart_count);
      } else {
        // Proces bude navstevnik
        visitor(file_descriptor, shared, idx - cart_count, queue_time);
      }
      // Konec funkce zivotniho cyklu procesu? Exitneme s kodem 0
      exit(0);
    } else if (fork_return == -1) {
      fprintf(stderr, "err: failed to fork a child process\n");
      free_shared_values(shared, cart_count);
      fclose(file_descriptor);
      return ERROR_EXIT_CODE;
    }
  };

  // Hlavni proces ceka na ukonceni vsech ostatnich procesu pred tim, nez skonci
  for (int idx = 0; idx <= cart_count + visitor_count; idx++)
    wait(NULL);

  free_shared_values(shared, cart_count);
  fclose(file_descriptor);
  return 0;
}
