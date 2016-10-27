/**
 * @file hangman.h
 * @author David Walter, 1426861
 * @date 2016-01-06
 * @brief Common headerfile of hangman-client and hangman-server
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <signal.h>

#include <errno.h>

#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>

#define SHM_NAME        "/hangmanData"
#define SEM_SERVER      "/hangmanServer"
#define SEM_CLIENT      "/hangmanClient"
#define SEM_LOCKED      "/hangmanLOCKED"

#define PERMISSION      (0600)
#define MAX_WORD_LENGTH 128

sem_t *server;
sem_t *client;
sem_t *locked;

char *progname;

struct hangmanData {
    // Server
    short status; // 0(not ingame), 1(not ingame, wrong input), 2(ingame), 3(ingame, wrong input), -1(disconnect client), -2(server shutdown)
    char info[128];

    short wrongGuesses;
    int index;
    char word[MAX_WORD_LENGTH];
    char guessed[26];

    short clientW;
    short clientL;
    // Client
    int id;
    char send;
    short signal;
};

/**
 * Shared object between server and client
 */
struct hangmanData *shared;

/**
 * Exits the programm and writes a usage description to stderr
 */
static void usage(void);

/**
 * Exits the programm freeing allocated space and printing an error message to stderr
 * @param error Error description
 */
static void bail_out(char *error);

/**
 * Frees allocated space
 */
static void free_alloc(void);

/**
 * Handles signals
 * @param sig Int value of the signal
 */
static void signalHandler(int sig);
