#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define TCP_PORT 9410
#define UDP_PORT 9411

#define BUFFER_SIZE 4096

#define SESSION_ID "SID:7392"
#define AUTH_TOKEN "OPS-2937"

#define STORAGE_PATH "./agentfiles/IT24102937/"

/* Send all TCP data */
int send_all(int socket_fd, const char *data, size_t length)
{
    size_t total = 0;

    while (total < length)
    {
        ssize_t sent = send(socket_fd, data + total, length - total, 0);

        if (sent <= 0)
            return -1;

        total += sent;
    }

    return 0;
}

/* SYSINFO */
void send_sysinfo(int client_fd)
{
    char buffer[BUFFER_SIZE];

    FILE *fp = popen(
        "hostname; uname -s; uname -r; "
        "lscpu | grep 'Model name' | head -1",
        "r");

    if (fp == NULL)
    {
        send_all(client_fd, "SYSINFO ERROR", 13);
        return;
    }

    size_t total = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[total] = '\0';

    pclose(fp);

    send_all(client_fd, buffer, strlen(buffer));
}

/* LISTPROC */
void send_listproc(int client_fd)
{
    char buffer[BUFFER_SIZE * 2];

    FILE *fp = popen(
        "ps -eo pid,comm,user --sort=pid | head -30",
        "r");

    if (fp == NULL)
    {
        send_all(client_fd, "LISTPROC ERROR", 14);
        return;
    }

    size_t total = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[total] = '\0';

    pclose(fp);

    send_all(client_fd, buffer, strlen(buffer));
}

/* EXEC whitelist */
int is_allowed_command(const char *command)
{
    if (strcmp(command, "date") == 0)
        return 1;

    if (strcmp(command, "whoami") == 0)
        return 1;

    return 0;
}

void execute_command(int client_fd, const char *command)
{
    char buffer[BUFFER_SIZE];

    if (!is_allowed_command(command))
    {
        send_all(client_fd, "EXEC DENIED", 11);
        return;
    }

    FILE *fp = popen(command, "r");

    if (fp == NULL)
    {
        send_all(client_fd, "EXEC ERROR", 10);
        return;
    }

    size_t total = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[total] = '\0';

    pclose(fp);

    send_all(client_fd, buffer, strlen(buffer));
}

/* Receive PUT file */
void receive_file(int client_fd, const char *filename, long filesize)
{
    char path[BUFFER_SIZE];

    snprintf(path, sizeof(path),
             "%s%s", STORAGE_PATH, filename);

    FILE *file = fopen(path, "wb");

    if (file == NULL)
    {
        send_all(client_fd, "PUT ERROR", 9);
        return;
    }

    char buffer[BUFFER_SIZE];
    long total_received = 0;

    while (total_received < filesize)
    {
        long remaining = filesize - total_received;

        int chunk_size =
            remaining < BUFFER_SIZE ? remaining : BUFFER_SIZE;

        ssize_t bytes = recv(client_fd, buffer, chunk_size, 0);

        if (bytes <= 0)
        {
            fclose(file);
            return;
        }

        fwrite(buffer, 1, bytes, file);

        total_received += bytes;
    }

    fclose(file);

    send_all(client_fd, "PUT OK", 6);
}

/* Send GET file */
void send_file(int client_fd, const char *filename)
{
    char path[BUFFER_SIZE];

    snprintf(path, sizeof(path),
             "%s%s", STORAGE_PATH, filename);

    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        send_all(client_fd, "FILE ERROR", 10);
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    char header[BUFFER_SIZE];

    snprintf(header, sizeof(header),
             "FILESIZE %ld", filesize);

    send_all(client_fd, header, strlen(header));

    char ready[BUFFER_SIZE] = {0};

    recv(client_fd, ready, sizeof(ready) - 1, 0);

    if (strncmp(ready, "READY", 5) != 0)
    {
        fclose(file);
        return;
    }

    char buffer[BUFFER_SIZE];

    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        send_all(client_fd, buffer, bytes_read);
    }

    fclose(file);
}

