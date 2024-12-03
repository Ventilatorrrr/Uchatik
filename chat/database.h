#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include "protocol.h"


int initialize_database(sqlite3 **db);
int register_user(sqlite3 *db, const char *username, const char *password);
int check_user_credentials(sqlite3 *db, const char *username, const char *password);
int save_message(sqlite3 *db, const char *sender, const char *content);
int get_chat_history(sqlite3 *db, Message **messages, int *count);

#endif // DATABASE_H

