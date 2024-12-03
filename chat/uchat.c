// uchat.c

#include "client.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Використання: %s [IP-адреса сервера] [порт]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    Client client;
    client.authenticated = 0;

    // Створення сокета
    client.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client.socket_fd == -1) {
        perror("Помилка створення сокета");
        exit(EXIT_FAILURE);
    }

    // Налаштування адреси сервера
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Невірна IP-адреса");
        exit(EXIT_FAILURE);
    }

    // Підключення до сервера
    if (connect(client.socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Не вдалося підключитися до сервера");
        exit(EXIT_FAILURE);
    }

    printf("Підключено до сервера %s:%d\n", server_ip, port);

    // Аутентифікація
    printf("Введіть ім'я користувача: ");
    fgets(client.username, MAX_USERNAME_LENGTH, stdin);
    strtok(client.username, "\n"); // Видалити символ нового рядка

    char password[MAX_PASSWORD_LENGTH];
    printf("Введіть пароль: ");
    fgets(password, MAX_PASSWORD_LENGTH, stdin);
    strtok(password, "\n");

    // Відправити дані аутентифікації на сервер
    Message auth_msg;
    memset(&auth_msg, 0, sizeof(auth_msg));
    auth_msg.type = MSG_AUTH;
    strncpy(auth_msg.sender, client.username, MAX_USERNAME_LENGTH);
    strncpy(auth_msg.content, password, MAX_PASSWORD_LENGTH);

    if (send(client.socket_fd, &auth_msg, sizeof(auth_msg), 0) < 0) {
        perror("Помилка відправки даних аутентифікації");
        close(client.socket_fd);
        exit(EXIT_FAILURE);
    }

    // Очікувати підтвердження аутентифікації
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client.socket_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        printf("Не вдалося отримати відповідь від сервера\n");
        close(client.socket_fd);
        exit(EXIT_FAILURE);
    }

    buffer[bytes_received] = '\0';
    Message *response = (Message *)buffer;
    if (response->type == MSG_INFO && strcmp(response->content, "AUTH_SUCCESS") == 0) {
        printf("Аутентифікація успішна.\n");
        client.authenticated = 1;
    } else {
        printf("Аутентифікація не вдалася: %s\n", response->content);
        close(client.socket_fd);
        exit(EXIT_FAILURE);
    }

    // Запустити потік для отримання повідомлень
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_handler, (void *)&client) != 0) {
        perror("Помилка створення потоку отримання");
        close(client.socket_fd);
        exit(EXIT_FAILURE);
    }

    // Основний цикл відправки повідомлень
    prompt();
    char input[MAX_MESSAGE_LENGTH];
    while (fgets(input, MAX_MESSAGE_LENGTH, stdin) != NULL) {
        strtok(input, "\n"); // Видалити символ нового рядка

        if (strcmp(input, "/exit") == 0) {
            break;
        } else if (strncmp(input, "/edit ", 6) == 0) {
            // Команда редагування повідомлення
            // Формат: /edit [message_id] [new message]
            // Реалізація за бажанням
        } else if (strncmp(input, "/delete ", 8) == 0) {
            // Команда видалення повідомлення
            // Формат: /delete [message_id]
            // Реалізація за бажанням
        } else {
            // Відправити текстове повідомлення
            Message msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = MSG_TEXT;
            strncpy(msg.sender, client.username, MAX_USERNAME_LENGTH);
            strncpy(msg.content, input, MAX_MESSAGE_LENGTH);

            // Додати часову мітку
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            strftime(msg.timestamp, 20, "%Y-%m-%d %H:%M:%S", tm_info);

            if (send(client.socket_fd, &msg, sizeof(msg), 0) < 0) {
                perror("Помилка відправки повідомлення");
                break;
            }
        }
        prompt();
    }

    // Відключитися від сервера
    close(client.socket_fd);
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);

    printf("Відключено від сервера.\n");
    return 0;
}

