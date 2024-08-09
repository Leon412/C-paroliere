#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/select.h>
#include <ctype.h>
#include <stdarg.h>
#include <getopt.h>
#include "messages.h"
#include "hashmap.h"
#include "list.h"
#include "queue.h"

#define BUFFER_SIZE 1024

#define MAX_NUM_CLIENTS 32
#define DEFAULT_GAME_LENGTH 180
#define PAUSE_LENGTH 60
#define USERNAME_MAX_LENGTH 10
#define DEFAULT_DICTIONARY_PATH "../data/dictionary_ita.txt"

#define MSG_OK 'K'
#define MSG_ERR 'E'
#define MSG_REGISTRA_UTENTE 'R'
#define MSG_MATRICE 'M'
#define MSG_TEMPO_PARTITA 'T'
#define MSG_TEMPO_ATTESA 'A'
#define MSG_PAROLA 'W'
#define MSG_PUNTI_FINALI 'F'
#define MSG_PUNTI_PAROLA 'P'

typedef struct Game {
    char board[4][4]; //la matrice di gioco corrente

    char *boards_file_name; //opzione --matrici
    char **boards_from_file;
    int boards_from_file_size;
    int current_board_from_file;

    char *dictionary_file_name; //opzione --diz

    int status; //1 = partita | 0 = pausa
    time_t end_time;
    HashMap *clients;

    int game_length; //opzione --durata
    int pause_length;

    int seed; //opzione --seed

    Queue *scores; //coda dei punteggi condivisa tra client_handler thread e scorer
    char *scores_csv;
    int active_client_count; //il numero dei client connessi e registrati, quindi partecipanti al gioco
    int completed_client_count; //serve per far sapere allo scorer thread quando iniziare a calcolare la classifica
    int scores_done;
} Game;

typedef struct Client {
    int fd;
    struct sockaddr_in address;
    socklen_t address_len;
    pthread_t thread;

    int registered;
    char username[USERNAME_MAX_LENGTH + 1];

    List *past_guesses;
} Client;

typedef struct Score {
    char username[USERNAME_MAX_LENGTH + 1];
    int points;
} Score;

Game game;

pthread_mutex_t game_status_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t game_status_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t game_time_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t scores_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clients_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t scorer_cond = PTHREAD_COND_INITIALIZER;

void timer_handler(int sig);
void set_game_timer(int duration);

void print_board(char board[4][4]) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(board[i][j] == 'Q') {
                printf("Qu ");
            }
            else {
                printf("%c  ", board[i][j]);
            }
        }
        printf("\n");
    }
}

int count_boards_in_file(char *boards_file_name) {
    FILE *boards = fopen(boards_file_name, "r");
    if (!boards) {
        perror("Error opening file");
        return -1;
    }

    int num_boards = 0;
    char board[64];
    while (fgets(board, sizeof(board), boards)) {
        num_boards++;
    }

    fclose(boards);
    return num_boards;
}

char **get_boards_from_file(char *boards_file_name, int *num_boards_in_file) {
    FILE *boards;
    char buffer[64];

    boards = fopen(boards_file_name, "r");
    if (boards == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    *num_boards_in_file = count_boards_in_file(boards_file_name);
    if(*num_boards_in_file <= 1) {
        exit(EXIT_FAILURE);
    }

    char **boards_from_file = (char **)malloc((*num_boards_in_file) * sizeof(char *));

    int line_count = 0;
    while (fgets(buffer, sizeof(buffer), boards) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        boards_from_file[line_count] = malloc(17);
        boards_from_file[line_count][0] = '\0';

        strncat(boards_from_file[line_count], strtok(buffer, " "), 1);
        for(int i = 0; i < 15; i++) {
            strncat(boards_from_file[line_count], strtok(NULL, " "), 1);
        }
        line_count++;
    }

    fclose(boards);

    return boards_from_file;
}

void set_file_board(char board[4][4], char **boards_from_file, int boards_from_file_size, int current_board_from_file) {
    char *board_from_file = boards_from_file[current_board_from_file % boards_from_file_size];
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            board[i][j] = board_from_file[(i*4)+j];
        }
    }
    game.current_board_from_file++;
}

void set_random_board(char board[4][4]) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            board[i][j] = (char)(65 + rand() % 26);
        }
    }
}

void get_board_string(char *board_str, char board[4][4]) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            board_str[(i*4)+j] = board[i][j];
        }
    }
    board_str[17] = '\0';
}

