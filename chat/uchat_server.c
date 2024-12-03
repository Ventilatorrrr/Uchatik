// uchat_server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sqlite3.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "protocol.h"
#include "database.h"

#define BUFFER_SIZE sizeof(Message)
#define MAX_CLIENTS 100

volatile sig_atomic_t keep_running = 1;

void int_handler(int dummy) {
    keep_running = 0;
}

typedef struct {
    int socket;
    sqlite3 *db;
    char username[MAX_USERNAME_LENGTH];
} client_info_t;

client_info_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Прототипи функцій
void *handle_client(void *arg);
void broadcast_message(Message *msg, client_info_t *sender);
void add_client(client_info_t *cl);
void remove_client(client_info_t *cl);

void *handle_client(void *arg) {
    client_info_t *client = (client_info_t *)arg;
    int client_socket = client->socket;
    sqlite3 *db = client->db;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int authenticated = 0;

    // Отримувати повідомлення від клієнта
    while ((bytes_read = recv(client_socket, buffer, sizeof(Message), 0)) > 0) {
        Message *msg = (Message *)buffer;

        if (!authenticated) {
            if (msg->type == MSG_AUTH) {
                if (check_user_credentials(db, msg->sender, msg->content)) {
                    authenticated = 1;
                    strncpy(client->username, msg->sender, MAX_USERNAME_LENGTH);

                    // Відправити підтвердження клієнту
                    Message response;
                    memset(&response, 0, sizeof(response));
                    response.type = MSG_INFO;
                    strcpy(response.content, "AUTH_SUCCESS");
                    send(client_socket, &response, sizeof(response), 0);

                    // Додати клієнта до списку
                    add_client(client);

                    // Відправити історію чату
                    Message *history;
                    int message_count;
                    if (get_chat_history(db, &history, &message_count)) {
                        for (int i = 0; i < message_count; ++i) {
                            send(client_socket, &history[i], sizeof(Message), 0);
                        }
                        free(history);
                    }
                } else {
                    // Відправити повідомлення про невдалу аутентифікацію
                    Message response;
                    memset(&response, 0, sizeof(response));
                    response.type = MSG_INFO;
                    strcpy(response.content, "AUTH_FAILED");
                    send(client_socket, &response, sizeof(response), 0);
                    break;
                }
            } else if (msg->type == MSG_REGISTER) {
                int reg_result = register_user(db, msg->sender, msg->content);
                Message response;
                memset(&response, 0, sizeof(response));
                response.type = MSG_INFO;
                if (reg_result == 1) {
                    // Відправити підтвердження клієнту
                    strcpy(response.content, "REG_SUCCESS");
                    send(client_socket, &response, sizeof(response), 0);
                } else if (reg_result == -1) {
                    // Ім'я користувача вже існує
                    strcpy(response.content, "USERNAME_EXISTS");
                    send(client_socket, &response, sizeof(response), 0);
                } else {
                    // Реєстрація не вдалася
                    strcpy(response.content, "REG_FAILED");
                    send(client_socket, &response, sizeof(response), 0);
                }
            } else {
                continue;
            }
        } else {
            if (msg->type == MSG_TEXT) {
                // Зберегти повідомлення в базу даних
                save_message(db, client->username, msg->content);

                // Розіслати повідомлення іншим клієнтам
                broadcast_message(msg, client);
            }
            // Обробка інших типів повідомлень за потреби
        }
    }

    // Завершення роботи з клієнтом
    close(client_socket);
    remove_client(client);
    free(client);
    pthread_exit(NULL);
}

void broadcast_message(Message *msg, client_info_t *sender) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->socket != sender->socket) {
                send(clients[i]->socket, msg, sizeof(Message), 0);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void add_client(client_info_t *cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(client_info_t *cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == cl) {
            clients[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void daemonize() {
    pid_t pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS); // Батьківський процес завершується

    // Створити нову сесію
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    // Ігнорувати сигнали
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    // Створити новий процес
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS); // Батьківський процес завершується

    // Змінити права доступу до файлів
    umask(0);

    // Змінити робочий каталог (можна закоментувати для тестування)
    // chdir("/");

    // Закрити файлові дескриптори
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close(x);
    }

    // Перенаправити stdin, stdout, stderr у лог-файл
    int fd = open("/tmp/uchat_server.log", O_RDWR | O_CREAT | O_APPEND, 0600);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);  // fd 0
        dup2(fd, STDOUT_FILENO); // fd 1
        dup2(fd, STDERR_FILENO); // fd 2
        if (fd > 2)
            close(fd);
    } else {
        // Якщо не вдалося відкрити лог-файл, перенаправляємо у /dev/null
        fd = open("/dev/null", O_RDWR);
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2)
            close(fd);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Використання: %s [порт]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    // Вивести PID процесу у термінал перед демонізацією
    printf("PID сервера: %d\n", getpid());

    // Демонізувати процес
    daemonize();

    // Ініціалізація бази даних
    sqlite3 *db;
    if (initialize_database(&db) != SQLITE_OK) {
        exit(EXIT_FAILURE);
    }

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    pthread_t tid;

    // Створити сокет
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Помилка створення сокета");
        exit(EXIT_FAILURE);
    }

    // Налаштувати адресу сервера
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Прив'язати сокет
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Помилка прив'язки");
        exit(EXIT_FAILURE);
    }

    // Слухати з'єднання
    if (listen(server_socket, 10) < 0) {
        perror("Помилка прослуховування");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, int_handler);

    printf("Сервер запущено на порту %d\n", port);

    // Приймати з'єднання
    while (keep_running) {
        socklen_t client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

        if (client_socket < 0) {
            perror("Помилка прийому з'єднання");
            continue;
        }

        client_info_t *client = malloc(sizeof(client_info_t));
        client->socket = client_socket;
        client->db = db;
        memset(client->username, 0, MAX_USERNAME_LENGTH);

        if (pthread_create(&tid, NULL, handle_client, (void *)client) != 0) {
            perror("Помилка створення потоку");
            free(client);
        }

        pthread_detach(tid);
    }

    close(server_socket);
    sqlite3_close(db);
    unlink("/var/run/uchat_server.pid");

    return 0;
}

