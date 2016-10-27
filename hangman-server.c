/**
 * @file hangman-server.c
 * @author David Walter, 1426861
 * @date 2016-01-06
 * @brief The hangman server. Manages games from hangmna clients
 */
#include "hangman.h"

char *failureDrawing[10];

char **words;
int wordCount;

struct hangmanList {
    struct hangmanData data;
    struct hangmanList *next;
};

struct hangmanList *hangmanListHead;

/**
 * Determines how many clients are connected
 * @return  Number of clients
 */
static int calcClients(void) {
    if (hangmanListHead == NULL) {
        return 0;
    } else {
        int count = 0;

        for (struct hangmanList *head = hangmanListHead; head != NULL; head = head->next) {
            count++;
        }

        return count;
    }
}

/**
 * Adds a new Client to the list
 * @return ID of the new client
 */
static struct hangmanData *addClient(int id) {
    struct hangmanList *newClient;

    if (hangmanListHead == NULL) {
        hangmanListHead = (struct hangmanList *)malloc(sizeof(struct hangmanList));
        newClient = hangmanListHead;
    } else {
        struct hangmanList *newElem = (struct hangmanList *)malloc(sizeof(struct hangmanList));
        newElem->next = hangmanListHead->next;
        hangmanListHead->next = newElem;
        newClient = newElem;
    }

    if (newClient != NULL) {
        newClient->data.id = id;
        newClient->data.status = 0;
        newClient->data.wrongGuesses = 0;
        newClient->data.index = -1;
        newClient->data.clientW = 0;
        newClient->data.clientL = 0;
        memset(&newClient->data.word, 0, sizeof(newClient->data.word));
        memset(&newClient->data.info, 0, sizeof(newClient->data.info));
        memset(&newClient->data.guessed, '_', sizeof(newClient->data.guessed));

        return &newClient->data;
    }

    return NULL;
}

/**
 * Returns the clients data from the list
 * @param  id ID obtained by the client
 * @return    hangmanData for specified ID
 */
static struct hangmanData *getClient(int id) {
    if (hangmanListHead == NULL) {
        return NULL;
    } else {
        for (struct hangmanList *head = hangmanListHead; head != NULL; head = head->next) {
            if (head->data.id == id) {
                return &head->data;
            }
        }

        return NULL;
    }
}

/**
 * Removes a client from the list
 * @param id ID from the client
 */
static void removeClient(int id) {
    if (hangmanListHead == NULL) {
        return;
    } else {
        struct hangmanList *before = NULL;

        for (struct hangmanList *head = hangmanListHead; head != NULL; head = head->next) {
            if (head->data.id == id) {
                if (before == NULL) {
                    hangmanListHead = head->next;
                } else {
                    before->next = head->next;
                }

                free(head);
                return;
            }

            before = head;
        }

        return;
    }
}

/**
 * Reads a file
 * @param file The file to read
 */
static void readFile(FILE *file);

/**
 * Adds a word to the wordlist
 * @param word word to add to the wordlist
 */
static void addWord(char *word) {
    if (words == NULL) {
        wordCount = 1;
        words = (char **)malloc(sizeof(word) * wordCount);
    } else {
        ++wordCount;
        words = (char **)realloc(words, sizeof(word) * wordCount);
    }

    if (words != NULL) {
        words[wordCount - 1] = (char *)malloc(MAX_WORD_LENGTH * sizeof(char));
        (void)strcpy(words[wordCount - 1], word);

        for (size_t i = 0; i < MAX_WORD_LENGTH; i++) {
            words[wordCount - 1][i] = (char)toupper(words[wordCount - 1][i]);
        }

        (void)strtok(words[wordCount - 1], "\n");
    }
}

/**
 * Replaces _ from a word if a correct letter was guessed
 * @param id    Client ID
 * @param index Index of the word the client is guessing
 */