void print_usage() {
    printf("paroliere_srv nome_server porta_server [--matrici data_filename] [--durata durata_in_minuti] [--seed rnd_seed] [--diz dizionario]\n");
}

void set_options(int argc, char **argv) {
    int opt;

    static struct option longopts[] = {
        {"matrici", required_argument, 0, 'm'},
        {"durata", required_argument, 0, 'd'},
        {"seed", required_argument, 0, 's'},
        {"diz", required_argument, 0, 'D'},
        {0, 0, 0, 0}
    };

    game.boards_file_name = NULL;
    game.game_length = DEFAULT_GAME_LENGTH;
    game.seed = (unsigned int)time(0);
    game.dictionary_file_name = DEFAULT_DICTIONARY_PATH;

    while ((opt = getopt_long(argc, argv, "m:d:s:D:", longopts, NULL)) != -1) {
        switch (opt) {
            case 'm': 
                game.boards_file_name = optarg;
                break;
            case 'd':
                game.game_length = atoi(optarg)*60;
                break;
            case 's':
                game.seed = (unsigned int)atoi(optarg);
                break;
            case 'D':
                game.dictionary_file_name = optarg;
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }
}

void set_board() {
    if(game.boards_file_name == NULL) {
        set_random_board(game.board);
    }
    else {
        set_file_board(game.board, game.boards_from_file, game.boards_from_file_size, game.current_board_from_file);
    }
}

//inizializza tutti i parametri di gioco che devono essere settati all'avvio del server
void init_game() {
    srand(game.seed);
    game.pause_length = PAUSE_LENGTH;

    if(game.boards_file_name != NULL) {
        game.boards_from_file = get_boards_from_file(game.boards_file_name, &game.boards_from_file_size);
        game.current_board_from_file = 0;
    }
    set_board();
    
    game.clients = create_hashmap();
    game.scores = queue_create();
    game.active_client_count = 0;
    game.completed_client_count = 0;
    game.scores_done = 0;

    game.status = 1;
    set_game_timer(game.game_length);
}

//resetta il timer e cambia lo stato del gioco
void reset_game() {
    pthread_mutex_lock(&game_status_mutex);
    game.status = !game.status;
    if(game.status) {
        set_board();
        set_game_timer(game.game_length);
        printf("Game began\n");
    }
    else {
        set_game_timer(game.pause_length);
        printf("Game over\n");
    }
    pthread_cond_broadcast(&game_status_cond);
    pthread_mutex_unlock(&game_status_mutex);
}

Client *init_client() {
    Client *client = (Client *)malloc(sizeof(Client));
    client->registered = 0;
    client->username[0] = '\0';
    client->address_len = sizeof(client->address);
    client->past_guesses = NULL;
    return client;
}

void client_cleanup(Client *client) {
    if(client->registered) {
        game.active_client_count--;
    }
    hashmap_delete(game.clients, client->fd);
}   

void to_lowercase(char *str) {
    while (*str) {
        *str = tolower(*str);
        str++;
    }
}

void to_uppercase(char *str) {
    while (*str) {
        *str = toupper(*str);
        str++;
    }
}

int search_dictionary(char *dictionary_name, char *guess) {
    FILE *dictionary;
    char dict_word[32];

    dictionary = fopen(dictionary_name, "r");
    if (dictionary == NULL) {
        perror("Error opening file");
        pthread_exit(NULL);
    }

    while (fgets(dict_word, sizeof(dict_word), dictionary) != NULL) {
        if(strcmp(strtok(dict_word, "\r\n"), guess) == 0) {
            fclose(dictionary);
            return 1;
        }
    }

    fclose(dictionary);
    return 0;
}

int imax(int n, ...) {
    va_list args;
    va_start(args, n);

    int x;
    int max = va_arg(args, int);
    for (int i = 1; i < n; i++) {
        x = va_arg(args, int);
        max = max > x ? max : x;
    }

    va_end(args);

    return max;
}

int board_letter_equals(char board_char, char *guess, int *d) {
    if(board_char == 'Q' && strncmp(&guess[*d], "QU", 2) == 0) {
        *d = *d+2;
        return 2;
    }
    else if(board_char == 'Q') {
        *d = *d+1;
        return 0;
    }
    *d = *d+1;
    return board_char == guess[*d-1];
}

int search_board_rec(char board[4][4], char *guess, int x, int y, int d, int visited[4][4]) {
    if(x < 0 || x > 3 || y < 0 || y > 3 || (int)strlen(guess) <= d || !board_letter_equals(board[x][y], guess, &d) || visited[x][y]) return 0;

    visited[x][y] = 1;
    int n = (board[x][y] == 'Q' ? 2 : 1) + imax(4,
        search_board_rec(board, guess, x + 1, y, d, visited),
        search_board_rec(board, guess, x - 1, y, d, visited),
        search_board_rec(board, guess, x, y + 1, d, visited),
        search_board_rec(board, guess, x, y - 1, d, visited)
    );
    visited[x][y] = 0;

    return n;
}

int search_board(char board[4][4], char *guess) {
    if(strlen(guess) == 0) return 0;

    int max_n = 0;
    int visited[4][4] = {0};
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if((max_n = imax(2, max_n, search_board_rec(board, guess, i, j, 0, visited))) == (int)strlen(guess)) {
                return max_n;
            }
        }
    }

    return 0;
}

