#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#define MAX_BUFFER_SIZE 2048

void send_request(int server_socket, const char *request, size_t request_size) {
    send(server_socket, request, request_size, 0);
}

char *base64_decode(const char *encoded) {
    BIO *bio, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, encoded, strlen(encoded));
    BIO_flush(bio);

    BIO_get_mem_ptr(bio, &bptr);

    char *decoded = (char *)malloc(bptr->length + 1);
    memcpy(decoded, bptr->data, bptr->length);
    decoded[bptr->length] = '\0';

    BIO_free_all(bio);

    return decoded;
}

void download_file(const char *file_path, int client_socket) {
    FILE *file = fopen(file_path, "wb");
    if (file == NULL) {
        perror("Error opening file");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    char response[MAX_BUFFER_SIZE];
    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, response, sizeof(response), 0)) > 0) {
        fwrite(response, 1, bytes_received, file);
    }

    fclose(file);
    close(client_socket);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("<ENCODED_LIST_FILE_PATH>");
        exit(EXIT_FAILURE);
    }

    const char *server_address = "127.0.0.1";
    const char *server_port = "8080";
    const char *encoded_list_file_path = argv[1];

    FILE *encoded_list_file = fopen(encoded_list_file_path, "r");
    if (encoded_list_file == NULL) {
        perror("Error opening encoded list file");
        exit(EXIT_FAILURE);
    }

    char encoded_line[MAX_BUFFER_SIZE];
    while (fgets(encoded_line, sizeof(encoded_line), encoded_list_file) != NULL) {
        size_t len = strlen(encoded_line);
        if (len > 0 && encoded_line[len - 1] == '\n') {
            encoded_line[len - 1] = '\0'; // Remove the newline character
        }

        char *decoded_line = base64_decode(encoded_line);

        pid_t pid = fork();

        if (pid < 0) {
            perror("Error forking process");
            free(decoded_line);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            int client_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (client_socket < 0) {
                perror("Error creating socket");
                free(decoded_line);
                exit(EXIT_FAILURE);
            }

            struct hostent *server = gethostbyname(server_address);
            if (server == NULL) {
                fprintf(stderr, "Error: No such host\n");
                close(client_socket);
                free(decoded_line);
                exit(EXIT_FAILURE);
            }

            struct sockaddr_in server_address_info;
            memset(&server_address_info, 0, sizeof(server_address_info));
            server_address_info.sin_family = AF_INET;
            bcopy((char *)server->h_addr, (char *)&server_address_info.sin_addr.s_addr, server->h_length);
            server_address_info.sin_port = htons(atoi(server_port));

            if (connect(client_socket, (struct sockaddr *)&server_address_info, sizeof(server_address_info)) < 0) {
                perror("Error connecting to server");
                close(client_socket);
                free(decoded_line);
                exit(EXIT_FAILURE);
            }

            char request[MAX_BUFFER_SIZE];
            snprintf(request, sizeof(request), "GET /downloads/%s\r\n\r\n", decoded_line);
            send_request(client_socket, request, strlen(request));

            download_file(decoded_line, client_socket);

            free(decoded_line);
            close(client_socket);
            exit(EXIT_SUCCESS);
        }

        free(decoded_line);
    }

    fclose(encoded_list_file);

    int status;
    while (wait(&status) > 0);

    return 0;
}