static void clearWord(int id, int index) {
    long length = strlen(words[index]);

    for (size_t i = 0; i < length; i++) {
        for (size_t j = 0; j < 26; j++) {
            struct hangmanData *client = getClient(id);

            if (words[index][i] == client->guessed[j]) {
                client->word[i] = words[index][i];
            }
        }
    }
}

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

    while ((c = getopt(argc, argv, "?")) != -1) {
        switch (c) {
            case '?':
                usage();
                break;

            default:
                assert(0);
        }
    }

    if (argc > 2) {
        usage();
    } else if (argc == 1) {
        readFile(stdin);
    } else {
        FILE *file = fopen(argv[1], "r");

        if (file == NULL) {
            bail_out("Could not read file");
            exit(EXIT_FAILURE);
        } else {
            readFile(file);
        }

        fclose(file);
    }

    failureDrawing[0] = (char *)"\n\n\n\n\n\n";
    failureDrawing[1] = (char *)"\n\n\n\n\n/\n";
    failureDrawing[2] = (char *)"\n\n\n\n\n/ \\\n";
    failureDrawing[3] = (char *)"\n\n\n\n |\n/ \\\n";
    failureDrawing[4] = (char *)"\n\n\n |\n |\n/ \\\n";
    failureDrawing[5] = (char *)"\n\n /\n |\n |\n/ \\\n";
    failureDrawing[6] = (char *)"\n  /\n /\n |\n |\n/ \\\n";
    failureDrawing[7] = (char *)"   __\n  /\n /   \n |\n |\n/ \\\n";
    failureDrawing[8] = (char *)"   __\n  /  |\n /\n |\n |\n/ \\\n";
    failureDrawing[9] = (char *)"   __\n  /  |\n /   O\n |  /|\\\n |  / \\\n/ \\\n";

    // MARK: Signal

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // MARK: Shared Memory

    int shm = shm_open(SHM_NAME, O_CREAT | O_RDWR, PERMISSION);

    if (shm == -1) {
        bail_out("shm_open");
    }

    if (ftruncate(shm, sizeof(struct hangmanData)) == -1) {
        printf("%s\n", "ftruncate");
    }

    shared = (struct hangmanData *)mmap(NULL, sizeof(struct hangmanData), PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);

    if (shared == MAP_FAILED) {
        bail_out("mmap");
    }

    if (close(shm) == -1) {
        bail_out("close");
    }

    // MARK: Semaphore

    server = sem_open(SEM_SERVER, O_CREAT | O_EXCL, PERMISSION, 0);
    client = sem_open(SEM_CLIENT, O_CREAT | O_EXCL, PERMISSION, 0);
    locked = sem_open(SEM_LOCKED, O_CREAT | O_EXCL, PERMISSION, 1);

    if (server == SEM_FAILED) {
        bail_out("sem_open (SEM_SERVER)");
    }

    if (client == SEM_FAILED) {
        bail_out("sem_open (SEM_CLIENT)");
    }

    if (locked == SEM_FAILED) {
        bail_out("sem_open (SEM_LOCKED)");
    }

    // MARK: Server-Client

    while (1) {
        (void)printf("\n\nClients: %i\nWaiting for a client...", calcClients());
        (void)fflush(stdout);

        if (sem_wait(client) < 0) {
            bail_out("sem_wait(client)");
        }

        (void)printf("Client");

        int id = shared->id;

        if (shared->signal == 0) {
            struct hangmanData *clientData = getClient(id);

            if (clientData == NULL) {
                clientData = addClient(id);
            }

            (void)printf("(%i) [%i - %c]\n", id, clientData->status, shared->send);

            if (clientData->status > 1) {
                // In game

                int index = clientData->index;

                if (shared->send < 'A' || shared->send > 'Z') {
                    clientData->status = 3;
                    (void)strcpy(clientData->info, "Invalid input.");
                } else if (strchr(clientData->guessed, shared->send) == NULL) {
                    int no = (int)shared->send - 65;
                    clientData->guessed[no] = shared->send;

                    if (strchr(words[index], shared->send)) {
                        clearWord(id, index);
                    } else {
                        clientData->wrongGuesses++;
                    }

                    (void)strcpy(clientData->info, failureDrawing[clientData->wrongGuesses]);
                    clientData->status = 2;

                    if (clientData->wrongGuesses == 9) {
                        clientData->status = 0;
                        clientData->clientL++;
                        (void)strcpy(clientData->word, words[index]);
                    } else {
                        if (!strchr(clientData->word, '_')) {
                            clientData->status = 0;
                            clientData->clientW++;
                        }
                    }
                } else {
                    clientData->status = 3;
                    (void)strcpy(clientData->info, "Already guessed.");
                }
            } else if (clientData->status >= 0) {
                // Not in game

                if (shared->send == 'Y') {
                    clientData->status = 2;
                    clientData->index++;

                    if (clientData->index < wordCount) {
                        memset(&clientData->word, 0, MAX_WORD_LENGTH);
                        memset(&clientData->word, '_', strlen(words[clientData->index]));
                        clientData->wrongGuesses = 0;
                        (void)strcpy(clientData->info, failureDrawing[clientData->wrongGuesses]);
                        memset(&clientData->guessed, '_', 26);
                    } else {
                        clientData->status = -1;
                        strcpy(clientData->info, "No more words");
                    }
                } else if (shared->send == 'N') {
                    // N or Invalid input
                    clientData->status = -1;
                    (void)strcpy(clientData->info, "Quit game");
                } else {
                    clientData->status = 1;
                    (void)strcpy(clientData->info, "Invalid input");
                }
            }
        } else {
            (void)printf("(%i)\nClient disconnected, free resources", id);
            getClient(id)->status = -1;
            (void)strcpy(getClient(id)->info, "Client shutdown");
        }

        struct hangmanData *clientData = getClient(id);

        shared->status = clientData->status;
        shared->wrongGuesses = clientData->wrongGuesses;
        shared->index = clientData->index;
        shared->clientW = clientData->clientW;
        shared->clientL = clientData->clientL;
        (void)strcpy(shared->guessed, clientData->guessed);
        (void)strcpy(shared->word, clientData->word);
        (void)strcpy(shared->info, clientData->info);

        if (sem_post(server) < 0) {
            bail_out("sem_post(server)");
        }

        if (clientData->status == -1) {         // Client should be removed
            removeClient(id);
        }
    }
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

