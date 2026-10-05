#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PORT 9410
#define BUFFER_SIZE 1024
#define SYSINFO_BUFFER 4096
#define PROCESS_BUFFER 8192
#define FILE_BUFFER 4096

#define SESSION_ID "SID:7392"
#define AUTH_TOKEN "OPS-2937"

#define STORAGE_PATH "./agentfiles/IT24102937/"


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


int is_allowed_command(const char *command) {

    if (strcmp(command, "date") == 0) {
        return 1;
    }

    if (strcmp(command, "whoami") == 0) {
        return 1;
    }

    return 0;
}


void execute_command(int client_fd, const char *command) {

    FILE *fp;
    char buffer[4096];
    char line[512];

    memset(buffer, 0, sizeof(buffer));

    fp = popen(command, "r");

    if (fp == NULL) {
        char error_msg[] = "EXEC ERROR";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strlen(buffer) + strlen(line) < sizeof(buffer) - 1) {
            strcat(buffer, line);
        }
    }

    pclose(fp);

    if (strlen(buffer) == 0) {
        strcpy(buffer, "Command executed successfully.");
    }

    send(client_fd, buffer, strlen(buffer), 0);
}


/* Receive file from Controller */

void receive_file(int client_fd, const char *filename, long filesize) {

    char filepath[512];

    snprintf(filepath,
             sizeof(filepath),
             "%s%s",
             STORAGE_PATH,
             filename);

    FILE *fp = fopen(filepath, "wb");

    if (fp == NULL) {
        char error_msg[] = "PUT ERROR";
        send(client_fd, error_msg, strlen(error_msg), 0);
        return;
    }

    char file_buffer[FILE_BUFFER];
    long total_received = 0;

    while (total_received < filesize) {

        long remaining = filesize - total_received;

        int chunk_size;

        if (remaining < FILE_BUFFER) {
            chunk_size = (int)remaining;
        } else {
            chunk_size = FILE_BUFFER;
        }

        int bytes_received = recv(client_fd,
                                  file_buffer,
                                  chunk_size,
                                  0);

        if (bytes_received <= 0) {
            fclose(fp);
            return;
        }

        fwrite(file_buffer,
               1,
               bytes_received,
               fp);

        total_received += bytes_received;
    }

    fclose(fp);

    char response[] = "PUT OK";

    send(client_fd,
         response,
         strlen(response),
         0);

    printf("[AGENT] File received: %s (%ld bytes)\n",
           filepath,
           filesize);
}


/* Send file to Controller */

void send_file(int client_fd, const char *filename) {

    char filepath[512];

    snprintf(filepath,
             sizeof(filepath),
             "%s%s",
             STORAGE_PATH,
             filename);

    FILE *fp = fopen(filepath, "rb");

    if (fp == NULL) {

        char error_msg[] = "GET ERROR";

        send(client_fd,
             error_msg,
             strlen(error_msg),
             0);

        return;
    }

    fseek(fp, 0, SEEK_END);

    long filesize = ftell(fp);

    fseek(fp, 0, SEEK_SET);


    /* Send file size */

    char header[128];

    snprintf(header,
             sizeof(header),
             "FILESIZE %ld",
             filesize);

    send(client_fd,
         header,
         strlen(header),
         0);


    /* Wait for READY */

    char ready[BUFFER_SIZE];

    memset(ready, 0, sizeof(ready));

    recv(client_fd,
         ready,
         sizeof(ready) - 1,
         0);


    /* Send file */

    char file_buffer[FILE_BUFFER];

    size_t bytes_read;

    while ((bytes_read = fread(file_buffer,
                               1,
                               sizeof(file_buffer),
                               fp)) > 0) {

        send(client_fd,
             file_buffer,
             bytes_read,
             0);
    }

    fclose(fp);

    printf("[AGENT] File sent: %s (%ld bytes)\n",
           filepath,
           filesize);
}


