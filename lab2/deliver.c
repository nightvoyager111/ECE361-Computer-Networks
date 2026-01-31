//Section 2.1: RTT calculation: RTT = time_received - time_sent
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define MAX_DATA 1000
#define BUFSIZE 1200

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: deliver <hostname> <port>\n");
        return 1;
    }

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(argv[1], argv[2], &hints, &servinfo) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    char input[256], filename[256], command[16];
    printf("Enter: ftp <filename>\n");
    fgets(input, sizeof(input), stdin);
    sscanf(input, "%s %s", command, filename);

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("File not found");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);
    int total_frag = (filesize + MAX_DATA - 1) / MAX_DATA;

    char packet[BUFSIZE];
    char ack[10];
    struct timespec send_time, recv_time;

    for (int i = 1; i <= total_frag; i++) {
        char data[MAX_DATA];
        int bytes_read = fread(data, 1, MAX_DATA, fp);

        int header_size= sprintf(packet, "%d:%d:%d:%s:", total_frag, i, bytes_read, filename);

        memcpy(packet + header_size, data, bytes_read);
        int packet_size = header_size + bytes_read;

        clock_gettime(CLOCK_MONOTONIC, &send_time);

        if (sendto(sockfd, packet, packet_size, 0, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
            perror("sendto failed");
            break;  
        }

        if (recvfrom(sockfd, ack, sizeof(ack), 0, NULL, NULL) > 0) {
            clock_gettime(CLOCK_MONOTONIC, &recv_time);
            double rtt = (recv_time.tv_sec - send_time.tv_sec) + (recv_time.tv_nsec - send_time.tv_nsec) / 1e9;
            printf("Fragment %d sent. RTT: %f s\n", i, rtt);
        }
    }
    printf("File transfer completed.\n");
    fclose(fp);
    freeaddrinfo(servinfo);
    close(sockfd);
    return 0;

}