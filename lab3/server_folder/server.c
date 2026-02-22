// server test: simulate packet drops
// -> Upon receiving a packet, the server draw a random # and only send back an ACK if the # exceeds some threshold
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define BUF_SIZE 4096
#define DROP_RATE 0.1 // drop ~10% of packets

int rev_packet(const char *buf, size_t buf_len,
                 unsigned int *total_frag,
                 unsigned int *frag_no,
                 unsigned int *size,
                 char *filename,
                 size_t *data_start)
{
    size_t col[4];
    int count = 0;

    for (size_t i = 0; i < buf_len && count < 4; i++)
        if (buf[i] == ':'){
            col[count] = i;
            count++;
        }
    if (count < 4) return 0;

    char a[32], b[32], c[32];
    memcpy(a, buf, col[0]); a[col[0]] = '\0';
    memcpy(b, buf + col[0] + 1, col[1]-col[0]-1); b[col[1]-col[0]-1] = '\0';
    memcpy(c, buf + col[1] + 1, col[2]-col[1]-1); c[col[2]-col[1]-1] = '\0';

    size_t fname_len = col[3] - col[2] - 1;
    memcpy(filename, buf + col[2] + 1, fname_len);
    filename[fname_len] = '\0';

    *total_frag = atoi(a);
    *frag_no = atoi(b);
    *size = atoi(c);
    *data_start = col[3] + 1;

    if (*data_start + *size > buf_len){
        return 0;
    }

    return 1;
}

void send_ctrl(int sockfd, struct sockaddr *client, socklen_t client_len,
               const char *type, unsigned int frag_no) {
    char msg[64];
    int n = snprintf(msg, sizeof(msg), "%s:%u", type, frag_no); //"ACK OR NACK:frag_no"
    sendto(sockfd, msg, n, 0, client, client_len);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    srand(time(NULL)); //seed for random number generator

    //create socket and bind it
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    getaddrinfo(NULL, argv[1], &hints, &res);

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(sockfd, res->ai_addr, res->ai_addrlen);
    //freeaddrinfo(res);

    printf("Server listening on port %s...\n", argv[1]);

    //transfer
    FILE *fp = NULL;
    char filename[256];
    char buf[BUF_SIZE];

    while (1) {
        struct sockaddr_storage client;
        socklen_t client_len = sizeof(client);

        ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0,
                             (struct sockaddr *)&client, &client_len); // n is bytes recevied


        unsigned int total_frag, frag_no, size;
        size_t data_start;

        if (!rev_packet(buf, n, &total_frag, &frag_no, &size, filename, &data_start)) {
            send_ctrl(sockfd, (struct sockaddr *)&client, client_len, "NACK", 0);
            continue;
        }

        // Simulate packet drop
        if (((double)rand() / RAND_MAX) < DROP_RATE) {
            printf("Packet dropped: fragment %u was lost\n", frag_no);
            continue; // No ACK
        }


        if (frag_no == 1) {
            if (fp) fclose(fp);
            fp = fopen(filename, "wb");
        
            printf("Receiving file: %s (%u fragments)\n", filename, total_frag);
        }

        
        //last fragment recevied and close
        if (frag_no == total_frag) {
            fclose(fp);
            fp = NULL;
            printf("File %s received successfully.\n", filename);
            break;
        }
    }

    close(sockfd);
    return 0;
}
