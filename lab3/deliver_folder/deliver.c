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
#include <sys/time.h>
#include <math.h>

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

    // RTT estimation variables
    double estimated_rtt = 0.1;
    double dev_rtt = 0.0;
    double timeout_interval = 0.5;
    const double alpha = 0.125, beta = 0.25;


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

    for (unsigned int frag_no = 1; frag_no <= total_frag; frag_no++) {
        size_t size = fread(data, 1, DATA_SIZE, fp);
        int header_len = snprintf(packet, sizeof(packet), "%u:%u:%zu:%s:", total_frag, frag_no, size, filename);
        memcpy(packet + header_len, data, size);
            

        while (1) { 
            struct timeval tv;
            tv.tv_sec = (long)timeout_interval;
            tv.tv_usec = (long)((timeout_interval - tv.tv_sec) * 1e6);
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            clock_gettime (CLOCK_MONOTONIC, &sendtime); //
            sendto(sockfd, packet, header_len + size, 0, res->ai_addr, res->ai_addrlen);
            
            struct sockaddr_storage server_addr;
            socklen_t server_len = sizeof(server_addr);
            ssize_t rn = recvfrom(sockfd, ctrlbuf, sizeof(ctrlbuf)-1, 0, (struct sockaddr *)&server_addr, &server_len);
            
            if (rn < 0) {
                printf("Timeout. Retransmitting fragment %u\n", frag_no);
                timeout_interval *= 1.5;
                continue;
            }
            
            ctrlbuf[rn] = '\0'; 
            clock_gettime(CLOCK_MONOTONIC, &recvtime);
            double rtt = (recvtime.tv_sec - sendtime.tv_sec) + (recvtime.tv_nsec - sendtime.tv_nsec)/1e9;

            estimated_rtt = (1 - alpha) * estimated_rtt + alpha * rtt;
            dev_rtt = (1 - beta) * dev_rtt + beta * fabs(rtt - estimated_rtt);
            timeout_interval = estimated_rtt + 4 * dev_rtt;
            if (timeout_interval < 0.01) timeout_interval = 0.1;

            unsigned int ack_no;
            if (sscanf(ctrlbuf, "ACK: %u", &ack_no) == 1 && ack_no == frag_no) {
                printf("Fragment %u sent. RTT: %f s\n", frag_no, rtt);
                break;
            } else {
                printf("Received NACK or wrong ACK. Retransmitting fragment %u\n", frag_no);
            }

           
        }
    }

    printf("File transfer completed.\n");
    fclose(fp);
    close(sockfd);
    freeaddrinfo(res);
    return 0;
}
