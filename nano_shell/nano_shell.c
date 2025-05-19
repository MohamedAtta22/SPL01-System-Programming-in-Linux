#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define MAX_VARS 100
#define MAX_VAR_LEN 256

typedef struct {
    char name[MAX_VAR_LEN];
    char value[MAX_VAR_LEN];
} Variable;

Variable local_vars[MAX_VARS];
int var_count = 0;

Variable* find_variable(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) {
            return &local_vars[i];
        }
    }
    return NULL;
}

int set_variable(const char* name, const char* value) {
    if (var_count >= MAX_VARS) {
        fprintf(stderr, "Error: Maximum number of variables reached\n");
        return 0;
    }
    
    Variable* var = find_variable(name);
    if (var) {
        strncpy(var->value, value, MAX_VAR_LEN - 1);
        var->value[MAX_VAR_LEN - 1] = '\0';
    } else {
        strncpy(local_vars[var_count].name, name, MAX_VAR_LEN - 1);
        local_vars[var_count].name[MAX_VAR_LEN - 1] = '\0';
        strncpy(local_vars[var_count].value, value, MAX_VAR_LEN - 1);
        local_vars[var_count].value[MAX_VAR_LEN - 1] = '\0';
        var_count++;
    }
    return 1;
}

char* substitute_variables(const char* input) {
    if (!input) return NULL;
    char* result = strdup(input);
    char* start = result;
    while ((start = strchr(start, '$')) != NULL) {
        char* end = start + 1;
        while (*end && (isalnum(*end) || *end == '_')) end++;
        int var_len = end - start - 1;
        if (var_len > 0) {
            char var_name[MAX_VAR_LEN];
            strncpy(var_name, start + 1, var_len);
            var_name[var_len] = '\0';
            Variable* var = find_variable(var_name);
            if (var) {
                int new_len = strlen(result) - var_len - 1 + strlen(var->value);
                char* new_result = (char*)malloc(new_len + 1);
                int pos = start - result;
                strncpy(new_result, result, pos);
                strcpy(new_result + pos, var->value);
                strcpy(new_result + pos + strlen(var->value), end);
                free(result);
                result = new_result;
                start = result + pos + strlen(var->value);
            } else {
                int new_len = strlen(result) - var_len - 1;
                char* new_result = (char*)malloc(new_len + 1);
                int pos = start - result;
                strncpy(new_result, result, pos);
                new_result[pos] = '\0';
                strcat(new_result, end);
                free(result);
                result = new_result;
                start = result + pos;
            }
        } else {
            start++;
        }
    }
    return result;
}

int handle_assignment(const char* line) {
    const char* equal_sign = strchr(line, '=');
    if (!equal_sign) return 0;
    
    const char* space = strchr(line, ' ');
    if (space && space < equal_sign) return 0;
    
    int name_len = equal_sign - line;
    if (name_len == 0) return 0;
    
    char name[MAX_VAR_LEN];
    strncpy(name, line, name_len);
    name[name_len] = '\0';
    
    const char* value = equal_sign + 1;
    if (*value == '\0') return 0;
    
    return set_variable(name, value);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    int status = 0;
    char *line = NULL;
    size_t len = 0;

    while (1) {
        printf("Nano Shell Prompt > ");
        fflush(stdout);
        if (getline(&line, &len, stdin) == -1) {
            break;
        }

        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
        }

        if (line[0] == '\0') continue;
        
        if (strchr(line, '=')) {
            if (!handle_assignment(line)) {
                printf("Invalid command\n");
            }
            continue;
        }

        int cap = 10;
        int count = 0;
        char **args = (char **)malloc(cap * sizeof(char *));
        char *token = strtok(line, " ");
        while (token != NULL) {
            if (count >= cap) {
                cap *= 2;
                args = (char **)realloc(args, cap * sizeof(char *));
            }
            
            char* substituted = substitute_variables(token);
            args[count++] = substituted ? substituted : strdup(token);
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
        } else if (strcmp(args[0], "export") == 0) {
            if (count != 2) {
                fprintf(stderr, "export: usage: export VARIABLE\n");
                status = 1;
            } else {
                Variable* var = find_variable(args[1]);
                if (var) {
                    if (setenv(var->name, var->value, 1) != 0) {
                        perror("export");
                        status = 1;
                    } else {
                        status = 0;
                    }
                } else {
                    fprintf(stderr, "export: %s: variable not found\n", args[1]);
                    status = 1;
                }
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
