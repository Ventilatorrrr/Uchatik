// client.c

#include "client.h"

void *receive_handler(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    
    while ((bytes_received = recv(client->socket_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        // Обробка отриманих даних
        Message *msg = (Message *)buffer;
        
        switch (msg->type) {
            case MSG_TEXT:
                printf("[%s] %s: %s\n", msg->timestamp, msg->sender, msg->content);
                break;
            case MSG_INFO:
                printf("[Сервер]: %s\n", msg->content);
                break;
            case MSG_HISTORY:
                // Не завантажувати історію повторно при перепідключенні
                break;
            default:
                break;
        }
        prompt();
    }
    
    if (bytes_received == 0) {
        printf("З'єднання з сервером втрачено.\n");
        client->authenticated = 0;
        // Спроба повторного підключення може бути реалізована тут
    } else if (bytes_received == -1) {
        perror("Помилка отримання даних");
    }
    
    pthread_exit(NULL);
}

void prompt() {
    printf(">> ");
    fflush(stdout);
}

int connect_to_server(const char *hostname, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // Створення сокета
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Помилка створення сокета");
        return -1;
    }

    // Налаштування адреси сервера
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, hostname, &server_addr.sin_addr) <= 0) {
        perror("Невірна адреса сервера");
        close(sockfd);
        return -1;
    }

    // Підключення до сервера
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Помилка підключення до сервера");
        close(sockfd);
        return -1;
    }

    return sockfd;
}
