#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define WORD_FILE "words.txt"
#define MAX_WORD_LEN 100
#define BUFFER_SIZE 1024

char words[69000][MAX_WORD_LEN];
int word_count = 0;

void load_words() {
    FILE *file = fopen(WORD_FILE, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    
    while (fgets(words[word_count], MAX_WORD_LEN, file)) {
        strtok(words[word_count], "\n"); // remove new line
        word_count++;
    }
    fclose(file);
}

char *choose_word() {
    srand(time(NULL));
    int index = rand() % word_count;
    return words[index];
}

void *handle_client(void *socket_desc) {
    int new_socket = *(int *)socket_desc;
    free(socket_desc);

    char buffer[BUFFER_SIZE];
    int lives = 10;
    char *word = choose_word();
    int word_len = strlen(word);
    char guessed_word[MAX_WORD_LEN];
    memset(guessed_word, '-', word_len);
    guessed_word[word_len] = '\0';

    char guessed_letters[26] = {0};
    int guessed_count = 0;

    while (lives > 0) {
        sprintf(buffer, "%s\nYou have %d lives left.\nGuessed letters: %s\n", guessed_word, lives, guessed_letters);
        send(new_socket, buffer, strlen(buffer), 0);

        // Receive a guess from client
        int valread = read(new_socket, buffer, BUFFER_SIZE);
        buffer[valread] = '\0';
        char guess = tolower(buffer[0]);

        if (!isalpha(guess) || strlen(buffer) != 1) {
            strcpy(buffer, "Invalid input. Please guess a single letter.\n");
            send(new_socket, buffer, strlen(buffer), 0);
            continue;
        }

        // Check if letter already guessed
        if (strchr(guessed_letters, guess)) {
            strcpy(buffer, "Letter already guessed. Try again.\n");
            send(new_socket, buffer, strlen(buffer), 0);
            continue;
        }

        guessed_letters[guessed_count++] = guess;

        int correct_guess = 0;
        for (int i = 0; i < word_len; i++) {
            if (word[i] == guess) {
                guessed_word[i] = guess;
                correct_guess = 1;
            }
        }

        if (!correct_guess) {
            lives--;
        }

        // Check if word is completely guessed
        if (strcmp(guessed_word, word) == 0) {
            sprintf(buffer, "Congratulations! The correct word was '%s'.\nWant to play again? (y/n)\n", word);
            send(new_socket, buffer, strlen(buffer), 0);
            break;
        }
    }

    if (lives == 0) {
        sprintf(buffer, "Sorry, you've used up all your lives. The correct word was '%s'.\n", word);
        send(new_socket, buffer, strlen(buffer), 0);
    }

    close(new_socket);
    return 0;
}

int main() {
    load_words();

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        pthread_t thread_id;
        int *new_sock = malloc(1);
        *new_sock = new_socket;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread_id); // Detach to handle multiple clients
    }

    return 0;
}
