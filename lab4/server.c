#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#define MAX_NAME 50
#define MAX_DATA 1024
#define MAX_CLIENTS 10
#define MAX_SESSIONS 10
#define BACKLOG 10

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

typedef struct {
    char id[MAX_NAME];
    char password[MAX_NAME];
} credential_t;

typedef struct {
    int active;
    int sockfd;
    char id[MAX_NAME];
    char session_id[MAX_NAME];
    char ip[INET_ADDRSTRLEN];
    int port;
} client_t;

typedef struct {
    int active;
    char session_id[MAX_NAME];
} session_t;

static credential_t registered_users[] = {
    {"tammy", "123"},
    {"amelia", "456"},
    {"alice", "hi"},
    {"bob", "bob123"}
};

static const int registered_count =
    (int)(sizeof(registered_users) / sizeof(registered_users[0]));

static client_t clients[MAX_CLIENTS];
static session_t sessions[MAX_SESSIONS];

static int send_all(int sockfd, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = (const char *)buf;
    while (total < len) {
        ssize_t n = send(sockfd, p + total, len - total, 0);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

static int recv_all(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    char *p = (char *)buf;
    while (total < len) {
        ssize_t n = recv(sockfd, p + total, len - total, 0);
        if (n == 0) return 0;
        if (n < 0) return -1;
        total += (size_t)n;
    }
    return 1;
}

static void init_message(message *msg, unsigned int type, const char *source, const char *data) {
    memset(msg, 0, sizeof(*msg));
    msg->type = type;
    if (source) strncpy(msg->source, source, MAX_NAME - 1);
    if (data) {
        strncpy(msg->data, data, MAX_DATA - 1);
        msg->size = (unsigned int)strlen(msg->data);
    } else {
        msg->size = 0;
    }
}

static void send_reply(int sockfd, unsigned int type, const char *data) {
    message msg;
    init_message(&msg, type, "server", data);
    send_all(sockfd, &msg, sizeof(msg));
}

static int valid_user(const char *id, const char *pass) {
    for (int i = 0; i < registered_count; i++) {
        if (strcmp(registered_users[i].id, id) == 0 &&
            strcmp(registered_users[i].password, pass) == 0) {
            return 1;
        }
    }
    return 0;
}

static int find_client_by_sock(int sockfd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].sockfd == sockfd) return i;
    }
    return -1;
}

static int find_client_by_id(const char *id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].id, id) == 0) return i;
    }
    return -1;
}

static int alloc_client_slot(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) return i;
    }
    return -1;
}

static int find_session(const char *sid) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && strcmp(sessions[i].session_id, sid) == 0) return i;
    }
    return -1;
}

static int alloc_session_slot(void) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].active) return i;
    }
    return -1;
}

static int session_has_members(const char *sid) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].session_id, sid) == 0) return 1;
    }
    return 0;
}

static void remove_empty_session(const char *sid) {
    if (sid == NULL || sid[0] == '\0') return;
    if (session_has_members(sid)) return;

    int s = find_session(sid);
    if (s >= 0) {
        sessions[s].active = 0;
        memset(sessions[s].session_id, 0, sizeof(sessions[s].session_id));
    }
}

static void remove_client(int idx) {
    if (idx < 0 || idx >= MAX_CLIENTS || !clients[idx].active) return;

    char old_session[MAX_NAME];
    memset(old_session, 0, sizeof(old_session));
    strncpy(old_session, clients[idx].session_id, MAX_NAME - 1);

    close(clients[idx].sockfd);

    clients[idx].active = 0;
    clients[idx].sockfd = -1;
    memset(clients[idx].id, 0, sizeof(clients[idx].id));
    memset(clients[idx].session_id, 0, sizeof(clients[idx].session_id));
    memset(clients[idx].ip, 0, sizeof(clients[idx].ip));
    clients[idx].port = 0;

    remove_empty_session(old_session);
}

static void broadcast_to_session(const char *sid, const message *msg) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].session_id, sid) == 0) {
            send_all(clients[i].sockfd, msg, sizeof(*msg));
        }
    }
}

static void handle_login(int sockfd, const message *msg, struct sockaddr_in *peer) {
    if (!valid_user(msg->source, msg->data)) {
        send_reply(sockfd, LO_NAK, "Invalid ID or password");
        return;
    }

    if (find_client_by_id(msg->source) >= 0) {
        send_reply(sockfd, LO_NAK, "User already logged in");
        return;
    }

    int slot = alloc_client_slot();
    if (slot < 0) {
        send_reply(sockfd, LO_NAK, "Server full");
        return;
    }

    clients[slot].active = 1;
    clients[slot].sockfd = sockfd;
    strncpy(clients[slot].id, msg->source, MAX_NAME - 1);
    clients[slot].session_id[0] = '\0';
    inet_ntop(AF_INET, &peer->sin_addr, clients[slot].ip, sizeof(clients[slot].ip));
    clients[slot].port = ntohs(peer->sin_port);

    send_reply(sockfd, LO_ACK, "Login successful");
}

static void handle_new_session(int sockfd, const message *msg) {
    int c = find_client_by_sock(sockfd);
    if (c < 0) return;

    if (clients[c].session_id[0] != '\0') {
        send_reply(sockfd, JN_NAK, "Already in a session");
        return;
    }

    if (find_session(msg->data) >= 0) {
        send_reply(sockfd, JN_NAK, "Session already exists");
        return;
    }

    int s = alloc_session_slot();
    if (s < 0) {
        send_reply(sockfd, JN_NAK, "Session table full");
        return;
    }

    sessions[s].active = 1;
    strncpy(sessions[s].session_id, msg->data, MAX_NAME - 1);
    strncpy(clients[c].session_id, msg->data, MAX_NAME - 1);

    send_reply(sockfd, NS_ACK, msg->data);
}

