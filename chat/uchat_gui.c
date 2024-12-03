// uchat_gui.c

#include <gtk/gtk.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "protocol.h"
#include "client.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *chat_view;
    GtkWidget *entry;
    int socket_fd;
    char username[MAX_USERNAME_LENGTH];
    int authenticated;
    GtkTextBuffer *buffer;
} AppData;

// Структура для передачі даних в append_text_to_chat
typedef struct {
    AppData *app_data;
    char *message;
} AppendData;

// Прототипи функцій
int show_login_dialog(AppData *app_data);
void *receive_messages(void *arg);
void send_message(GtkWidget *widget, AppData *app_data);
gboolean append_text_to_chat(gpointer data);

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    AppData app_data;
    memset(&app_data, 0, sizeof(AppData));

    // Підключення до сервера
    app_data.socket_fd = connect_to_server("127.0.0.1", 5555);
    if (app_data.socket_fd == -1) {
        fprintf(stderr, "Не вдалося підключитися до сервера\n");
        return 1;
    }

    // Створення головного вікна
    app_data.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app_data.window), "UChat");
    gtk_window_set_default_size(GTK_WINDOW(app_data.window), 400, 600);
    g_signal_connect(app_data.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Створення інтерфейсу
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    // Поле відображення чату
    app_data.chat_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app_data.chat_view), FALSE);
    app_data.buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app_data.chat_view));

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), app_data.chat_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    // Поле введення повідомлення
    app_data.entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), app_data.entry, FALSE, FALSE, 0);

    // Обробник події натискання Enter
    g_signal_connect(app_data.entry, "activate", G_CALLBACK(send_message), &app_data);

    gtk_container_add(GTK_CONTAINER(app_data.window), vbox);

    // Аутентифікація користувача
    if (!show_login_dialog(&app_data)) {
        close(app_data.socket_fd);
        return 1;
    }

    // Запуск потоку для отримання повідомлень
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &app_data);

    gtk_widget_show_all(app_data.window);
    gtk_main();

    // Завершення роботи
    close(app_data.socket_fd);
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);

    return 0;
}

int register_user(int socket_fd, const char *username, const char *password) {
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_REGISTER;
    strncpy(msg.sender, username, MAX_USERNAME_LENGTH);
    strncpy(msg.content, password, MAX_PASSWORD_LENGTH);

    if (send(socket_fd, &msg, sizeof(Message), 0) == -1) {
        perror("Помилка відправки повідомлення реєстрації");
        return 0;
    }

    // Отримати відповідь від сервера
    if (recv(socket_fd, &msg, sizeof(Message), 0) > 0) {
        if (msg.type == MSG_INFO && strcmp(msg.content, "REGISTER_SUCCESS") == 0) {
            return 1;
        }
    }

    return 0;
}


// Функція для відображення діалогу входу
int show_login_dialog(AppData *app_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Вхід або Реєстрація", GTK_WINDOW(app_data->window),
                                                    GTK_DIALOG_MODAL, "_Вхід", GTK_RESPONSE_ACCEPT,
                                                    "_Реєстрація", GTK_RESPONSE_APPLY,
                                                    "_Скасувати", GTK_RESPONSE_REJECT, NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *username_label = gtk_label_new("Ім'я користувача:");
    GtkWidget *username_entry = gtk_entry_new();

    GtkWidget *password_label = gtk_label_new("Пароль:");
    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(grid), username_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), username_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 1, 1, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    int result = 0;
    int response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_APPLY) {
        const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
        const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));

        strncpy(app_data->username, username, MAX_USERNAME_LENGTH);

        if (response == GTK_RESPONSE_ACCEPT) {
            // Вхід
            if (authenticate(app_data->socket_fd, username, password)) {
                result = 1;
            } else {
                GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(app_data->window),
                                                                 GTK_DIALOG_MODAL,
                                                                 GTK_MESSAGE_ERROR,
                                                                 GTK_BUTTONS_CLOSE,
                                                                 "Невдалий вхід. Перевірте ім'я користувача та пароль.");
                gtk_dialog_run(GTK_DIALOG(error_dialog));
                gtk_widget_destroy(error_dialog);
                result = 0;
            }
        } else if (response == GTK_RESPONSE_APPLY) {
            // Реєстрація
            if (register_user(app_data->socket_fd, username, password)) {
                GtkWidget *info_dialog = gtk_message_dialog_new(GTK_WINDOW(app_data->window),
                                                                GTK_DIALOG_MODAL,
                                                                GTK_MESSAGE_INFO,
                                                                GTK_BUTTONS_CLOSE,
                                                                "Реєстрація успішна! Тепер ви можете увійти.");
                gtk_dialog_run(GTK_DIALOG(info_dialog));
                gtk_widget_destroy(info_dialog);
                result = 0; // Повертаємося до діалогу входу
            } else {
                GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(app_data->window),
                                                                 GTK_DIALOG_MODAL,
                                                                 GTK_MESSAGE_ERROR,
                                                                 GTK_BUTTONS_CLOSE,
                                                                 "Невдала реєстрація. Ім'я користувача може вже використовуватися.");
                gtk_dialog_run(GTK_DIALOG(error_dialog));
                gtk_widget_destroy(error_dialog);
                result = 0;
            }
        }
    }

    gtk_widget_destroy(dialog);
    return result;
}


