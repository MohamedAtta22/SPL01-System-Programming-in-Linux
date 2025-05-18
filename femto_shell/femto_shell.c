#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

#define SIZE 30000
int femtoshell_main(int argc, char *argv[]) {
int status = 0;
    char buf[SIZE];
    while (1) {
        printf("welcome to my shell, enter your command!\n>  ");
        if (fgets(buf, SIZE, stdin) == NULL) {
            exit(status);
        }
        if (strcmp(buf, "\n") == 0) continue;
        if (buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = 0;
        char *token = strtok(buf, " ");
        if (token == NULL) continue;
        if (strcmp(token, "exit") == 0) {
            printf("Good Bye\n");
            exit(status);
        } else if (strcmp(token, "echo") == 0) {
            token = strtok(NULL, "");
            if (token) printf("%s\n", token);
            else printf("\n");
        } else {
            printf("Invalid command\n");
            status = 1;
        }
    }
}