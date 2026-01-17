// This is a UDP server program that responds "yes" if the messsage received is "ftp"
// and "no" otherwise.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(int argc, char const *argv[]) {
    // 1. Check the port number from command line argument
    if (argc != 2) {
        fprintf(stderr, "Usage: server <UDP listen port>\n");
        exit(1);
    }

    int port = atoi(argv[1]); 

    // 2. Create a UDP socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(1);
    }

    // 3. Set up the server address structure
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, izeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 4. Bind the socket to the sepecified port
    if ((bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) == -1) {
        perror("bind failed");
        close(sockfd);
        exit(1);            
    }

    printf("Server is listening on port %d...\n", port);

    // 5. Listen for incoming messages
    const int BUFSIZE = 100;
    char buffer[BUFSIZE];
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    // 6. Receive
    int numbytes = recvfrom(sockfd, buffer, BUFSIZE - 1, 0, (struct sockaddr *) &cli_addr, &cli_len);
    if(numbytes == -1) {
        perror("recvfrom failed");
        close(sockfd);
        exit(1);
    }

    buffer[numbytes] = '\0';
    printf("Received message: %s\n", buffer);

    // 7. Check the message
    if (strcmp(buffer, "ftp") == 0) {
        if (sendto(sockfd, "yes", 3, 0, (struct sockaddr *) &cli_addr, cli_len) == -1) {
            perror("sendto failed");
        } 
    } else {
        if (sendto(sockfd, "no", 2, 0, (struct sockaddr *) &cli_addr, cli_len) == -1) {
            perror("sendto failed");
        }
    }

    close(sockfd);
    return 0;
}
