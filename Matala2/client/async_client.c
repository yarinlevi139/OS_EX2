#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#define MAX_BUFFER_SIZE 2048

void send_request(int server_socket, const char *request, size_t request_size) {
    send(server_socket, request, request_size, 0);
}

//Function to decode base64 string and return the result
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


void download_file(const char *file_path, int client_socket) {
    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "GET /downloads/%s \r\n\r\n", file_path);
    send_request(client_socket, request, strlen(request));

    usleep(1000000); // wait to get the server full response

    // Receive and print the server's response
    char response[MAX_BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, response, sizeof(response), 0);

    if(strcmp(response,"404 Not Found\r\n\r\n") == 0)
    {
        printf("%s",response);
        exit(EXIT_FAILURE);
    }

    if (bytes_received < 0) {
        perror("Error receiving response from server");
    } else {
        printf("<<<%s>>>\n",response);
        char *token = strtok(response, "\r\n");
        token = strtok(NULL, "\r\n");

        size_t len = strlen(token);
        char *decoded = base64_decode(token);

        char *subpath = malloc(strlen("files/") + strlen(file_path) + 1);
        strcpy(subpath, "files/");
        strcat(subpath, file_path);

        FILE *file = fopen(subpath, "wb");
        fwrite(decoded, 1, strlen(decoded), file);
        fclose(file);

        free(decoded);  // Free the allocated memory for decoded data
        free(subpath);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("<ENCODED_LIST_FILE_PATH>");
        exit(EXIT_FAILURE);
    }

    const char *server_address = "127.0.0.1";
    const char *server_port = "8080";
    char *item_to_download = argv[1];


    if (strstr(item_to_download,".list") == NULL) //meaning its normal we open conenction normally
    {
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
        download_file(item_to_download,client_socket);
    }
    else { //we contain .list file
        //fetch the .list first:
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
        bcopy((char *) server->h_addr, (char *) &server_address_info.sin_addr.s_addr, server->h_length);
        server_address_info.sin_port = htons(atoi(server_port));

        if (connect(client_socket, (struct sockaddr *) &server_address_info, sizeof(server_address_info)) < 0) {
            perror("Error connecting to server");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        char *file_path = malloc(strlen("files/") + strlen(item_to_download) + 1);

        if (file_path == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }

        strcpy(file_path, "files/");
        strcat(file_path, item_to_download);


        char request[MAX_BUFFER_SIZE];
        snprintf(request, sizeof(request), "GET /downloads/%s \r\n\r\n", item_to_download);
        send_request(client_socket, request, strlen(request));

        usleep(1000000); // wait to get the server full response

        // Receive and print the server's response
        char response[MAX_BUFFER_SIZE];
        ssize_t bytes_received = recv(client_socket, response, sizeof(response), 0);
        if (bytes_received < 0) {
            perror("Error receiving response from server");
        } else {
            response[bytes_received] = '\0';
            printf("<<<<<<%s>>>>>>\n",response);

            close(client_socket); //closing the connection and starting to open multiple connections


            char *token = strtok(response, "\r\n");
            while (token != NULL) {
                token = strtok(NULL, "\r\n");

                pid_t pid = fork();

                if (pid == 0 && token!=NULL) //child process
                {
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
                    bcopy((char *) server->h_addr, (char *) &server_address_info.sin_addr.s_addr, server->h_length);
                    server_address_info.sin_port = htons(atoi(server_port));

                    if (connect(client_socket, (struct sockaddr *) &server_address_info, sizeof(server_address_info)) <
                        0) {
                        perror("Error connecting to server");
                        close(client_socket);
                        exit(EXIT_FAILURE);
                    }

                    download_file(token, client_socket);
                    close(client_socket);
                    exit(EXIT_SUCCESS);
                }

            }



            // Don't forget to free the allocated memory when you're done with it
            while (wait(NULL) > 0) {
                // Keep waiting until no more child processes are left
            }
            free(file_path);
        }



    }
    return 0;
}
