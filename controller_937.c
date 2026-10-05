#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9410
#define BUFFER_SIZE 4096

#define SESSION_ID "SID:7392"
#define AUTH_TOKEN "OPS-2937"

int main(void)
{
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("[CONTROLLER] Connected to Agent.\n");

    /* ---------------- AUTH ---------------- */

    char auth_message[BUFFER_SIZE];

    snprintf(auth_message, sizeof(auth_message),
             "AUTH %s %s", SESSION_ID, AUTH_TOKEN);

    send(sockfd, auth_message, strlen(auth_message), 0);

    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer) - 1, 0);

    printf("[CONTROLLER] Agent response: %s\n", buffer);

    if (strcmp(buffer, "AUTH OK") != 0)
    {
        printf("[CONTROLLER] Authentication failed.\n");
        close(sockfd);
        return 1;
    }

    /* ---------------- GET ---------------- */

    char command[BUFFER_SIZE];

    strcpy(command, "GET testfile.txt");

    send(sockfd, command, strlen(command), 0);

    printf("[CONTROLLER] Command sent: %s\n", command);

    /* Receive FILESIZE message */

    memset(buffer, 0, sizeof(buffer));

    int received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

    if (received <= 0)
    {
        printf("[CONTROLLER] Failed to receive FILESIZE.\n");
        close(sockfd);
        return 1;
    }

    buffer[received] = '\0';

    printf("[CONTROLLER] Agent response: %s\n", buffer);

    long filesize;

    if (sscanf(buffer, "FILESIZE %ld", &filesize) != 1)
    {
        printf("[CONTROLLER] Invalid FILESIZE response.\n");
        close(sockfd);
        return 1;
    }

    printf("[CONTROLLER] File size: %ld bytes\n", filesize);

    /* Tell Agent that Controller is ready */

    send(sockfd, "READY", 5, 0);

    /* Open output file */

    FILE *file = fopen("downloaded_testfile.txt", "wb");

    if (file == NULL)
    {
        perror("fopen");
        close(sockfd);
        return 1;
    }

    /* Receive file */

    long total_received = 0;

    while (total_received < filesize)
    {
        int remaining = filesize - total_received;

        int chunk_size =
            remaining < BUFFER_SIZE ? remaining : BUFFER_SIZE;

        int bytes = recv(sockfd, buffer, chunk_size, 0);

        if (bytes <= 0)
        {
            printf("[CONTROLLER] File transfer interrupted.\n");
            fclose(file);
            close(sockfd);
            return 1;
        }

        fwrite(buffer, 1, bytes, file);

        total_received += bytes;
    }

    fclose(file);

    printf("[CONTROLLER] GET successful.\n");
    printf("[CONTROLLER] File saved as downloaded_testfile.txt\n");
    printf("[CONTROLLER] Received %ld bytes.\n", total_received);

    close(sockfd);

    return 0;
}
