#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define BUFFER_SIZE 1024

#define SESSION_ID "SID:7392"
#define AUTH_TOKEN "OPS-2937"

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

    /* Configure Agent address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0) {

        perror("Invalid server address");
        close(sock);

        return 1;
    }

    /* Connect */
    printf("[CONTROLLER] Connecting to Agent...\n");

    if (connect(sock,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {

        perror("Connection failed");
        close(sock);

        return 1;
    }

    printf("[CONTROLLER] Connected to Agent.\n");

    /* Create authentication message */
    char auth_message[BUFFER_SIZE];

    snprintf(auth_message,
             BUFFER_SIZE,
             "AUTH %s %s",
             SESSION_ID,
             AUTH_TOKEN);

    /* Send authentication */
    send(sock,
         auth_message,
         strlen(auth_message),
         0);

    printf("[CONTROLLER] Authentication request sent.\n");

    /* Receive authentication response */
    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(sock,
                              buffer,
                              BUFFER_SIZE - 1,
                              0);

    if (bytes_received < 0) {

        perror("Receive failed");

    } else {

        printf("[CONTROLLER] Agent response: %s\n",
               buffer);
    }

    /* Close */
    close(sock);

    printf("[CONTROLLER] Connection closed.\n");

    return 0;
}