int score_word(char board[4][4], char *dictionary_name, List *past_guesses, char *guess) {
    char *guess_str = (char *)malloc(strlen(guess) + 1);
    strcpy(guess_str, guess);

    to_lowercase(guess_str);
    if(list_contains(past_guesses, guess_str)) { //se l'utente ha già indovinato la parola
        free(guess_str);
        return 0;
    }

    if(!search_dictionary(dictionary_name, guess_str)) { //se la parola è nel dizionario dato
        free(guess_str);
        return -1;
    }

    to_uppercase(guess_str);
    if(!search_board(board, guess_str)) { //se la parola può essere formata usando la matrice data
        free(guess_str);
        return -1;
    }

    to_lowercase(guess_str);
    list_put(past_guesses, guess_str);

    int points = strlen(guess_str);

    free(guess_str);

    return points;
}

//setta il nome utente solo se non c'è già qualcuno con quel nome utente
int set_username(Client *client, char *username) {
    int num_entries;
    Entry** entries = hashmap_get_all_entries(game.clients, &num_entries);

    for(int i = 0; i < num_entries; i++) {
        if(strcmp(username, ((Client *) entries[i]->value)->username) == 0) {
            return -1;
        }
    }

    strcpy(client->username, username);
    
    return 1;
}

void timer_handler(int sig) {
    if (sig == SIGALRM) {
        reset_game();
    }
}

void set_game_timer(int duration) {
    signal(SIGALRM, timer_handler);
    alarm(duration);
    pthread_mutex_lock(&game_time_mutex);
    time(&game.end_time);
    game.end_time += duration;
    pthread_mutex_unlock(&game_time_mutex);
}

int get_remaining_time() {
    pthread_mutex_lock(&game_time_mutex);
    time_t current_time;
    time(&current_time);
    int remaining_time = difftime(game.end_time, current_time);
    if (remaining_time < 0) {
        remaining_time = 0;
    }
    pthread_mutex_unlock(&game_time_mutex);
    return remaining_time;
}

void send_matrix(Client *client) {
    Message *message = (Message *)malloc(sizeof(Message));

    message->type = MSG_MATRICE;
    message->length = 17;

    message->data = (char *)malloc(message->length);
    get_board_string(message->data, game.board);
    
    if(send_message(message, client->fd) < 0) {
        pthread_exit(NULL);
    }

    free(message);
}

void send_remaining_time(Client *client) {
    Message *message = (Message *)malloc(sizeof(Message));

    pthread_mutex_lock(&game_status_mutex);
    message->type = game.status ? MSG_TEMPO_PARTITA : MSG_TEMPO_ATTESA;
    pthread_mutex_unlock(&game_status_mutex);
    
    message->length = sizeof(int);

    message->data = (char*)malloc(message->length);
    int remaining_time = get_remaining_time();
    memcpy(message->data, &remaining_time, sizeof(int));
    
    send_message(message, client->fd);

    free(message);
}

void send_final_score(Client *client) {
    Message *message = (Message *)malloc(sizeof(Message));

    message->type = MSG_PUNTI_FINALI;
    message->length = strlen(game.scores_csv);
    message->data = game.scores_csv;
    
    send_message(message, client->fd);

    free(message);
}

int is_game_over() {
    pthread_mutex_lock(&game_status_mutex);
    if (game.status) {
        pthread_mutex_unlock(&game_status_mutex);
        return 0;
    }
    pthread_mutex_unlock(&game_status_mutex);
    return 1;
}

