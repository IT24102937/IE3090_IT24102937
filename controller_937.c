#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define BUFFER_SIZE 1024

int main() {

    int sock;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];

    /* Create TCP socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    printf("[CONTROLLER] TCP socket created.\n");

    /* Configure server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP,
                  &server_addr.sin_addr) <= 0) {

        perror("Invalid server address");
        close(sock);
        return 1;
    }

    /* Connect to Agent */
    printf("[CONTROLLER] Connecting to Agent...\n");

    if (connect(sock,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {

        perror("Connection failed");
        close(sock);
        return 1;
    }

    printf("[CONTROLLER] Connected to Agent.\n");

    /* Send test message */
    char message[] = "Hello from Controller";

    send(sock,
         message,
         strlen(message),
         0);

    printf("[CONTROLLER] Message sent.\n");

    /* Receive Agent response */
    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(sock,
                              buffer,
                              BUFFER_SIZE - 1,
                              0);

    if (bytes_received < 0) {

        perror("Receive failed");

    } else {

        printf("[CONTROLLER] Agent response: %s\n", buffer);
    }

    /* Close connection */
    close(sock);

    printf("[CONTROLLER] Connection closed.\n");

    return 0;
}
