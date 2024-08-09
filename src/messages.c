#include "messages.h"


int write_message(int sock, const char *message, int length) {
    int n_written = write(sock, &length, sizeof(length));
    if(n_written <= 0) {
        return n_written;
    }
    return write(sock, message, length);
}

int read_message(int sock, char *buffer) {
    int length;
    int n_read = read(sock, &length, sizeof(length));
    if(n_read <= 0) {
        return n_read;
    }
    return read(sock, buffer, length);
}

int read_message_timeout(int sock, char *buffer, int timeout_time, int *activity) {
    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    timeout.tv_sec = timeout_time;
    timeout.tv_usec = 0;

    *activity = select(sock + 1, &read_fds, NULL, NULL, &timeout);

    if (*activity <= 0) {
        return -1;
    }

    if (FD_ISSET(sock, &read_fds)) {
        int length;
        int n_read = read(sock, &length, sizeof(length));

        if(n_read < 0) {
            return -1;
        }
        else if(n_read == 0) {
            return -2;
        }

        n_read = read(sock, buffer, length);
        if(n_read < 0) {
            return -1;
        }
        else if(n_read == 0) {
            return -2;
        }

        return n_read;
    }

    return -1;
}

int get_input_timeout(char *buffer, int buffer_size, int timeout_time) {
    fd_set set;
    struct timeval timeout;

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    timeout.tv_sec = timeout_time;
    timeout.tv_usec = 0;

    fflush(stdout);

    int retvalue = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout); 

    if (retvalue <= 0) {
        return retvalue;
    }

    fgets(buffer, buffer_size, stdin);

    return 1;
}

int serialize_message(char **serialized, const Message *message, int *serialized_size) {
    *serialized_size = sizeof(message->type) + sizeof(message->length) + message->length;

    *serialized = malloc(*serialized_size);
    if (!*serialized) {
        return -1;
    }

    int offset = 0;

    memcpy(*serialized + offset, &message->type, sizeof(message->type));
    offset += sizeof(message->type);

    memcpy(*serialized + offset, &message->length, sizeof(message->length));
    offset += sizeof(message->length);

    if (message->length > 0 && message->data != NULL) {
        memcpy(*serialized + offset, message->data, message->length);
    }

    return 1;
}

int deserialize_message(Message *message, const char *serialized, int serialized_size) {
    if (serialized_size < (int)(sizeof(char) + sizeof(unsigned int))) {
        return -1;
    }

    if (!message) {
        return -1;
    }

    int offset = 0;
    memcpy(&message->type, serialized + offset, sizeof(message->type));
    offset += sizeof(message->type);
    memcpy(&message->length, serialized + offset, sizeof(message->length));
    offset += sizeof(message->length);

    if (message->length > 0) {
        message->data = malloc(message->length);
        if (!message->data) {
            free(message);
            return -1;
        }
        memcpy(message->data, serialized + offset, message->length);
    } else {
        message->data = NULL;
    }

    return 1;
}

int receive_message_timeout(Message *message, char *buffer, int buffer_size, int fd, int timeout_seconds) {
    int n_read, activity;

    memset(buffer, 0, buffer_size);

    n_read = read_message_timeout(fd, buffer, timeout_seconds, &activity);

    if (activity < 0) {
        return -1;
    }
    else if (activity == 0) {
        return 0;
    }
    if (n_read <= 0) {
        return n_read;
    }

    if (deserialize_message(message, buffer, n_read) < 0) {
        return -1;
    }

    return 1;
}

int receive_message(Message *message, char *buffer, int buffer_size, int fd) {
    memset(buffer, 0, buffer_size);

    int n_read = read_message(fd, buffer);

    if (n_read <= 0) {
        return n_read;
    }

    if (deserialize_message(message, buffer, n_read) < 0) {
        return -1;
    }

    return 1;
}

int send_message(Message *message, int fd) {
    int serialized_size;
    char *serialized_message = NULL;

    if (serialize_message(&serialized_message, message, &serialized_size) < 0) {
        return -1;
    }

    int n_written = write_message(fd, serialized_message, serialized_size);

    free(serialized_message);

    if (n_written < 0) {
        return -1;
    }

    return 1;
}