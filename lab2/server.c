//Section 2.2 requirement (server): reassemble the file.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSIZE 1200

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: server <port>\n");
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serv_addr, cli_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    char buffer[BUFSIZE];
    FILE *fp = NULL;

    while (1) {
        socklen_t addr_len = sizeof(cli_addr);
        int n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&cli_addr, &addr_len);
        if (n <= 0) continue;

        int total, frag_no, size;
        char filename[256];
        int header_end = 0;
        int colon_count = 0;

        for (int i = 0; i < n; i++) {
            if (buffer[i] == ':') {
                colon_count++;
                if (colon_count == 4) {
                    header_end = i;
                    break;
                }
            }
        }

        sscanf(buffer, "%d:%d:%d:%[^:]:", &total, &frag_no, &size, filename);

        if (frag_no == 1) {
            fp = fopen(filename, "wb");
        }

        if (fp) {
            fwrite(buffer + header_end, 1, size, fp);
        }

        sendto(sockfd, "ACK", 3, 0, (struct sockaddr *)&cli_addr, addr_len);
        if (frag_no == total) {
            printf("File %s received successfully.\n", filename);
            fclose(fp);
            break;
        }

    }

    close(sockfd);
    return 0;
    
}