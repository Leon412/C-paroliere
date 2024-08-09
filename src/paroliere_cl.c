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
#include <ctype.h>
#include "messages.h"

#define BUFFER_SIZE 1024

#define USERNAME_MAX_LENGTH 10

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
    char board[4][4];
    int remaining_time;
    int status;
    time_t end_time;
} Game;

Game game;
int client_fd;
int message_pipe_fds[2];

void set_board(char board[4][4], char *board_str) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            board[i][j] = board_str[(i*4)+j];
        }
    }
}

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

int check_username(char *username) {
    int username_length = strlen(username);
    if (username_length >= USERNAME_MAX_LENGTH) {
        printf("Il nome utente è troppo lungo, usa un nome con %i caratteri o meno.\n", USERNAME_MAX_LENGTH);
        return 0;
    }

    for (int i = 0; i < username_length; i++) {
        if (!isalnum(username[i])) {
            printf("Il nome non può contenere caratteri speciali.\n");
            return 0;
        }
    }

    return 1;
}

void request_board_and_time() {
    Message *message = (Message *)malloc(sizeof(Message));
    message->type = MSG_MATRICE;
    message->length = 0;
    message->data = NULL;

    send_message(message, client_fd);

    free(message);
}

void receive_board_and_time(char *buffer, int buffer_size) {
    Message *message = (Message *)malloc(sizeof(Message));

    int n_read = receive_message(message, buffer, buffer_size, message_pipe_fds[0]);
    if (n_read < 0) {
        perror("receive_board_and_time Read failed");
        exit(EXIT_FAILURE);
    }
    else if (n_read == 0) {
        printf("Server disconnected\n");
        exit(EXIT_FAILURE);
    }

    switch(message->type) {
        case MSG_MATRICE:
            set_board(game.board, message->data);
            break;

        case MSG_TEMPO_ATTESA:
            memcpy(&game.remaining_time, message->data, sizeof(int));
            game.status = 0;
            free(message);
            return;

        case MSG_PUNTI_FINALI:
            printf("classifica:\n%s", message->data);
            break;

        default:
            printf("request type unknown\n");
            free(message);
            return;
    }

    n_read = receive_message(message, buffer, buffer_size, message_pipe_fds[0]);
    if (n_read < 0) {
        perror("receive_board_and_time 2 Read failed");
        exit(EXIT_FAILURE);
    }
    else if (n_read == 0) {
        printf("Server disconnected\n");
        exit(EXIT_FAILURE);
    }

    switch(message->type) {
        case MSG_TEMPO_PARTITA:
            memcpy(&game.remaining_time, message->data, sizeof(int));
            game.status = 1;
            break;
        case MSG_PUNTI_FINALI:
            printf("classifica:\n%s", message->data);
            break;
        default:
            printf("request type unknown\n");
            break;
    }
    free(message);

    return;
}

//questo thread legge tutti i messaggi che arrivano dal server e se è un messaggio dei punti finali allora lo stampa
//se invece non lo è lo manda al thread principale tramite pipe che lo leggerà come se venisse direttamente dal server
void *message_listener(void *arg) {
    (void)arg;
    
    char buffer[BUFFER_SIZE];
    
    while (1) {
        Message *message = (Message *)malloc(sizeof(Message));
        int n_read = receive_message(message, buffer, BUFFER_SIZE, client_fd);
        if (n_read < 0) {
            perror("listener read failed");
            exit(EXIT_FAILURE);
        } else if (n_read == 0) {
            printf("Server disconnected\n");
            exit(EXIT_FAILURE);
        }

        switch (message->type) {
            case MSG_PUNTI_FINALI:
                printf("\nclassifica:\n%s\n[PROMPT PAROLIERE]-->", message->data);
                fflush(stdout);
                break;
            default:
                send_message(message, message_pipe_fds[1]);
                break;
        }
        free(message);
    }
    return NULL;
}

void print_board_and_time() {
    if(game.status) {
        printf("Matrice:\n");
        print_board(game.board);
        printf("Tempo rimanente: %i\n", game.remaining_time);
    }
    else {
        printf("Tempo rimanente all'inizio della partita: %i\n", game.remaining_time);
    }
}

void request_final_score() {
    Message *message = (Message *)malloc(sizeof(Message));
    message->type = MSG_PUNTI_FINALI;
    message->length = 0;
    message->data = NULL;

    send_message(message, client_fd);
    free(message);
}

int register_user(char *buffer, int buffer_size, char *username) {
    if(!check_username(username)) {
        return 0;
    }
    
    Message *message = (Message *)malloc(sizeof(Message));
    message->type = MSG_REGISTRA_UTENTE;

    size_t str_len = strlen(username)+1;

    message->data = (char *)malloc(str_len);

    strcpy(message->data, username);

    message->length = str_len;
    
    send_message(message, client_fd);

    int n_read = receive_message(message, buffer, buffer_size, message_pipe_fds[0]);
    if (n_read < 0) {
        perror("register_user Read failed");
        exit(EXIT_FAILURE);
    }
    else if (n_read == 0) {
        printf("Server disconnected\n");
        exit(EXIT_FAILURE);
    }

    int registration_status = message->type == MSG_OK ? 1 : 0;

    free(message);

    if(registration_status) {
        printf("Registrazione completata.\n");
        receive_board_and_time(buffer, BUFFER_SIZE);
        print_board_and_time();
    }
    else {
        printf("Nome utente già in uso, scegline un altro.\n");
    }

    return registration_status;
}


