#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BUFFER_SIZE 1024
#define SYSINFO_BUFFER 4096
#define PROCESS_BUFFER 8192

#define SESSION_ID "SID:7392"
#define AUTH_TOKEN "OPS-2937"


void send_sysinfo(int client_fd) {
    FILE *fp;
    char buffer[SYSINFO_BUFFER];
    char line[512];

    memset(buffer, 0, sizeof(buffer));

    fp = popen(
        "hostname; uname -s; uname -r; lscpu | grep 'Model name' | head -1",
        "r"
    );

    if (fp == NULL) {
        char error_msg[] = "SYSINFO ERROR";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strlen(buffer) + strlen(line) < sizeof(buffer) - 1) {
            strcat(buffer, line);
        }
    }

    pclose(fp);

    send(client_fd, buffer, strlen(buffer), 0);
}


void send_listproc(int client_fd) {
    FILE *fp;
    char buffer[PROCESS_BUFFER];
    char line[512];

    memset(buffer, 0, sizeof(buffer));

    fp = popen(
        "ps -eo pid,comm,user --sort=pid | head -30",
        "r"
    );

    if (fp == NULL) {
        char error_msg[] = "LISTPROC ERROR";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strlen(buffer) + strlen(line) < sizeof(buffer) - 1) {
            strcat(buffer, line);
        }
    }

    pclose(fp);

    send(client_fd, buffer, strlen(buffer), 0);
}


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


    /* Configure server address */

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);


    /* Bind socket */

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


    /* Accept controller connection */

    client_fd = accept(server_fd,
                        (struct sockaddr *)&client_addr,
                        &client_len);

    if (client_fd < 0) {

        perror("Accept failed");

        close(server_fd);

        return 1;
    }

    printf("[AGENT] Controller connected.\n");


    /* Receive authentication request */

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


    /* Create expected authentication message */

    char expected_auth[BUFFER_SIZE];

    snprintf(expected_auth,
             BUFFER_SIZE,
             "AUTH %s %s",
             SESSION_ID,
             AUTH_TOKEN);


    /* Check authentication */

    if (strcmp(buffer, expected_auth) == 0) {

        char response[] = "AUTH OK";

        send(client_fd,
             response,
             strlen(response),
             0);

        printf("[AGENT] Authentication successful.\n");


        /* Wait for command */

        memset(buffer, 0, BUFFER_SIZE);

        bytes_received = recv(client_fd,
                              buffer,
                              BUFFER_SIZE - 1,
                              0);

        if (bytes_received > 0) {

            printf("[AGENT] Command received: %s\n", buffer);


            /* SYSINFO command */

            if (strcmp(buffer, "SYSINFO") == 0) {

                send_sysinfo(client_fd);

                printf("[AGENT] SYSINFO sent.\n");
            }


            /* LISTPROC command */

            else if (strcmp(buffer, "LISTPROC") == 0) {

                send_listproc(client_fd);

                printf("[AGENT] LISTPROC sent.\n");
            }


            /* Unknown command */

            else {

                char response[] = "UNKNOWN COMMAND";

                send(client_fd,
                     response,
                     strlen(response),
                     0);

                printf("[AGENT] Unknown command.\n");
            }
        }

    }


    /* Authentication failed */

    else {

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
