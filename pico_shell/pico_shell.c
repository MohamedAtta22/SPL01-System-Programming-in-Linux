#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
int status = 0;
    char *line = NULL;
    size_t len = 0;

    while (1) {
        printf("PicoShell > ");
        fflush(stdout);
        if (getline(&line, &len, stdin) == -1) {
            break;
        }

        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
        }

        if (line[0] == '\0') continue;

        int cap = 10;
        int count = 0;
        char **args = (char **)malloc(cap * sizeof(char *));
        char *token = strtok(line, " ");
        while (token != NULL) {
            if (count >= cap) {
                cap *= 2;
                args = (char **)realloc(args, cap * sizeof(char *));
            }
            args[count++] = strdup(token);
            token = strtok(NULL, " ");
        }
        args[count] = NULL;

        if (strcmp(args[0], "exit") == 0) {
            printf("Good Bye\n");
            for (int i = 0; i < count; i++) free(args[i]);
            free(args);
            break;
        } else if (strcmp(args[0], "echo") == 0) {
            for (int i = 1; i < count; i++) {
                printf("%s", args[i]);
                if (i < count - 1) printf(" ");
            }
            printf("\n");
            status = 0;
        } else if (strcmp(args[0], "pwd") == 0) {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
                status = 0;
            } else {
                perror("pwd");
                status = 1;
            }
        } else if (strcmp(args[0], "cd") == 0) {
            const char *path = count > 1 ? args[1] : getenv("HOME");
            if (chdir(path) != 0) {
                fprintf(stderr, "cd: %s: No such file or directory\n", path);
                status = 1;
            } else {
                status = 0;
            }
        } else {
            pid_t pid = fork();
            if (pid == 0) {
                execvp(args[0], args);
                fprintf(stderr, "%s: command not found\n", args[0]);
                exit(127);
            } else if (pid > 0) {
                int wstatus;
                waitpid(pid, &wstatus, 0);
                status = WEXITSTATUS(wstatus);
            } else {
                perror("fork");
                status = 1;
            }
        }

        for (int i = 0; i < count; i++) free(args[i]);
        free(args);
    }

    free(line);
    return status;
}