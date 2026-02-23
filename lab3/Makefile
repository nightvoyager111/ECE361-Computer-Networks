CC = gcc
CFLAGS = -Wall -g -lm

DELIVER_DIR = deliver_folder
SERVER_DIR = server_folder

ALL = $(DELIVER_DIR)/deliver $(SERVER_DIR)/server

all: $(ALL)

$(DELIVER_DIR)/deliver: $(DELIVER_DIR)/deliver.c
	$(CC) $(CFLAGS) $(DELIVER_DIR)/deliver.c -o $(DELIVER_DIR)/deliver

$(SERVER_DIR)/server: $(SERVER_DIR)/server.c
	$(CC) $(CFLAGS) $(SERVER_DIR)/server.c -o $(SERVER_DIR)/server

clean:
	rm -f $(DELIVER_DIR)/deliver $(SERVER_DIR)/server
	rm -f $(SERVER_DIR)/*.jpg $(SERVER_DIR)/*.png