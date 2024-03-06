#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#define MAX_BUFFER_SIZE 10000

void send_request(int server_socket, const char *request, size_t request_size) {
    send(server_socket, request, request_size, 0);
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

void download_file(char* file_path,int client_socket) {

    // Receive and print the server's response
    char response[MAX_BUFFER_SIZE];
    bzero(response,MAX_BUFFER_SIZE);
    ssize_t bytes_received = recv(client_socket, response, sizeof(response), 0);
    if (bytes_received == 0)
        return;
    response[bytes_received] = '\0';
    printf("\n bytes recieved %ld\n",bytes_received);
    if (bytes_received < 0) {
        perror("Error receiving response from server");
    } else {
        printf("<<<%s>>>\n",response);
        char *token = strtok(response, "\r\n");
        if(strcmp(token,"404 Not Found") == 0)
        {
            printf("Server File Error");
            exit(EXIT_FAILURE);
        }

        token = strtok(NULL, "\r\n");
        size_t len = strlen(token);
        char *decoded;
        int bytes_decoded = base64_decode(token, len, &decoded);

        char *subpath = malloc(strlen("files/") + strlen(file_path) + 1);
        strcpy(subpath, "files/");
        strcat(subpath, file_path);

        FILE *file = fopen(subpath, "wb");
        fwrite(decoded, 1, bytes_decoded, file);
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

        char request[MAX_BUFFER_SIZE];
        snprintf(request, sizeof(request), "GET /downloads/%s \r\n\r\n", item_to_download);
        send_request(client_socket, request, strlen(request));

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

        usleep(1100000); // wait to get the server full response

        // Receive and print the server's response
        char response[MAX_BUFFER_SIZE];
        ssize_t bytes_received = recv(client_socket, response, sizeof(response), 0);
        if (bytes_received < 0) {
            perror("Error receiving response from server");
        } else {
            response[bytes_received] = '\0';
            close(client_socket); //closing the connection and starting to open multiple connections

            int counter = 0;
            char *temp = strtok(response, "\r\n");

            // Initialize the filenames array
            char **filenames = NULL;

            while (temp != NULL) {
                temp = strtok(NULL, "\r\n");
                if (temp != NULL) {
                    // Increase the size of the array by one
                    filenames = (char **)realloc(filenames, (counter + 1) * sizeof(char *));
                    if (filenames == NULL) {
                        // Handle memory allocation failure
                        fprintf(stderr, "Memory allocation failed\n");
                        exit(EXIT_FAILURE);
                    }

                    // Allocate memory for the token and copy it to the filenames array
                    filenames[counter] = strdup(temp);
                    if (filenames[counter] == NULL) {
                        // Handle memory allocation failure
                        fprintf(stderr, "Memory allocation failed\n");
                        exit(EXIT_FAILURE);
                    }

                    counter++;
                }
            }


            int *sockets = (int *) malloc(sizeof(int) * counter);
            if (sockets == NULL)
            {
                perror("Allocation error");
                exit(1);
            }

            struct sockaddr_in server_addr;

            // Create sockets and connect to the server
            for (int i = 0; i < counter; ++i) {
                sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
                if (sockets[i] < 0) {
                    perror("socket");
                    exit(EXIT_FAILURE);
                }

                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_addr.s_addr = inet_addr(server_address);
                server_addr.sin_port = htons(atoi(server_port));

                if (connect(sockets[i], (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
                    perror("connect");
                    exit(EXIT_FAILURE);
                }
            }

            for (int i = 0; i < counter; ++i) {
                char request[MAX_BUFFER_SIZE];
                snprintf(request, sizeof(request), "GET /downloads/%s \r\n\r\n", filenames[i]);
                send_request(sockets[i], request, strlen(request));
            }

            fd_set readfds;
            while(1) {

                FD_ZERO(&readfds);
                int max_fd = -1;

                // Add sockets to the set
                for (int i = 0; i < counter; ++i) {
                    if(sockets[i]!=0)
                        FD_SET(sockets[i], &readfds);
                    if (sockets[i] > max_fd)
                        max_fd = sockets[i];
                }

                struct timeval timeout;
                timeout.tv_sec = 3; // 3 seconds timeout
                timeout.tv_usec = 0;

                // Use select to wait for activity on sockets
                int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
                if(activity == 0)
                {
                    printf("Breaking");
                    break;
                }
                if (activity < 0) {
                    perror("select");
                    exit(EXIT_FAILURE);
                }


                for (int i = 0; i < counter; ++i) {
                    if (FD_ISSET(sockets[i], &readfds)) {
                        if (filenames[i] != NULL) {
                            download_file(filenames[i], sockets[i]);
                        }
                        close(sockets[i]);
                        sockets[i] = 0;
                    }
                }
            }

            free(sockets);
            free(file_path);
            // Free individual strings and then the array
            for (int i = 0; i < counter; i++) {
                free(filenames[i]); // Free memory for each string
            }
            free(filenames); // Free memory for the array itself


        }
        // Don't forget to free the allocated memory when you're done with it
    }
    return 0;
}