// Функція для відправки повідомлення на сервер
void send_message(GtkWidget *widget, AppData *app_data) {
    const char *message_text = gtk_entry_get_text(GTK_ENTRY(app_data->entry));

    if (strlen(message_text) == 0) {
        return;
    }

    // Відправити повідомлення на сервер
    send_text_message(app_data->socket_fd, app_data->username, message_text);

    // Очистити поле введення
    gtk_entry_set_text(GTK_ENTRY(app_data->entry), "");
}

// Потік для отримання повідомлень від сервера
void *receive_messages(void *arg) {
    AppData *app_data = (AppData *)arg;
    Message msg;
    ssize_t bytes_read;

    while ((bytes_read = recv(app_data->socket_fd, &msg, sizeof(Message), 0)) > 0) {
        if (msg.type == MSG_TEXT) {
            // Додати повідомлення в текстовий буфер GTK
            char display_message[MAX_MESSAGE_LENGTH + MAX_USERNAME_LENGTH + 5];
            snprintf(display_message, sizeof(display_message), "%s: %s\n", msg.sender, msg.content);

            // Оновлення GUI повинно виконуватися в головному потоці
            AppendData *append_data = g_malloc(sizeof(AppendData));
            append_data->app_data = app_data;
            append_data->message = g_strdup(display_message);

            g_idle_add((GSourceFunc)append_text_to_chat, append_data);
        }
    }

    return NULL;
}

// Допоміжна функція для додавання тексту в чат (виконується в головному потоці)
gboolean append_text_to_chat(gpointer data) {
    AppendData *append_data = (AppendData *)data;
    AppData *app_data = append_data->app_data;
    char *message = append_data->message;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app_data->chat_view));
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(buffer, &end_iter);
    gtk_text_buffer_insert(buffer, &end_iter, message, -1);

    g_free(message);
    g_free(append_data);
    return FALSE;
}

// Функції з client.c
int authenticate(int socket_fd, const char *username, const char *password) {
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_AUTH;
    strncpy(msg.sender, username, MAX_USERNAME_LENGTH);
    strncpy(msg.content, password, MAX_PASSWORD_LENGTH);

    if (send(socket_fd, &msg, sizeof(Message), 0) == -1) {
        perror("Помилка відправки повідомлення аутентифікації");
        return 0;
    }

    // Отримати відповідь від сервера
    if (recv(socket_fd, &msg, sizeof(Message), 0) > 0) {
        if (msg.type == MSG_INFO && strcmp(msg.content, "AUTH_SUCCESS") == 0) {
            return 1;
        }
    }

    return 0;
}

void send_text_message(int socket_fd, const char *username, const char *message_text) {
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_TEXT;
    strncpy(msg.sender, username, MAX_USERNAME_LENGTH);
    strncpy(msg.content, message_text, MAX_MESSAGE_LENGTH);

    if (send(socket_fd, &msg, sizeof(Message), 0) == -1) {
        perror("Помилка відправки текстового повідомлення");
    }
}

