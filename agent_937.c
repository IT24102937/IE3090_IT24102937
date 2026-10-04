#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BUFFER_SIZE 1024

#define SESSION_ID "SID:7392"
#define AUTH_TOKEN "OPS-2937"

int main() {

    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    char buffer[BUFFER_SIZE];

    /* Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    printf("[AGENT] TCP socket created.\n");

    /* Configure server */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* Bind */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {

        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    printf("[AGENT] Bound to port %d.\n", PORT);

    /* Listen */
    if (listen(server_fd, 5) < 0) {

        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    printf("[AGENT] Waiting for Controller...\n");

    /* Accept Controller */
    client_fd = accept(server_fd,
                       (struct sockaddr *)&client_addr,
                       &client_len);

    if (client_fd < 0) {

        perror("Accept failed");
        close(server_fd);
        return 1;
    }

    printf("[AGENT] Controller connected.\n");

    /* Receive AUTH message */
    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(client_fd,
                              buffer,
                              BUFFER_SIZE - 1,
                              0);

    if (bytes_received < 0) {

        perror("Receive failed");

        close(client_fd);
        close(server_fd);

        return 1;
    }

    printf("[AGENT] Received: %s\n", buffer);

    /* Check authentication */
    char expected_auth[BUFFER_SIZE];

    snprintf(expected_auth,
             BUFFER_SIZE,
             "AUTH %s %s",
             SESSION_ID,
             AUTH_TOKEN);

    if (strcmp(buffer, expected_auth) == 0) {

        char response[] = "AUTH OK";

        send(client_fd,
             response,
             strlen(response),
             0);

        printf("[AGENT] Authentication successful.\n");

    } else {

        char response[] = "AUTH FAILED";

        send(client_fd,
             response,
             strlen(response),
             0);

        printf("[AGENT] Authentication failed.\n");
    }

    /* Close connection */
    close(client_fd);
    close(server_fd);

    printf("[AGENT] Connection closed.\n");

    return 0;
}
