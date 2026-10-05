#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define TCP_PORT 9410
#define UDP_PORT 9411

#define BUFFER_SIZE 4096

#define SESSION_ID "SID:7392"
#define AUTH_TOKEN "OPS-2937"

/* ---------------- TCP AUTH TEST ---------------- */

void tcp_auth_test(void)
{
    int sockfd;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("TCP socket");
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("TCP connect");
        close(sockfd);
        return;
    }

    char auth_message[BUFFER_SIZE];

    snprintf(auth_message,
             sizeof(auth_message),
             "AUTH %s %s",
             SESSION_ID,
             AUTH_TOKEN);

    send(sockfd,
         auth_message,
         strlen(auth_message),
         0);

    memset(buffer, 0, sizeof(buffer));

    int bytes =
        recv(sockfd,
             buffer,
             sizeof(buffer) - 1,
             0);

    if (bytes > 0)
    {
        buffer[bytes] = '\0';

        printf("[CONTROLLER] TCP AUTH response: %s\n",
               buffer);
    }

    close(sockfd);
}

/* ---------------- UDP MONITOR ---------------- */

void udp_monitor_test(void)
{
    int udp_fd;

    struct sockaddr_in agent_addr;

    char buffer[BUFFER_SIZE];

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_fd < 0)
    {
        perror("UDP socket");
        return;
    }

    memset(&agent_addr, 0, sizeof(agent_addr));

    agent_addr.sin_family = AF_INET;
    agent_addr.sin_port = htons(UDP_PORT);
    agent_addr.sin_addr.s_addr =
        inet_addr("127.0.0.1");

    const char *message = "MONITOR";

    sendto(
        udp_fd,
        message,
        strlen(message),
        0,
        (struct sockaddr *)&agent_addr,
        sizeof(agent_addr));

    printf("[CONTROLLER] UDP MONITOR request sent.\n");

    memset(buffer, 0, sizeof(buffer));

    socklen_t agent_len =
        sizeof(agent_addr);

    int bytes =
        recvfrom(
            udp_fd,
            buffer,
            sizeof(buffer) - 1,
            0,
            (struct sockaddr *)&agent_addr,
            &agent_len);

    if (bytes > 0)
    {
        buffer[bytes] = '\0';

        printf("\n[CONTROLLER] UDP Monitoring Response:\n");
        printf("--------------------------------------\n");
        printf("%s\n", buffer);
        printf("--------------------------------------\n");
    }
    else
    {
        printf("[CONTROLLER] No UDP response received.\n");
    }

    close(udp_fd);
}

int main(void)
{
    printf("========================================\n");
    printf(" RemoteOps Controller - UDP Monitoring\n");
    printf("========================================\n\n");

    /* Test TCP authentication */
    tcp_auth_test();

    /* Test UDP monitoring */
    udp_monitor_test();

    return 0;
}
