#ifndef MESSAGES_H
#define MESSAGES_H

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
#include <sys/select.h>
#include <termios.h>

typedef struct Message{
    unsigned int length;
    char type;
    char *data;
} Message;

int write_message(int sock, const char *message, int length);

int read_message(int sock, char *buffer);

int read_message_timeout(int sock, char *buffer, int timeout_time, int *activity);

int get_input_timeout(char *buffer, int buffer_size, int timeout_time);

int serialize_message(char **serialized, const Message *message, int *serialized_size);

int deserialize_message(Message *message, const char *serialized, int serialized_size);

int receive_message_timeout(Message *message, char *buffer, int buffer_size, int fd, int timeout_seconds);

int receive_message(Message *message, char *buffer, int buffer_size, int fd);

int send_message(Message *message, int fd);

#endif