static void handle_join(int sockfd, const message *msg) {
    int c = find_client_by_sock(sockfd);
    if (c < 0) return;

    if (clients[c].session_id[0] != '\0') {
        send_reply(sockfd, JN_NAK, "Already in a session");
        return;
    }

    if (find_session(msg->data) < 0) {
        send_reply(sockfd, JN_NAK, "Session does not exist");
        return;
    }

    strncpy(clients[c].session_id, msg->data, MAX_NAME - 1);
    send_reply(sockfd, JN_ACK, msg->data);
}

static void handle_leave(int sockfd) {
    int c = find_client_by_sock(sockfd);
    if (c < 0) return;

    if (clients[c].session_id[0] == '\0') {
        send_reply(sockfd, JN_NAK, "Not in a session");
        return;
    }

    char old_session[MAX_NAME];
    memset(old_session, 0, sizeof(old_session));
    strncpy(old_session, clients[c].session_id, MAX_NAME - 1);

    clients[c].session_id[0] = '\0';
    remove_empty_session(old_session);

    send_reply(sockfd, JN_ACK, "Left session");
}

static void handle_query(int sockfd) {
    char out[MAX_DATA];
    out[0] = '\0';

    strncat(out, "Online users:\n", MAX_DATA - strlen(out) - 1);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            char line[256];
            if (clients[i].session_id[0] != '\0') {
                snprintf(line, sizeof(line), "  %s (session: %s)\n",
                         clients[i].id, clients[i].session_id);
            } else {
                snprintf(line, sizeof(line), "  %s (session: none)\n",
                         clients[i].id);
            }
            strncat(out, line, MAX_DATA - strlen(out) - 1);
        }
    }

    strncat(out, "Available sessions:\n", MAX_DATA - strlen(out) - 1);
    int found = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active) {
            char line[128];
            snprintf(line, sizeof(line), "  %s\n", sessions[i].session_id);
            strncat(out, line, MAX_DATA - strlen(out) - 1);
            found = 1;
        }
    }
    if (!found) {
        strncat(out, "  none\n", MAX_DATA - strlen(out) - 1);
    }

    send_reply(sockfd, QU_ACK, out);
}

static void handle_chat(int sockfd, const message *msg) {
    int c = find_client_by_sock(sockfd);
    if (c < 0) return;

    if (clients[c].session_id[0] == '\0') {
        send_reply(sockfd, JN_NAK, "You are not in a session");
        return;
    }

    message out;
    init_message(&out, MESSAGE, clients[c].id, msg->data);
    broadcast_to_session(clients[c].session_id, &out);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: server <TCP port number to listen on>\n");
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0) {
        fprintf(stderr, "Invalid port\n");
        return 1;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = 0;
        clients[i].sockfd = -1;
    }
    for (int i = 0; i < MAX_SESSIONS; i++) {
        sessions[i].active = 0;
    }

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        return 1;
    }

    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons((unsigned short)port);

    if (bind(listener, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("bind");
        close(listener);
        return 1;
    }

    if (listen(listener, BACKLOG) < 0) {
        perror("listen");
        close(listener);
        return 1;
    }

    fd_set master, readfds;
    FD_ZERO(&master);
    FD_SET(listener, &master);
    int fdmax = listener;

    printf("Server listening on port %d\n", port);

    while (1) {
        readfds = master;

        if (select(fdmax + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        for (int fd = 0; fd <= fdmax; fd++) {
            if (!FD_ISSET(fd, &readfds)) continue;

            if (fd == listener) {
                struct sockaddr_in peer;
                socklen_t plen = sizeof(peer);
                int newfd = accept(listener, (struct sockaddr *)&peer, &plen);
                if (newfd < 0) {
                    perror("accept");
                    continue;
                }

                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;
            } else {
                message msg;
                int status = recv_all(fd, &msg, sizeof(msg));

                if (status <= 0) {
                    int c = find_client_by_sock(fd);
                    if (c >= 0) remove_client(c);
                    else close(fd);
                    FD_CLR(fd, &master);
                    continue;
                }

                struct sockaddr_in peer;
                socklen_t plen = sizeof(peer);
                memset(&peer, 0, sizeof(peer));
                getpeername(fd, (struct sockaddr *)&peer, &plen);

                switch (msg.type) {
                    case LOGIN:
                        handle_login(fd, &msg, &peer);
                        break;
                    case EXIT: {
                        int c = find_client_by_sock(fd);
                        if (c >= 0) remove_client(c);
                        else close(fd);
                        FD_CLR(fd, &master);
                        break;
                    }
                    case NEW_SESS:
                        handle_new_session(fd, &msg);
                        break;
                    case JOIN:
                        handle_join(fd, &msg);
                        break;
                    case LEAVE_SESS:
                        handle_leave(fd);
                        break;
                    case QUERY:
                        handle_query(fd);
                        break;
                    case MESSAGE:
                        handle_chat(fd, &msg);
                        break;
                    default:
                        send_reply(fd, LO_NAK, "Unknown request");
                        break;
                }
            }
        }
    }

    close(listener);
    return 0;
}