int main() {

    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    char buffer[BUFFER_SIZE];


    /* Create storage directory */

    mkdir("agentfiles", 0755);
    mkdir(STORAGE_PATH, 0755);


    /* Create socket */

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    printf("[AGENT] TCP socket created.\n");


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


    /* Accept */

    client_fd = accept(server_fd,
                       (struct sockaddr *)&client_addr,
                       &client_len);

    if (client_fd < 0) {

        perror("Accept failed");

        close(server_fd);

        return 1;
    }

    printf("[AGENT] Controller connected.\n");


    /* Receive AUTH */

    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(client_fd,
                              buffer,
                              BUFFER_SIZE - 1,
                              0);

    if (bytes_received <= 0) {

        close(client_fd);
        close(server_fd);

        return 1;
    }

    printf("[AGENT] Received: %s\n", buffer);


    char expected_auth[BUFFER_SIZE];

    snprintf(expected_auth,
             BUFFER_SIZE,
             "AUTH %s %s",
             SESSION_ID,
             AUTH_TOKEN);


    /* AUTH */

    if (strcmp(buffer, expected_auth) == 0) {

        char response[] = "AUTH OK";

        send(client_fd,
             response,
             strlen(response),
             0);

        printf("[AGENT] Authentication successful.\n");


        /* Receive command */

        memset(buffer, 0, BUFFER_SIZE);

        bytes_received = recv(client_fd,
                              buffer,
                              BUFFER_SIZE - 1,
                              0);

        if (bytes_received > 0) {

            printf("[AGENT] Command received: %s\n", buffer);


            /* SYSINFO */

            if (strcmp(buffer, "SYSINFO") == 0) {

                send_sysinfo(client_fd);

                printf("[AGENT] SYSINFO sent.\n");
            }


            /* LISTPROC */

            else if (strcmp(buffer, "LISTPROC") == 0) {

                send_listproc(client_fd);

                printf("[AGENT] LISTPROC sent.\n");
            }


            /* EXEC */

            else if (strncmp(buffer, "EXEC ", 5) == 0) {

                char *command = buffer + 5;

                printf("[AGENT] EXEC requested: %s\n", command);

                if (is_allowed_command(command)) {

                    printf("[AGENT] Command allowed.\n");

                    execute_command(client_fd, command);

                } else {

                    char response[] = "EXEC DENIED";

                    send(client_fd,
                         response,
                         strlen(response),
                         0);

                    printf("[AGENT] Command denied.\n");
                }
            }


            /* PUT */

            else if (strncmp(buffer, "PUT ", 4) == 0) {

                char filename[256];
                long filesize;

                if (sscanf(buffer + 4,
                           "%255s %ld",
                           filename,
                           &filesize) == 2) {

                    printf("[AGENT] PUT requested: %s (%ld bytes)\n",
                           filename,
                           filesize);

                    receive_file(client_fd,
                                 filename,
                                 filesize);

                } else {

                    char response[] = "PUT INVALID";

                    send(client_fd,
                         response,
                         strlen(response),
                         0);
                }
            }


            /* GET */

            else if (strncmp(buffer, "GET ", 4) == 0) {

                char filename[256];

                if (sscanf(buffer + 4,
                           "%255s",
                           filename) == 1) {

                    printf("[AGENT] GET requested: %s\n",
                           filename);

                    send_file(client_fd,
                              filename);

                } else {

                    char response[] = "GET INVALID";

                    send(client_fd,
                         response,
                         strlen(response),
                         0);
                }
            }


            /* Unknown */

            else {

                char response[] = "UNKNOWN COMMAND";

                send(client_fd,
                     response,
                     strlen(response),
                     0);
            }
        }

    } else {

        char response[] = "AUTH FAILED";

        send(client_fd,
             response,
             strlen(response),
             0);

        printf("[AGENT] Authentication failed.\n");
    }


    close(client_fd);
    close(server_fd);

    printf("[AGENT] Connection closed.\n");

    return 0;
}
