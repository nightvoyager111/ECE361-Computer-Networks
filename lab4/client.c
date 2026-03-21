#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_NAME 50
#define MAX_DATA 1024

typedef struct {
    unsigned int type;
    unsigned int size;
    char source[MAX_NAME];
    char data[MAX_DATA];
} message;

enum {
    LOGIN = 1,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK
};

int sockfd = -1;
int logged_in = 0;
char client_id[MAX_NAME];
pthread_t recv_thread;

void reset_state() {
    if (sockfd != -1) {
        close(sockfd);
    }
    sockfd = -1;
    logged_in = 0;
    memset(client_id, 0, sizeof(client_id));
}

void *receiver(void *arg) {
    message msg;

    while (1) {
        int n = recv(sockfd, &msg, sizeof(msg), 0);

        if (n <= 0) {
            if (logged_in) {
                printf("Logged out / disconnected from server\n");
            }
            reset_state();
            break;
        }

        if (msg.type == MESSAGE) {
            printf("%s: %s\n", msg.source, msg.data);
        } else {
            printf("%s\n", msg.data);
        }
    }

    return NULL;
}

void send_msg(int type, char *data) {
    message msg;

    if (!logged_in || sockfd == -1) {
        printf("Please /login first\n");
        return;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    strcpy(msg.source, client_id);

    if (data) {
        strcpy(msg.data, data);
        msg.size = strlen(data);
    }

    send(sockfd, &msg, sizeof(msg), 0);
}

int main() {
    char input[1024];

    while (1) {
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;

        if (strncmp(input, "/login ", 7) == 0) {
            char id[50], pass[50], ip[50];
            int port;

            if (logged_in) {
                printf("Already logged in\n");
                continue;
            }

            if (sscanf(input, "/login %49s %49s %49s %d", id, pass, ip, &port) != 4) {
                printf("Usage: /login <id> <password> <server-ip> <port>\n");
                continue;
            }

            sockfd = socket(AF_INET, SOCK_STREAM, 0);

            struct sockaddr_in server;
            memset(&server, 0, sizeof(server));
            server.sin_family = AF_INET;
            server.sin_port = htons(port);

            if (inet_pton(AF_INET, ip, &server.sin_addr) <= 0) {
                printf("Invalid IP address\n");
                reset_state();
                continue;
            }

            if (connect(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0) {
                printf("Connection failed\n");
                reset_state();
                continue;
            }

            strcpy(client_id, id);

            message msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = LOGIN;
            strcpy(msg.source, id);
            strcpy(msg.data, pass);
            msg.size = strlen(pass);

            send(sockfd, &msg, sizeof(msg), 0);
            recv(sockfd, &msg, sizeof(msg), 0);

            if (msg.type == LO_ACK) {
                logged_in = 1;
                printf("Login successful\n");
                pthread_create(&recv_thread, NULL, receiver, NULL);
            } else {
                printf("Login failed: %s\n", msg.data);
                reset_state();
            }
        }

        else if (strcmp(input, "/logout") == 0) {
            if (!logged_in) {
                printf("You are not logged in\n");
                continue;
            }

            send_msg(EXIT, "");
            pthread_join(recv_thread, NULL);
            printf("You can login again now\n");
        }

        else if (strncmp(input, "/joinsession ", 13) == 0) {
            char session[50];
            sscanf(input, "/joinsession %49s", session);
            send_msg(JOIN, session);
        }

        else if (strcmp(input, "/leavesession") == 0) {
            send_msg(LEAVE_SESS, "");
        }

        else if (strncmp(input, "/createsession ", 15) == 0) {
            char session[50];
            sscanf(input, "/createsession %49s", session);
            send_msg(NEW_SESS, session);
        }

        else if (strcmp(input, "/list") == 0) {
            send_msg(QUERY, "");
        }

        else if (strcmp(input, "/quit") == 0) {
            if (logged_in) {
                send_msg(EXIT, "");
                pthread_join(recv_thread, NULL);
            }
            break;
        }

        else {
            send_msg(MESSAGE, input);
        }
    }

    return 0;
}