//per vedere se la partita è finita manda una richiesta di matrice e tempo rimanente perchè se arriva la matrice allora la partita è in corso, sennò no
int is_game_over(char *buffer, int buffer_size) {
    request_board_and_time();
    receive_board_and_time(buffer, buffer_size);
    return !game.status;
}

void send_word(char *buffer, int buffer_size, char *word) {
    Message *message = (Message *)malloc(sizeof(Message));
    message->type = MSG_PAROLA;

    message->length = strlen(word) + 1;

    message->data = (char *)malloc(message->length);
    if (message->data == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(message->data, word);

    send_message(message, client_fd);

    int n_read = receive_message(message, buffer, buffer_size, message_pipe_fds[0]);
    if (n_read < 0) { //errori
        perror("send_word Read failed");
        exit(EXIT_FAILURE);
    }
    else if (n_read == 0) { //il server si è disconnesso
        printf("Server disconnected\n");
        exit(EXIT_FAILURE);
    }

    int points;
    if(message->type == MSG_ERR) {
        printf("Parola incorretta (non creabile dalla matrice o non presente nel dizionario).\n");
        points = 0;
    }
    else if(message->type == MSG_PUNTI_PAROLA) {
        memcpy(&points, message->data, sizeof(int));
        if(points == 0) {
            printf("Parola già indovinata.\n");
        }
        else {
            printf("Parola corretta. Hai preso %i punti\n", points);
        }
    }
    free(message);
}

void print_usage() {
    printf("paroliere_cl nome_server porta_server\n");
}

void print_help() {
    printf("Comandi:\n");
    printf("aiuto - Stampa questo messaggio\n");
    printf("registra_utente <nome_utente> - Registra l'utente\n");
    printf("matrice - Riceve e stampa la matrice di gioco\n");
    printf("p <parola> - Prova ad indovinare una parola\n");
    printf("classifica - Riceve e stampa la classifica dei giocatori\n");
    printf("fine - Esce dal programma\n");
}

int main (int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    char *server_name = argv[1];
    int port = atoi(argv[2]);

    int retvalue;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char console_input_buffer[BUFFER_SIZE];
    char *command;
    char *parameter;
    int registered = 0;

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_name);

    retvalue = connect(client_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (retvalue < 0) {
        perror("connect");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    if (pipe(message_pipe_fds) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pthread_t listener_thread;
    if (pthread_create(&listener_thread, NULL, message_listener, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    while(1) {
        memset(console_input_buffer, 0, BUFFER_SIZE);

        printf("[PROMPT PAROLIERE]-->");
        fgets(console_input_buffer, BUFFER_SIZE, stdin);

        command = strtok(console_input_buffer, " \n");
        parameter = strtok(NULL, " \n");

        if(command == NULL) {
            continue;
        }
        else if (strcmp(command, "aiuto") == 0) {
            print_help();
        } 
        else if ((strcmp(command, "registra_utente") == 0)) {
            if(registered) {
                printf("Sei già registrato.\n");
            }
            else if (parameter == NULL) {
                printf("Nessun nome utente inserito.\n");
            }
            else {
                registered = register_user(buffer, BUFFER_SIZE, parameter);
            }
        } 
        else if ((strcmp(command, "matrice") == 0)) {
            if(!registered) {
                printf("Devi essere registrato per vedere la matrice (usa registra_utente <nome_utente>).\n");
            }
            else {
                request_board_and_time(message_pipe_fds[0]);
                receive_board_and_time(buffer, BUFFER_SIZE);
                print_board_and_time();
            }
        } 
        else if ((strcmp(command, "p") == 0)) {
            if(!registered) {
                printf("Devi essere registrato per inviare una parola (usa registra_utente <nome_utente>).\n");
            }
            else if (parameter == NULL) {
                printf("Nessuna parola inserita.\n");
            }
            else if (is_game_over(buffer, BUFFER_SIZE)) {
                printf("Nessun gioco in corso, aspetta l'inizio della prossima partita.\n");
            }
            else {
                send_word(buffer, BUFFER_SIZE, parameter);
            }
        }
        else if ((strcmp(command, "classifica") == 0)) {
            if(!registered) {
                printf("Devi essere registrato per vedere la classifica (usa registra_utente <nome_utente>).\n");
            }
            else if(!is_game_over(buffer, BUFFER_SIZE)) {
                printf("Il gioco è ancora in corso, aspetta la fine del gioco per richiedere la classifica.\n");
            }
            else {
                request_final_score();
            }
        }
        else if (strcmp(command, "fine") == 0) {
            break;
        } 
        else {
            printf("Comando non esiste (usa il comando aiuto per più informazioni).\n");
        }
    }

    close(client_fd);
    close(message_pipe_fds[0]);
    close(message_pipe_fds[1]);

    return EXIT_SUCCESS;
}