void register_user(char* buffer, Client *client) {
    int retvalue;
    Message *message = (Message *)malloc(sizeof(Message));
    do {
        retvalue = receive_message(message, buffer, BUFFER_SIZE, client->fd);
        if(retvalue < 0) { //errors
            perror("receive_message");
            client_cleanup(client);
            pthread_exit(NULL);
        }
        else if(retvalue == 0) { //client disconnected
            printf("[%i] disconnected\n", client->fd);
            client_cleanup(client);
            pthread_exit(NULL);
        }

        printf("[%i] received message: type=%c, length=%u, data=%s\n", client->fd, message->type, message->length, message->data); //log

        int retvalue = set_username(client, message->data);

        message->type = (retvalue < 0) ? MSG_ERR : MSG_OK;
        message->data = NULL;
        message->length = 0;

        send_message(message, client->fd);
    } while(message->type == MSG_ERR);

    free(message);

    client->registered = 1;
    printf("[%i] registered as %s\n", client->fd, client->username); //log

    //dopo la registrazione invia al client la matrice e il tempo rimanente se la partita è iniziata
    //e solo il tempo rimanente all'inizio della prossima se la partita è finita
    if(!is_game_over()) {
        send_matrix(client);
    }
    send_remaining_time(client);
}

void pause_loop(char * buffer, Client* client) {
    int retvalue;
    Message* message = (Message *)malloc(sizeof(Message));
    while(is_game_over()) {
        retvalue = receive_message_timeout(message, buffer, BUFFER_SIZE, client->fd, get_remaining_time());
        if(retvalue == -1) { //errors
            perror("receive_message_timeout");
            client_cleanup(client);
            pthread_exit(NULL);
        }
        else if(retvalue == -2) { //client disconnected
            printf("[%i] disconnected\n", client->fd);
            client_cleanup(client);
            pthread_exit(NULL);
        }
        else if(retvalue == 0) { //timeout
            continue;
        }

        printf("[%i] received message: type=%c, length=%u, data=%s\n", client->fd, message->type, message->length, message->data); //log

        switch(message->type) {
            case MSG_REGISTRA_UTENTE:
                break;
            case MSG_MATRICE:
                send_remaining_time(client);
                break;
            case MSG_PUNTI_FINALI:
                send_final_score(client);
                break;
            default:
                printf("request type unknown pause loop\n");
                break;
        }
    }
    free(message);
}

void game_loop(char* buffer, Client *client) {
    int retvalue;
    int points = 0;
    Message* message = (Message *)malloc(sizeof(Message));

    //resetta la lista delle parole già indovinate
    free_list(client->past_guesses);
    client->past_guesses = create_list();

    while(!is_game_over()) {
        retvalue = receive_message_timeout(message, buffer, BUFFER_SIZE, client->fd, get_remaining_time());
        if(retvalue == -1) { //errors
            perror("receive_message_timeout");
            client_cleanup(client);
            pthread_exit(NULL);
        }
        else if(retvalue == -2) { //client disconnected
            printf("[%i] disconnected\n", client->fd);
            client_cleanup(client);
            pthread_exit(NULL);
        }
        else if(retvalue == 0) { //timeout
            continue;
        }

        printf("[%i] received message: type=%c, length=%u, data=%s\n", client->fd, message->type, message->length, message->data); //log

        switch(message->type) {
            case MSG_MATRICE:
                send_matrix(client);
                send_remaining_time(client);
                break;
            case MSG_PAROLA:
                retvalue = score_word(game.board, game.dictionary_file_name, client->past_guesses, message->data);
                
                //se il punteggio è negativo significa che la parola non è presente nel dizionario o non si può 
                //creare con la matrice e manda un messaggio di errore al client
                //se invece la parola è già stata indovinata il punteggio è 0
                if(retvalue < 0) {
                    message->type = MSG_ERR;
                    message->data = NULL;
                    message->length = 0;
                }
                else {
                    points += retvalue;
                    message->type = MSG_PUNTI_PAROLA;
                    message->data = (char *)malloc(sizeof(int));
                    memcpy(message->data, &retvalue, sizeof(int));
                    message->length = sizeof(int);
                }
                
                send_message(message, client->fd);
                break;
            default:
                printf("request type unknown game loop");
                break;
        }
    }
    free(message);

    //invia il punteggio dell'utente ad una coda condivisa
    struct Score *score = (struct Score *)malloc(sizeof(struct Score));
    strcpy(score->username, client->username);
    score->points = points;
    queue_push(game.scores, (void *)score);

    //sincronizzazione con il thread scorer
    pthread_mutex_lock(&scores_mutex);
    game.completed_client_count++;
    if (game.completed_client_count == game.active_client_count) {
        pthread_cond_signal(&scorer_cond);
    }
    while (!game.scores_done) {
        pthread_cond_wait(&clients_cond, &scores_mutex);
    }
    game.completed_client_count--;
    if (game.completed_client_count == 0) {
        game.scores_done = 0;
    }
    pthread_mutex_unlock(&scores_mutex);

    //alla fine della partita invia la classifica
    send_final_score(client);
}