/**
 * Recursive free on hangmanlost
 * @param next Item to free
 */
static void free_hangman(struct hangmanList *next) {
    if (next == NULL) {
        return;
    } else {
        free_hangman(next->next);
        free(next);
    }
}

static void free_alloc(void) {
    free_hangman(hangmanListHead);

    for (size_t i = 0; i < wordCount; i++) {
        free(words[i]);
    }

    free(words);

    if (client != NULL) {
        (void)sem_close(client);
    }

    if (server != NULL) {
        (void)sem_close(server);
    }

    if (locked != NULL) {
        (void)sem_close(locked);
    }

    (void)sem_unlink(SEM_SERVER);
    (void)sem_unlink(SEM_CLIENT);
    (void)sem_unlink(SEM_LOCKED);

	(void)munmap(shared, sizeof(shared));

	if (shm_unlink(SHM_NAME) == -1) {
		(void)fprintf(stderr, "%s: shm_unlink\n", progname);
	}
}

static void readFile(FILE *file) {
    char c;
    char line[MAX_WORD_LENGTH];

    c = fgetc(file);
    int i = 0;

    while (c != EOF) {
        if (i < MAX_WORD_LENGTH && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
            line[i++] = c;
        } else if (c == '\n' || c == ' ') {
            line[i] = '\0';
            addWord(line);
            i = 0;
        }

        c = fgetc(file);
    }

    for (size_t i = 0; i < wordCount; i++) {
        (void)printf("%s\n", words[i]);
    }
}

static void signalHandler(int sig) {
    (void)printf("\nEXIT (%i)\n", sig);

    (void)strcpy(shared->info, "Server shutdown");
    shared->signal = -2;

    for (struct hangmanList *head = hangmanListHead; head != NULL; head = head->next) {
        (void)kill(head->data.id, SIGTERM);
    }

    free_alloc();
    exit(sig);
}
