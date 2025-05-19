#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define INIT_ARGV_CAPACITY 32
#define INPUT_BUFFER_INIT_SIZE 256
#define STDIN_REDIRECTED 0x2
#define STDOUT_REDIRECTED 0x4
#define STDERR_REDIRECTED 0x8

typedef struct {
    int argc;
    char **argv;
    int argvCapacity;
} Commands_t;

typedef struct node {
    struct node *next;
    char *key;
    char *val;
} node_t;

typedef struct {
    node_t *root;
} list_t;

static list_t *LocalVariablesList = NULL;
static int SavedStdinFD;
static int SavedStdoutFD;
static int SavedStderrFD;
static int status = 0;

void Tokenizer_init(Commands_t *Command) {
    Command->argc = 0;
    Command->argv = (char **)calloc(INIT_ARGV_CAPACITY, sizeof(char *));
    Command->argvCapacity = INIT_ARGV_CAPACITY;
}

void Tokenizer_free_ArgvStrs(Commands_t *command) {
    if (!command) return;
    for (int i = 0; i < command->argc; ++i) {
        if (command->argv[i]) {
            free(command->argv[i]);
            command->argv[i] = NULL;
        }
    }
    command->argc = 0;
}

void Tokenizer_free_all(Commands_t *comm) {
    if (comm) {
        Tokenizer_free_ArgvStrs(comm);
        if (comm->argv) {
            free(comm->argv);
            comm->argv = NULL;
            comm->argvCapacity = 0;
        }
    }
}

int Tokenize_Line(char *Line, int size, Commands_t *out) {
    if (!Line || !out || size == 0) return -1;
    char *str = Line;
    char *temp = (char *)malloc(size + 1);
    int tempLen = 0;
    int i = 0;
    if (!temp) return -2;
    while (i < size) {
        tempLen = 0;
        while (i < size && (str[i] == ' ' || str[i] == '\t')) i++;
        while (i < size && str[i] != ' ' && str[i] != '\t') {
            temp[tempLen++] = str[i++];
        }
        if (tempLen > 0) {
            temp[tempLen] = '\0';
            if (out->argc == out->argvCapacity) {
                out->argvCapacity *= 2;
                out->argv = (char **)realloc(out->argv, out->argvCapacity * sizeof(char *));
                if (!out->argv) {
                    free(temp);
                    return -2;
                }
            }
            out->argv[out->argc] = strdup(temp);
            if (!out->argv[out->argc]) {
                free(temp);
                return -2;
            }
            out->argc++;
        }
    }
    if (out->argc == out->argvCapacity) {
        out->argvCapacity++;
        out->argv = (char **)realloc(out->argv, out->argvCapacity * sizeof(char *));
    }
    out->argv[out->argc] = NULL;
    free(temp);
    return 0;
}

node_t *CreateNode(char *key, char *val) {
    node_t *node = (node_t *)malloc(sizeof(node_t));
    if (node) {
        node->key = key ? strdup(key) : NULL;
        node->val = val ? strdup(val) : NULL;
        node->next = NULL;
    }
    return node;
}

list_t *list_create() {
    list_t *root = (list_t *)malloc(sizeof(list_t));
    root->root = NULL;
    return root;
}

void list_Append(list_t **list, char *key, char *val) {
    if (!list || !key) return;
    if ((*list)->root == NULL) {
        (*list)->root = CreateNode(key, val);
    } else {
        node_t *temp = (*list)->root;
        while (temp) {
            if (strcmp(temp->key, key) == 0) {
                free(temp->val);
                temp->val = strdup(val);
                break;
            } else if (!temp->next) {
                temp->next = CreateNode(key, val);
                break;
            }
            temp = temp->next;
        }
    }
}

void list_Delete(list_t **list, char *key) {
    if (!list || !key) return;
    node_t *temp = (*list)->root;
    node_t *prev = NULL;
    while (temp) {
        if (strcmp(temp->key, key) == 0) {
            free(temp->key);
            free(temp->val);
            if (!prev) {
                (*list)->root = temp->next;
            } else {
                prev->next = temp->next;
            }
            free(temp);
            break;
        }
        prev = temp;
        temp = temp->next;
    }
}

char *list_GetVal(list_t *list, char *key) {
    if (!list || !key) return NULL;
    node_t *temp = list->root;
    while (temp) {
        if (strcmp(temp->key, key) == 0) {
            return temp->val;
        }
        temp = temp->next;
    }
    return NULL;
}

void list_clear(list_t **list) {
    if (!*list) return;
    node_t *temp = (*list)->root;
    while (temp) {
        node_t *next = temp->next;
        free(temp->key);
        free(temp->val);
        free(temp);
        temp = next;
    }
    free(*list);
    *list = NULL;
}

bool LocalVariableAssignHandler(Commands_t *comm) {
    if (!comm || comm->argc != 1) return false;
    char *equal = strchr(comm->argv[0], '=');
    if (!equal) return false;
    *equal = '\0';
    char *key = comm->argv[0];
    char *value = equal + 1;
    list_Append(&LocalVariablesList, key, value);
    return true;
}

char *substitute_all_vars(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *result = (char *)malloc(len * 2 + 1); 
    size_t ri = 0;
    for (size_t i = 0; i < len;) {
        if (str[i] == '$') {
            size_t var_start = i + 1;
            size_t var_len = 0;
            while (str[var_start + var_len] && (isalnum((unsigned char)str[var_start + var_len]) || str[var_start + var_len] == '_'))
                var_len++;
            if (var_len > 0) {
                char varname[128];
                strncpy(varname, str + var_start, var_len);
                varname[var_len] = 0;
                char *val = list_GetVal(LocalVariablesList, varname);
                if (val) {
                    size_t vlen = strlen(val);
                    memcpy(result + ri, val, vlen);
                    ri += vlen;
                }
                i = var_start + var_len;
                continue;
            }
        }
        result[ri++] = str[i++];
    }
    result[ri] = 0;
    return result;
}

