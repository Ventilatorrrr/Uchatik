#include "database.h"
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <stdlib.h>

int initialize_database(sqlite3 **db) {
    int rc = sqlite3_open("chat.db", db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Не вдалося відкрити базу даних: %s\n", sqlite3_errmsg(*db));
        return rc;
    }

    // Створення таблиці users
    const char *sql_create_users_table =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "password TEXT NOT NULL);";

    char *errmsg = NULL;
    rc = sqlite3_exec(*db, sql_create_users_table, 0, 0, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Помилка створення таблиці users: %s\n", errmsg);
        sqlite3_free(errmsg);
        return rc;
    }

    // Створення таблиці messages
    const char *sql_create_messages_table =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sender TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";

    rc = sqlite3_exec(*db, sql_create_messages_table, 0, 0, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Помилка створення таблиці messages: %s\n", errmsg);
        sqlite3_free(errmsg);
        return rc;
    }

    return SQLITE_OK;
}

int register_user(sqlite3 *db, const char *username, const char *password) {
    // Перевірка, чи користувач вже існує
    const char *sql_select = "SELECT id FROM users WHERE username = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Помилка підготовки запиту (перевірка існування користувача): %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    int user_exists = (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    if (user_exists) {
        // Користувач вже існує
        return -1;
    }

    // Хешування пароля
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)password, strlen(password), hash);

    // Перетворення хешу в рядок
    char hash_string[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(&hash_string[i * 2], "%02x", hash[i]);
    }

    // Додавання нового користувача
    const char *sql_insert = "INSERT INTO users (username, password) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Помилка підготовки запиту (додавання користувача): %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash_string, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Помилка виконання запиту (додавання користувача): %s\n", sqlite3_errmsg(db));
        return 0;
    }

    return 1; // Реєстрація успішна
}

int check_user_credentials(sqlite3 *db, const char *username, const char *password) {
    // Хешування введеного пароля
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)password, strlen(password), hash);

    // Перетворення хешу в рядок
    char hash_string[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(&hash_string[i * 2], "%02x", hash[i]);
    }

    const char *sql_select = "SELECT id FROM users WHERE username = ? AND password = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Помилка підготовки запиту: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash_string, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    int user_exists = (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return user_exists;
}

int save_message(sqlite3 *db, const char *sender, const char *content) {
    const char *sql_insert = "INSERT INTO messages (sender, content) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Помилка підготовки запиту (збереження повідомлення): %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, sender, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, content, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Помилка виконання запиту (збереження повідомлення): %s\n", sqlite3_errmsg(db));
        return 0;
    }

    return 1; // Повідомлення збережено успішно
}

int get_chat_history(sqlite3 *db, Message **messages, int *count) {
    const char *sql_select = "SELECT sender, content, timestamp FROM messages ORDER BY id ASC;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Помилка підготовки запиту: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    int capacity = 100;
    *messages = malloc(sizeof(Message) * capacity);
    *count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            *messages = realloc(*messages, sizeof(Message) * capacity);
        }

        Message *msg = &(*messages)[*count];
        msg->type = MSG_TEXT;
        strncpy(msg->sender, (const char *)sqlite3_column_text(stmt, 0), MAX_USERNAME_LENGTH);
        strncpy(msg->content, (const char *)sqlite3_column_text(stmt, 1), MAX_MESSAGE_LENGTH);
        strncpy(msg->timestamp, (const char *)sqlite3_column_text(stmt, 2), sizeof(msg->timestamp));

        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 1;
}

