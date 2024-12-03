CC = gcc

# Путь до папки libraries
LIB_DIR = ./libraries

# Прапорці компіляції для сервера
CFLAGS_SERVER = -Wall -pthread \
    -I$(LIB_DIR)/sqlite3/include \
    -I$(LIB_DIR)/openssl/include \
    -I$(LIB_DIR)/cJSON_install/include

# Прапорці компіляції для клієнта
CFLAGS_CLIENT = -Wall -pthread \
    `pkg-config --cflags gtk+-3.0` \
    -I$(LIB_DIR)/openssl/include \
    -I$(LIB_DIR)/cJSON_install/include

# Бібліотеки для сервера
LIBS_SERVER = \
    $(LIB_DIR)/sqlite3/lib/libsqlite3.a \
    $(LIB_DIR)/openssl/lib/libssl.a \
    $(LIB_DIR)/openssl/lib/libcrypto.a \
    $(LIB_DIR)/cJSON_install/lib/libcjson.a \
    -ldl -lpthread

# Бібліотеки для клієнта
LIBS_CLIENT = \
    $(LIB_DIR)/openssl/lib/libssl.a \
    $(LIB_DIR)/openssl/lib/libcrypto.a \
    $(LIB_DIR)/cJSON_install/lib/libcjson.a \
    `pkg-config --libs gtk+-3.0` \
    -ldl -lpthread

all: uchat_server uchat

# Сервер

uchat_server: uchat_server.o database.o
	$(CC) $(CFLAGS_SERVER) -o uchat_server uchat_server.o database.o $(LIBS_SERVER)

uchat_server.o: uchat_server.c database.h protocol.h
	$(CC) $(CFLAGS_SERVER) -c uchat_server.c

database.o: database.c database.h
	$(CC) $(CFLAGS_SERVER) -c database.c

# Клієнт

uchat: uchat_gui.o client.o
	$(CC) $(CFLAGS_CLIENT) -o uchat uchat_gui.o client.o $(LIBS_CLIENT)

uchat_gui.o: uchat_gui.c client.h protocol.h
	$(CC) $(CFLAGS_CLIENT) -c uchat_gui.c

client.o: client.c client.h protocol.h
	$(CC) $(CFLAGS_CLIENT) -c client.c

# Команда очищення

clean:
	rm -f *.o uchat uchat_server