void LocalVariableReplace(Commands_t *comm) {
    if (!comm) return;
    for (int i = 0; i < comm->argc; ++i) {
        char *newval = substitute_all_vars(comm->argv[i]);
        free(comm->argv[i]);
        comm->argv[i] = newval;
    }
}

int IO_RedirectionHandler(Commands_t *comm) {
    int i = 0, newfd, ret = 0, redirect_start = -1;
    for (; i < comm->argc; ++i) {
        if (!comm->argv[i]) continue;
        if ((strcmp(comm->argv[i], "<") == 0 || strcmp(comm->argv[i], ">") == 0 || strcmp(comm->argv[i], "2>") == 0) && i + 1 < comm->argc) {
            if (redirect_start == -1) redirect_start = i;
            char *filename = substitute_all_vars(comm->argv[i + 1]);
            int flags = 0, fd = -1;
            mode_t mode = 0666;
            if (strcmp(comm->argv[i], "<") == 0) { flags = O_RDONLY; fd = STDIN_FILENO; }
            else if (strcmp(comm->argv[i], ">") == 0) { flags = O_WRONLY | O_CREAT | O_TRUNC; fd = STDOUT_FILENO; }
            else if (strcmp(comm->argv[i], "2>") == 0) { flags = O_WRONLY | O_CREAT | O_TRUNC; fd = STDERR_FILENO; }
            newfd = open(filename, flags, mode);
            if (newfd < 0) {
                if (strcmp(comm->argv[i], "<") == 0) {
                    fprintf(stderr, "cannot access %s: %s\n", filename, strerror(errno));
                } else {
                    fprintf(stderr, "%s: %s\n", filename, strerror(errno));
                }
                free(filename);
                return -1;
            }
            dup2(newfd, fd);
            close(newfd);
            if (fd == STDIN_FILENO) ret |= STDIN_REDIRECTED;
            if (fd == STDOUT_FILENO) ret |= STDOUT_REDIRECTED;
            if (fd == STDERR_FILENO) ret |= STDERR_REDIRECTED;
            i++;
        }
    }
    if (redirect_start != -1) {
        comm->argc = redirect_start;
        comm->argv[comm->argc] = NULL;
    }
    return ret;
}

void restoreStandardIO(int redirectionFlag) {
    if (redirectionFlag & STDIN_REDIRECTED) dup2(SavedStdinFD, STDIN_FILENO);
    if (redirectionFlag & STDOUT_REDIRECTED) dup2(SavedStdoutFD, STDOUT_FILENO);
    if (redirectionFlag & STDERR_REDIRECTED) dup2(SavedStderrFD, STDERR_FILENO);
}

static int executeBuiltIn(Commands_t *command) {
    if (!command || !command->argv[0]) return -1;
    if (strcmp("exit", command->argv[0]) == 0) return 1;
    if (strcmp("pwd", command->argv[0]) == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
        return 0;
    }
    if (strcmp("echo", command->argv[0]) == 0) {
        for (int i = 1; i < command->argc; i++) printf("%s%s", command->argv[i], (i < command->argc - 1) ? " " : "");
        printf("\n");
        return 0;
    }
    if (strcmp("cd", command->argv[0]) == 0) {
        const char *path = command->argc > 1 ? command->argv[1] : getenv("HOME");
        return chdir(path) != 0 ? 1 : 0;
    }
    if (strcmp("export", command->argv[0]) == 0 && command->argc == 2) {
        char *equal = strchr(command->argv[1], '=');
        if (equal) {
            *equal = '\0';
            setenv(command->argv[1], equal + 1, 1);
        }
        return 0;
    }
    return -1;
}

static void executeBinary(Commands_t *command) {
    pid_t pid = fork();
    if (pid == -1) { status = 1; return; }
    if (pid == 0) {
        execvp(command->argv[0], command->argv);
        _exit(errno == ENOENT ? 127 : 126);
    } else {
        int wstatus;
        waitpid(pid, &wstatus, 0);
        status = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
    }
}

int ExecuteCommand(Commands_t *command) {
    if (LocalVariableAssignHandler(command)) return 0;
    LocalVariableReplace(command);
    if (command->argc == 0) return 0;
    int redirectionFlag = IO_RedirectionHandler(command);
    if (redirectionFlag == -1) { status = 1; return 0; }
    int ret = executeBuiltIn(command);
    if (ret == 1) return 1;
    if (ret == -1) executeBinary(command);
    restoreStandardIO(redirectionFlag);
    return 0;
}

int microshell_main(int argc, char **argv) {
    (void)argc; (void)argv;
    char *input = (char *)malloc(INPUT_BUFFER_INIT_SIZE);
    size_t inputSize = INPUT_BUFFER_INIT_SIZE;
    LocalVariablesList = list_create();
    Commands_t command;
    Tokenizer_init(&command);
    SavedStdinFD = dup(STDIN_FILENO);
    SavedStdoutFD = dup(STDOUT_FILENO);
    SavedStderrFD = dup(STDERR_FILENO);
    while (1) {
        fflush(stdout);
        if (getline(&input, &inputSize, stdin) == -1) break;
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') input[len - 1] = '\0';
        if (input[0] == '\0') continue;
        int ret = Tokenize_Line(input, strlen(input), &command);
        if (ret == 0) {
            if (ExecuteCommand(&command) == 1) break;
        } else {
            status = 1;
        }
        Tokenizer_free_ArgvStrs(&command);
    }
    free(input);
    Tokenizer_free_all(&command);
    list_clear(&LocalVariablesList);
    return status;
} 