// client.h

#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>

#include "protocol.h"

#define BUFFER_SIZE 2048

typedef struct {
    int socket_fd;
    char username[MAX_USERNAME_LENGTH];
    int authenticated;
} Client;

void *receive_handler(void *arg);
void prompt();
int connect_to_server(const char *hostname, int port);
int authenticate(int socket_fd, const char *username, const char *password);
void send_text_message(int socket_fd, const char *username, const char *message_text);
int register_user(int socket_fd, const char *username, const char *password);


#endif // CLIENT_H