int compare_scores(const void *a, const void *b) {
    Score *scoreA = *(Score **)a;
    Score *scoreB = *(Score **)b;
    return scoreB->points - scoreA->points;
}

char *scores_to_csv(Score **scores, int num_scores) {
    int line_length = USERNAME_MAX_LENGTH + 8;
    int csv_string_size = (num_scores * line_length) + 1;
    char *csv_string = (char *)malloc(csv_string_size);
    if (!csv_string) {
        return NULL;
    }

    csv_string[0] = '\0';
    for (int i = 0; i < num_scores; i++) {
        char line[line_length + 1];
        snprintf(line, sizeof(line), "%s,%d\n", scores[i]->username, scores[i]->points);
        strcat(csv_string, line);
    }

    return csv_string;
}

void *scorer_thread(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&scores_mutex);

        while (game.scores_done == 1 || game.active_client_count <= 0 || game.completed_client_count < game.active_client_count) {
            pthread_cond_wait(&scorer_cond, &scores_mutex);
        }

        //prende tutti i punteggi degli utenti, li ordina e poi li converte in csv
        int num_scores = queue_size(game.scores);
        struct Score **scores = (Score **)malloc(num_scores * sizeof(Score *));
        for(int i = 0; i < num_scores; i++) {
            scores[i] = (struct Score *)queue_pop(game.scores);
        }
        qsort(scores, num_scores, sizeof(Score *), compare_scores);
        game.scores_csv = scores_to_csv(scores, num_scores);

        printf("Scores (%i players):\n%s", num_scores, game.scores_csv); //log

        for(int i = 0; i < num_scores; i++) {
            free(scores[i]);
        }
        free(scores);

        game.scores_done = 1;
        pthread_cond_broadcast(&clients_cond);
        pthread_mutex_unlock(&scores_mutex);
    }
}

void *client_handler(void *arg) {
    char buffer[BUFFER_SIZE];

    Client *client = (Client *) arg;

    printf("[%i] connected\n", client->fd); //log

    register_user(buffer, client);

    pthread_mutex_lock(&scores_mutex);
    game.active_client_count++;
    pthread_mutex_unlock(&scores_mutex);

    while(1) {
        if (!is_game_over()) {
            game_loop(buffer, client);
        }
        else {
            pause_loop(buffer, client);
        }
    }

    printf("client disconnected");

    free_list(client->past_guesses);

    close(client->fd);

    return NULL;
}

int main (int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    char *server_name = argv[1];
    int port = atoi(argv[2]);

    set_options(argc, argv);
    init_game();

    pthread_t scorer_thread_t;
    if (pthread_create(&scorer_thread_t, NULL, scorer_thread, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    int server_fd, retvalue;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_name);
    server_addr.sin_port = htons(port);

    retvalue = bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (retvalue < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    retvalue = listen(server_fd, -1);
    if (retvalue < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening...\n");

    while(1) {
        while(hashmap_size(game.clients) < MAX_NUM_CLIENTS) {
            Client *client = init_client();
            
            do {
                client->fd = accept(server_fd, (struct sockaddr *) &client->address, &client->address_len);
                if(client->fd < 0) {
                    if(errno == EINTR) {
                        continue;
                    }
                    else {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                }
            } while(client->fd < 0);

            if (pthread_create(&client->thread, NULL, client_handler, (void *) client) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            hashmap_put(game.clients, client->fd, client);
        }
    }

    free_hashmap(game.clients);
    free_queue(game.scores);

    close(server_fd);

    return EXIT_SUCCESS;
}