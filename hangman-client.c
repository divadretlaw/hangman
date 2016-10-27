/**
 * @file hangman-client.c
 * @author David Walter, 1426861
 * @date 2016-01-06
 * @brief The hangman client that connects to a hangman server
 */
#include "hangman.h"

short isLocked = 0;

int id;

/**
 * Main
 * @brief     Main Function
 * @param     argc Number of arguments
 * @param     argv Array of arguments of type char*
 * @result    int EXIT_SUCCESS or in case of an error EXIT_FAILURE
 */
int main(int argc, char *argv[]) {
    progname = argv[0];

    int c;

    while ( (c = getopt(argc, argv, "")) != -1) {
        switch (c) {
            case '?': {
                usage();
                break;
            }

            default: {
                assert(0);
            }
        }
    }

    if (argc != 1) {
        usage();
    }

    (void)signal(SIGINT, signalHandler);
    (void)signal(SIGTERM, signalHandler);

    // MARK: Semaphore

    server = sem_open(SEM_SERVER, 0);
    client = sem_open(SEM_CLIENT, 0);
    locked = sem_open(SEM_LOCKED, 0);

    if (server == SEM_FAILED || client == SEM_FAILED || locked == SEM_FAILED) {
        bail_out("Could not connect to server");
    }

    // MARK: Shared Memory

    int shm = shm_open(SHM_NAME, O_RDWR, PERMISSION);

    if (shm == -1) {
        bail_out("Could not connect to server");
    }

    // if (ftruncate(shm, sizeof(struct hangmanData)) == -1) { bail_out("ftruncate"); }

    shared = (struct hangmanData *)mmap(NULL, sizeof(struct hangmanData), PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);

    if (shared == MAP_FAILED) {
        bail_out("mmap");
    }

    if (close(shm) == -1) {
        bail_out("close");
    }

    (void)printf("Trying to connect to server...");
    fflush(stdout);

    if (sem_wait(locked) < 0) {      // Wait until unlocked
        bail_out("sem_wait(locked)");
    }

    isLocked = 1;

    id = getpid();
    shared->id = id;
    shared->signal = 0;
    shared->send = '\0';
    // shared->send = 'Y';

    if (sem_post(client) < 0) {     // Send answer to server
        bail_out("sem_wait(client)");
    }

    if (sem_wait(server) < 0) {     // Wait for answer from server
        bail_out("sem_wait(server)");
    }

    id = shared->id;
    (void)printf("Obtained ID: %i\n", id);

    if (sem_post(locked) < 0) {     // Unlock
        bail_out("sem_post(locked)");
    }

    isLocked = 0;

    char send = 'Y';

    while (shared->status > -2) {
        if (sem_wait(locked) < 0) {              // Wait until unlocked
            bail_out("sem_wait(locked)");
        }

        isLocked = 1;

        // MARK: Client write

        shared->id = id;
        shared->send = send;
        shared->signal = 0;

        if (sem_post(client) < 0) {             // Send answer to server
            bail_out("sem_wait(client)");
        }

        if (sem_wait(server) < 0) {         // Wait for answer from server
            bail_out("sem_wait(server)");
        }

        // MARK: Client read

        int status = shared->status;
        // int wg = shared->wrongGuesses;

        if (shared->status == -1) {
            (void)printf("\n_______________________________________________________________________________\n");
            (void)printf("Server: %s (%i W, %i L)\nExit Game\n", shared->info, shared->clientW, shared->clientL);

            if (sem_post(locked) < 0) {                 // Unlock
                bail_out("sem_post(locked)");
            }

            free_alloc();
            exit(EXIT_SUCCESS);
        } else if (status == 2) {
            (void)printf("\n_______________________________________________________________________________\n");
            printf("Word to guess: %s\n", shared->word);

            for (int i = 0; i < 26; i++) {
                (void)printf("%c ", shared->guessed[i]);
            }

            (void)printf("\nWrong guesses: %i/9\n%s\n", shared->wrongGuesses, shared->info);
            // (void)printf("\nWrong guesses: %i/9\n%s\n", wg, failureDrawing[wg]);
        } else if (status == 0) {
            (void)printf("\n_______________________________________________________________________________\n");
            printf("The word was: %s\n", shared->word);

            for (int i = 0; i < 26; i++) {
                (void)printf("%c ", shared->guessed[i]);
            }

            (void)printf("\nWrong guesses: %i/9\n%s\n", shared->wrongGuesses, shared->info);
            // (void)printf("\nWrong guesses: %i/9\n%s\n", wg, failureDrawing[wg]);

            if (shared->wrongGuesses == 9) {
                (void)printf("You LOSE (%i W, %i L)\n_______________________________________________________________________________\n", shared->clientW, shared->clientL);
            } else {
                (void)printf("You WIN (%i W, %i L)\n_______________________________________________________________________________\n", shared->clientW, shared->clientL);
            }
        } else if (status == 1 || status == 3) {
            (void)fprintf(stderr, "%s\n", shared->info);
        }

        if (sem_post(locked) < 0) {             // Unlock
            bail_out("sem_post(locked)");
        }

        isLocked = 0;

        char input[512];

        if (status >= 2) {
            (void)printf("Guess a letter: ");

            if (fgets(input, MAX_WORD_LENGTH, stdin) != NULL) {
                send = input[0];
            }

            if (send >= 'a' && send <= 'z') {
                send -= 32;
            }

            // printf("SENDING: %c\n", send);
        } else {
            (void)printf("Do you want to play another game (Y/N)? ");

            if (fgets(input, MAX_WORD_LENGTH, stdin) != NULL) {
                send = input[0];
            }

            if (send >= 'y' || send == 'n') {
                send -= 32;
            }
        }
    }

    free_alloc();
    (void)printf("\nEXIT (%i)\n", -2);
    exit(-2);
}

static void bail_out(char *error) {
    (void)fprintf(stderr, "%s: %s\n", progname, error);
    free_alloc();
    exit(EXIT_FAILURE);
}

static void usage(void) {
    (void)fprintf(stderr, "Usage: %s [input-file]\n", progname);
    exit(EXIT_FAILURE);
}

static void free_alloc(void) {
	(void)munmap(shared, sizeof(shared));
    (void)sem_close(client);
    (void)sem_close(server);
    (void)sem_close(locked);
}

static void signalHandler(int sig) {
    (void)printf("\n\n");

    if (sig == 2) {
        if (isLocked) {
            shared->id = id;
            shared->signal = 1;
            (void)sem_post(locked);
        } else {
            (void)sem_wait(locked);
            shared->id = id;
            shared->signal = 1;
            (void)sem_post(client);
            (void)sem_wait(server);
            (void)sem_post(locked);
        }
    }

    (void)printf("EXIT (%s, Code: %i)\n", shared->info, sig);
    free_alloc();
    exit(sig);
}
