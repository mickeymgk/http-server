#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

void *handle_connection(void *arg) {
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        printf("Error receiving data: %s\n", strerror(errno));
        close(client_socket);
        pthread_exit(NULL);
    }

    char *buffer_copy = strdup(buffer);
    char *path = strtok(buffer, " ");
    path = strtok(NULL, " ");

    if (strcmp(path, "/user-agent") == 0) {
        char *user_agent = strstr(buffer_copy, "User-Agent: ");
        if (user_agent != NULL) {
            user_agent += strlen("User-Agent: ");
            char *end_of_user_agent = strstr(user_agent, "\r\n");
            if (end_of_user_agent != NULL) {
                *end_of_user_agent = '\0';
                const char *response_format = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s";
                char response[BUFFER_SIZE];
                sprintf(response, response_format, strlen(user_agent), user_agent);
                send(client_socket, response, strlen(response), 0);
            }
        }
        free(buffer_copy); // why not free duplicates?
    } else if (strncmp(path, "/echo/", 6) == 0) {
        char *echo_string = path + 6;
        char response[1024];
        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", strlen(echo_string), echo_string);
        send(client_socket, response, strlen(response), 0);
    } else if (strcmp(path, "/") == 0) {
        const char *response = "HTTP/1.1 200 OK\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
    } else {
        const char *response = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
    }

    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    // Disable output buffering
    setbuf(stdout, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    printf("Logs from your program will appear here!\n");

    // Uncomment this block to pass the first stage
    //
    int server_fd, client_addr_len;
    struct sockaddr_in client_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    // Since the tester restarts your program quite often, setting REUSE_PORT
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEPORT failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = {.sin_family = AF_INET,
                                    .sin_port = htons(4221),
                                    .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    while (1) {
        int client_socket = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        printf("Client connected\n");

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_connection, (void *)&client_socket) != 0) {
            printf("Failed to create thread\n");
            close(client_socket);
        } else {
            pthread_detach(thread);
        }
    }

    close(server_fd);

    return 0;
}
