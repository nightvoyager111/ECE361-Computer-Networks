#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: deliver <hostname> <UDP port>\n");
        exit(1);
    }

    const char *server_addr_str = argv[1];
    const char *server_port_str = argv[2];

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int rv;
    if ((rv = getaddrinfo(server_addr_str, server_port_str, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    int sockfd;
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
        perror("client: socket");
        freeaddrinfo(servinfo);
        exit(1);
    }
    
    const int BUFSIZE = 100;
    char input[BUFSIZE];
    char filename[BUFSIZE];
    printf("Enter message to send (ftp <filename>): ");
    fgets(input, BUFSIZE, stdin);

    char word[BUFSIZE];
    if(sscanf(input, "%s %s", word, filename) != 2 || strcmp(word, "ftp") != 0) {
        fprintf(stderr, "Invalid input format. Use: ftp <filename>\n");
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }

    if (access(filename, F_OK) == -1) {
        perror("File check");
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }

    int numbytes;
    if ((numbytes = sendto(sockfd, "ftp", 3, 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1) {
        perror("sendto");
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }

    char recv_buf[BUFSIZE];
    struct sockaddr_storage sender_addr;
    socklen_t addr_len = sizeof sender_addr;

    numbytes = recvfrom(sockfd, recv_buf, BUFSIZE - 1, 0, (struct sockaddr *)&sender_addr, &addr_len);
    if (numbytes == -1) {
        perror("recvfrom");
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }

    recv_buf[numbytes] = '\0';

    if(strcmp(recv_buf, "yes") == 0) {
        printf("A file transfer can start.\n");
    } else {
        printf("File transfer cannot start.\n");
    }
    freeaddrinfo(servinfo);
    close(sockfd);
    return 0;
}