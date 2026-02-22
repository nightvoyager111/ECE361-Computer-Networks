// lab3 1. If one packet from the client is lost. 2. If an ACK/NACK packet is lost
// client logic: implement a timer for ACK/NACK at the client side based on RTT
// -> timeout: retransmit and print a message
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>

#define DATA_SIZE 1000
#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server address> <server port>\n", argv[0]);
        return 1;
    }

    char cmd[16], filename[256];
    printf("ftp <file name>\n");
    if (scanf("%15s %255s", cmd, filename) != 2) return 1;
    if (strcmp(cmd, "ftp") != 0) return 1;

    FILE *fp = fopen(filename, "rb");
    if (!fp) return 1;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    getaddrinfo(argv[1], argv[2], &hints, &res);

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    fseek(fp, 0, SEEK_END); //move fp to the end of file
    long filesize = ftell(fp); //size of file
    rewind(fp);//move fp to start of file

    //calculate total_frag need to transfer this file
    unsigned int total_frag = (filesize + DATA_SIZE - 1) / DATA_SIZE;

    char packet[BUF_SIZE];
    char ctrlbuf[128]; //ACK or NACK
    unsigned char data[DATA_SIZE];
    struct timespec sendtime, recvtime; //time logic
    // double expected_rtt = 0.5;

    // For section 3
    // RTO variables 
    double srtt = -1.0;
    double rttvar = 0.0;
    double rto = 1.0; // Initial RTO = 1 second before any RTT measurements

    for (unsigned int frag_no = 1; frag_no <= total_frag; ) {
        static int data_loaded = 0; // flag
        static size_t size = 0;
        static int header_len = 0;

        if (!data_loaded) {
            size = fread(data, 1, DATA_SIZE, fp); //read up to 1000Bytes 
            header_len = snprintf(packet, sizeof(packet),
                                 "%u:%u:%zu:%s:", total_frag, frag_no, size, filename);
            memcpy(packet + header_len, data, size);
            data_loaded = 1;
        }

        struct timeval tv;
        tv.tv_sec = (time_t)rto;
        tv.tv_usec = (suseconds_t)((rto - tv.tv_sec));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        clock_gettime (CLOCK_MONOTONIC, &sendtime); //
        sendto(sockfd, packet, header_len + size, 0, res->ai_addr, res->ai_addrlen);

        while (1) { //why do we need this loop
            struct sockaddr_storage server;

            socklen_t server_len = sizeof(server);

            ssize_t rn = recvfrom(sockfd, ctrlbuf, sizeof(ctrlbuf)-1, 0, (struct sockaddr *)&server, &server_len);
            
            if (rn < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("Timeout on fragment %u, retransmit\n", frag_no);
                    data_loaded = 1; 
                    continue;
                }
                perror("recvfrom"); // other errors
                break;
            }
            
            ctrlbuf[rn] = '\0'; 
            clock_gettime(CLOCK_MONOTONIC, &recvtime);
            double rtt = (recvtime.tv_sec - sendtime.tv_sec) + (recvtime.tv_nsec - sendtime.tv_nsec)/1e9;

            char type[8];
            unsigned int fno;

            if (sscanf(ctrlbuf, "%7[^:]:%u", type, &fno) != 2) continue;
            if (fno != frag_no) continue;

            if (strcmp(type, "NACK") == 0) {
                printf("NACK received for fragment %u, retransmi\n", frag_no);
                data_loaded = 1;
                continue;
            }
                    
            if (strcmp(type, "ACK") == 0) {
                // update SRTT, RTTVAR, RTO using TCP formula
                if (srtt < 0) {
                    //first measurement
                    srtt = rtt;
                    rttvar = rtt / 2.0; 
                } else {
                    double alpha = 0.875, beta = 0.75;
                    rttvar = beta * rttvar + (1-beta) * fabs(srtt - rtt);
                    srtt = alpha * srtt + (1-alpha) * rtt;
                }
                rto = srtt + 4 * rttvar;
                if (rto < 0.1) rto = 0.1;
                printf("Fragment %u sent. RTT: %f s, RTO: %f s\n", frag_no, rtt, rto);
                frag_no++;
                data_loaded = 0;
            }
        }
    }

    printf("File transfer completed.\n");
    fclose(fp);
    close(sockfd);
    freeaddrinfo(res);
    return 0;
}