/* UDP Monitoring */
void udp_monitor(void)
{
    int udp_fd;

    struct sockaddr_in udp_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    char buffer[BUFFER_SIZE];

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_fd < 0)
    {
        perror("[AGENT] UDP socket");
        return;
    }

    memset(&udp_addr, 0, sizeof(udp_addr));

    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);

    if (bind(udp_fd,
             (struct sockaddr *)&udp_addr,
             sizeof(udp_addr)) < 0)
    {
        perror("[AGENT] UDP bind");
        close(udp_fd);
        return;
    }

    printf("[AGENT] UDP monitoring listening on port %d...\n",
           UDP_PORT);

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        ssize_t bytes = recvfrom(
            udp_fd,
            buffer,
            sizeof(buffer) - 1,
            0,
            (struct sockaddr *)&client_addr,
            &client_len);

        if (bytes <= 0)
            continue;

        buffer[bytes] = '\0';

        if (strcmp(buffer, "MONITOR") == 0)
        {
            char hostname[256] = "Unknown";

            gethostname(hostname, sizeof(hostname) - 1);

            char response[BUFFER_SIZE];

            snprintf(
                response,
                sizeof(response),
                "MONITOR OK\n"
                "Hostname: %s\n"
                "Agent TCP Port: %d\n"
                "Agent UDP Port: %d\n"
                "Status: Running",
                hostname,
                TCP_PORT,
                UDP_PORT);

            sendto(
                udp_fd,
                response,
                strlen(response),
                0,
                (struct sockaddr *)&client_addr,
                client_len);
        }
        else
        {
            const char *response = "UNKNOWN UDP COMMAND";

            sendto(
                udp_fd,
                response,
                strlen(response),
                0,
                (struct sockaddr *)&client_addr,
                client_len);
        }
    }

    close(udp_fd);
}

int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    char buffer[BUFFER_SIZE];

    mkdir("agentfiles", 0755);
    mkdir(STORAGE_PATH, 0755);

    /* ---------------- TCP SOCKET ---------------- */

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(TCP_PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[AGENT] TCP socket created.\n");
    printf("[AGENT] Bound to TCP port %d.\n", TCP_PORT);
    printf("[AGENT] Waiting for Controller...\n");

    /*
     * Start UDP monitoring in a child process.
     * TCP and UDP can run at the same time.
     */
    pid_t pid = fork();

    if (pid == 0)
    {
        close(server_fd);
        udp_monitor();
        exit(0);
    }

    /* ---------------- TCP LOOP ---------------- */

    while (1)
    {
        client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));

        int bytes_received =
            recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0)
        {
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';

        printf("[AGENT] Received: %s\n", buffer);

        /* ---------------- AUTH ---------------- */

        if (strncmp(buffer, "AUTH ", 5) == 0)
        {
            char expected[BUFFER_SIZE];

            snprintf(expected,
                     sizeof(expected),
                     "AUTH %s %s",
                     SESSION_ID,
                     AUTH_TOKEN);

            if (strcmp(buffer, expected) == 0)
            {
                send_all(client_fd, "AUTH OK", 7);
            }
            else
            {
                send_all(client_fd, "AUTH FAILED", 11);
                close(client_fd);
                continue;
            }
        }

        /* ---------------- COMMAND ---------------- */

        memset(buffer, 0, sizeof(buffer));

        int command_bytes =
            recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (command_bytes <= 0)
        {
            close(client_fd);
            continue;
        }

        buffer[command_bytes] = '\0';

        if (strcmp(buffer, "SYSINFO") == 0)
        {
            send_sysinfo(client_fd);
        }
        else if (strcmp(buffer, "LISTPROC") == 0)
        {
            send_listproc(client_fd);
        }
        else if (strncmp(buffer, "EXEC ", 5) == 0)
        {
            execute_command(client_fd, buffer + 5);
        }
        else if (strncmp(buffer, "PUT ", 4) == 0)
        {
            char filename[256];
            long filesize;

            if (sscanf(buffer + 4,
                       "%255s %ld",
                       filename,
                       &filesize) == 2)
            {
                receive_file(client_fd,
                             filename,
                             filesize);
            }
            else
            {
                send_all(client_fd, "PUT INVALID", 11);
            }
        }
        else if (strncmp(buffer, "GET ", 4) == 0)
        {
            send_file(client_fd, buffer + 4);
        }
        else
        {
            send_all(client_fd,
                     "UNKNOWN COMMAND",
                     15);
        }

        close(client_fd);

        printf("[AGENT] Controller connection closed.\n");
    }

    close(server_fd);

    return 0;
}
