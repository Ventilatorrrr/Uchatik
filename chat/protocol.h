// protocol.h

#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_USERNAME_LENGTH 32
#define MAX_PASSWORD_LENGTH 32
#define MAX_MESSAGE_LENGTH 1024

// Типи повідомлень
typedef enum {
    MSG_AUTH = 0,       // Аутентифікація
    MSG_REGISTER = 1,
    MSG_TEXT = 2,       // Текстове повідомлення
    MSG_INFO = 3,       // Інформаційне повідомлення від сервера
    MSG_EDIT = 4,       // Редагування повідомлення
    MSG_DELETE = 5,     // Видалення повідомлення
    MSG_HISTORY = 6,    // Історія повідомлень
    MSG_DISCONNECT = 7  // Відключення
} MessageType;

// Структура повідомлення
typedef struct {
    MessageType type;
    char sender[MAX_USERNAME_LENGTH];
    char content[MAX_MESSAGE_LENGTH];
    char timestamp[20]; // Формат часу "YYYY-MM-DD HH:MM:SS"
    int message_id;     // Ідентифікатор повідомлення для редагування/видалення
} Message;

#endif // PROTOCOL_H

