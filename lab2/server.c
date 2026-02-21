//Section 2.2 requirement (server): reassemble the file.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSIZE 1200

int main(int argc, char const *argv[]) {
    // 1. Check the port number from command line argument
    if (argc != 2) {
        fprintf(stderr, "Usage: server <port>\n");
        return 1;
    }
    // 2. Create a UDP socket and bind it to the specified port
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serv_addr, cli_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    char buffer[BUFSIZE]; //store the header + data
    FILE *fp = NULL;
    // 3. Main loop to receive fragments and reassemble the file
    while (1) {
        socklen_t addr_len = sizeof(cli_addr);
        ssize_t n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&cli_addr, &addr_len);
        // recvfrom: wait for a fragment
        if (n <= 0) continue;

        int total, frag_no, size;
        char filename[256];
        int header_end = -1;
        int colon_count = 0;

        // parsing the header
        for (int i = 0; i < n; i++) {
            if (buffer[i] == ':') {
                colon_count++;
                if (colon_count == 4) {
                    header_end = i + 1;
                    break;
                }
            }
        }

        

        char header[256];
        int header_len = header_end - 1;
        memcpy(header, buffer, header_len);
        header[header_len] = '\0';
        sscanf(header, "%d:%d:%d:%s", &total, &frag_no, &size, filename);

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
