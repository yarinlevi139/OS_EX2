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

#define MAX_BUFFER_SIZE 10000

void send_request(int server_socket, const char *request, size_t request_size) {
    send(server_socket, request, request_size, 0);
}

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

int base64_encode(const char *input, size_t length, char **output) {
    BIO *bio, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);

    BIO_get_mem_ptr(bio, &bptr);

    // Allocate enough memory for the encoded data plus padding
    *output = (char *)malloc(bptr->length + 1); // Add 1 for the null terminator
    if (*output == NULL) {
        BIO_free_all(bio);
        return -1; // Memory allocation failed
    }

    memcpy(*output, bptr->data, bptr->length);
    (*output)[bptr->length] = '\0'; // Null-terminate the string

    int bytes_encoded = bptr->length;
    BIO_free_all(bio);

    return bytes_encoded;
}

int base64_decode(const char *input, size_t length, char **output) {
    BIO *bio, *b64;
    // Create a BIO filter for base64 decoding
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(input, length);
    bio = BIO_push(b64, bio);

    // Disable line breaks
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    // Calculate the size of the decoded data
    size_t max_decoded_size = (length * 3) / 4 + 1; // Maximum size of decoded data without padding
    *output = (char *)malloc(max_decoded_size);

    // Decode the base64 data
    int bytes_decoded = BIO_read(bio, *output, length); // Use original input length

    (*output)[bytes_decoded] = '\0'; // Null-terminate the output string

    BIO_free_all(bio);

    return bytes_decoded;
}
int main(int argc,char *argv[]) {
    if (argc != 4)
    {
        printf("<PATH> <METHOD> <ITEM_FROM_SERVER>");
        exit(EXIT_FAILURE);
    }

    const char *server_address = "127.0.0.1";
    const char *server_port = "8080";
    const char *file_path = argv[1];
    const char *item_to_downlaod = argv[3];

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    struct hostent *server = gethostbyname(server_address);
    if (server == NULL) {
        fprintf(stderr, "Error: No such host\n");
        close(client_socket);
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
        exit(EXIT_FAILURE);
    }
    char* method = argv[2];
    if(strcmp("P",method) == 0) { //POST
        FILE *file = fopen(file_path, "rb");
        if (file == NULL) {
            perror("Error opening file");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        char request[MAX_BUFFER_SIZE];
        snprintf(request, sizeof(request), "POST %s \r\n",item_to_downlaod);
        send_request(client_socket, request, strlen(request));

        char buffer[MAX_BUFFER_SIZE];
        size_t bytes_read;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            char *encoded_data;
            int encoded_length = base64_encode(buffer, bytes_read, &encoded_data);
            send_request(client_socket, encoded_data, encoded_length);
            free(encoded_data);
        }

        send_request(client_socket, "\r\n\r\n", sizeof("\r\n\r\n"));
        fclose(file);

        usleep(1000000); // wait to get the server full response

        // Receive and print the server's response
        char response[MAX_BUFFER_SIZE];
        ssize_t bytes_received = recv(client_socket, response, sizeof(response), 0);
        if (bytes_received < 0) {
            perror("Error receiving response from server");
        } else {
            response[bytes_received] = '\0';
            printf("Server response:\n%s", response);
        }

        close(client_socket);
    }
    else if (strcmp("G", method) == 0) { // GET
        char request[MAX_BUFFER_SIZE];
        snprintf(request, sizeof(request), "GET %s \r\n\r\n",item_to_downlaod);
        send_request(client_socket, request, strlen(request));

        usleep(1000000); // wait to get the server full response

        // Receive and print the server's response
        char response[MAX_BUFFER_SIZE];
        ssize_t bytes_received = recv(client_socket, response, sizeof(response), 0);
        if (bytes_received < 0) {
            perror("Error receiving response from server");
        } else {
            response[bytes_received] = '\0';
            char *token = strtok(response, "\r\n");
            token = strtok(NULL, "\r\n");

            printf("Token: %s\n", token);

            if (token != NULL) {
                size_t len = strlen(token);
                char *decoded;
                int bytes_decoded = base64_decode(token, len, &decoded);

                if (bytes_decoded > 0) {
                    FILE *file = fopen(file_path, "wb");
                    fwrite(decoded, 1, bytes_decoded, file);
                    fclose(file);

                    printf("Decoded data: %.*s\n", (int)bytes_decoded, decoded);

                    free(decoded);  // Free the allocated memory for decoded data
                } else {
                    fprintf(stderr, "Base64 decoding failed.\n");
                }
            } else {
                fprintf(stderr, "Token not found in response.\n");
            }
        }
    }

    return 0;
}
