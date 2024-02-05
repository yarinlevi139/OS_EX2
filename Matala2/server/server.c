#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#define MAX_BUFFER_SIZE 2048
#define MAX_PATH_SIZE 256

void send_response(int client_socket, char *response) {
    send(client_socket, response, strlen(response), 0);
}

void handle_client(int client_socket, char *root_dir) {
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_received;

    usleep(70000);

    // Receive the request from the client
    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received < 0) {
        perror("Error receiving request");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("buffer contains <<<<%.*s>>>>\n", (int)bytes_received, buffer);


    // Tokenize the request based on "\r\n"
    char *token;
    char *method, *path;
    token = strtok(buffer, "\r\n");


    // Extract method and path
    if (token != NULL) {
        sscanf(token, "%ms %ms", &method, &path);
    } else {
        // Handle invalid request
        send_response(client_socket, "400 Bad Request\r\n\r\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    char full_path[MAX_PATH_SIZE];
    snprintf(full_path, sizeof(full_path), "%s%s", root_dir, path);


    if (strcmp(method, "GET") == 0) {
        // Handle GET request
        FILE *file = fopen(full_path, "rb");
        if (file == NULL) {
            // File not found
            send_response(client_socket, "404 Not Found\r\n\r\n");
        } else {
            // Read and send the file content
            send_response(client_socket, "200 OK\r\n");

            char file_buffer[MAX_BUFFER_SIZE];
            size_t bytes_read;

            while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
                send(client_socket, file_buffer, bytes_read, 0);
                printf("sending %s",file_buffer);
            }
            send_response(client_socket, "\r\n\r\n");

            fclose(file);
        }
    } else if (strcmp(method, "POST") == 0) {
        // Attempt to lock the file for writing
        struct flock lock;
        lock.l_type = F_WRLCK;  // Exclusive write lock
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;

        int file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file_fd < 0) {
            perror("Error opening file");
            send_response(client_socket, "500 INTERNAL ERROR\r\n\r\n");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        // Attempt to acquire the lock
        if (fcntl(file_fd, F_SETLK, &lock) == -1) {
            perror("Error acquiring lock");
            close(file_fd);
            send_response(client_socket, "500 INTERNAL ERROR\r\n\r\n");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        // Continue tokenizing and writing to file
        while (1) {
            token = strtok(NULL, "\r\n");
            if(token == NULL)
                break;
            printf("token contains <<<<%s>>>>\n", token);

            if (write(file_fd, token, strlen(token)) < 0) {
                perror("Error writing to file");
                close(file_fd);
                close(client_socket);
                exit(EXIT_FAILURE);
            }
        }

        printf("Finished writing to file\n");
        fflush(stdout);

        // Release the lock
        lock.l_type = F_UNLCK;
        if (fcntl(file_fd, F_SETLK, &lock) == -1) {
            perror("Error releasing lock");
            close(file_fd);
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        close(file_fd);

        // Send response to the client
        send_response(client_socket, "200 OK\r\n\r\n");
    }

    // Cleanup
    free(method);
    free(path);
    close(client_socket);
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <root_directory>\n", argv[0]);
        return 1;
    }

    char *root_dir = argv[1];

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error binding socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Error listening for connections");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Listening...\n");
    while (1) {
        fflush(stdout);
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            perror("Error accepting connection");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_socket);  // Close the server socket in the child process
            handle_client(client_socket, root_dir);
            exit(EXIT_SUCCESS);
        } else if (pid > 0) {
            // Parent process
            close(client_socket);
        } else {
            perror("Fork failed");
            close(client_socket);
            close(server_socket);
            exit(EXIT_FAILURE);
        }
    }

    close(server_socket);

    return 0;
}
