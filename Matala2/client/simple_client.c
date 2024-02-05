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

char *base64_encode(const char *input, size_t length) {
    BIO *bio, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);

    BIO_get_mem_ptr(bio, &bptr);

    char *output = (char *)malloc(bptr->length);
    memcpy(output, bptr->data, bptr->length - 1);
    output[bptr->length - 1] = '\0';

    BIO_free_all(bio);

    return output;
}

// Function to decode base64 string and return the result
char* base64_decode(const char *encoded) {
    // Define the base64 characters
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Create a reverse lookup table for base64 characters
    int reverse_table[256];
    for (int i = 0; i < 64; ++i) {
        reverse_table[(int)base64_chars[i]] = i;
    }

    // Calculate the length of the decoded string
    size_t encoded_len = strlen(encoded);
    size_t decoded_len = (encoded_len / 4) * 3;

    // Check for padding at the end
    if (encoded[encoded_len - 1] == '=') {
        decoded_len--;
        if (encoded[encoded_len - 2] == '=') {
            decoded_len--;
        }
    }

    // Allocate memory for the decoded string
    char *decoded = (char *)malloc(decoded_len + 1);

    // Decode the base64 string
    uint32_t buffer = 0;
    int bits = 0;
    size_t j = 0;
    for (size_t i = 0; i < encoded_len; ++i) {
        char c = encoded[i];
        if (c == '=') {
            break;
        }
        buffer = (buffer << 6) | reverse_table[(int)c];
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded[j++] = (char)((buffer >> bits) & 0xFF);
        }
    }

    // Null-terminate the decoded string
    decoded[j] = '\0';

    // Return the decoded string
    return decoded;
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
            char *encoded_data = base64_encode(buffer, bytes_read);
            send_request(client_socket, encoded_data, strlen(encoded_data));
            free(encoded_data);
        }

        send_request(client_socket, "\r\n\r\n", sizeof("\r\n\r\n"));
        fclose(file);

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

        usleep(1000); // wait to get the server full response

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
            size_t len = strlen(token);
            char *decoded = base64_decode(token);

            FILE *file = fopen(file_path, "wb");
            fwrite(decoded, 1, strlen(decoded), file);
            fclose(file);

            printf("Decoded data: %.*s\n", (int)len, decoded);
            free(decoded);  // Free the allocated memory for decoded data
        }
    }

    return 0;
}
