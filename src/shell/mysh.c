#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "mysh.h"

#define COMMANDSIZE 256
#define TOKENSIZE 50
#define NUMTOKENS 10
#define NUMCOMMANDS 5

int commandcount;

/* mysh_parse
 *
 * Description:
 * Parsing function that tokenizes the command string.
 * 
 * Arguments:
 * -command: pointer to the start of the command string
 *
 * Return Value:
 * -tokens: pointer to an array of token strings
 *
 * Possible Errors:
 * -TOKEN_TOO_LARGE: individual token exceeding set TOKENSIZE
 * -ALLOC_FAILURE: system cannot allocate memory by malloc or realloc
 */

char ** mysh_parse(char *command) {

    /*Tokenize our input command string*/
    
    char **tokens;
    char **currtoken;
    int currmaxtoken = NUMTOKENS;
    int tokenlen = 0;
    int tokencount = 0;
    int doublequote = 0;
    char *curr = command;
    char *tokencursor;

    tokens = (char**)malloc(currmaxtoken * sizeof(char*));
    if (!tokens)
        fprintf(stderr, "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
    currtoken = tokens;
    *currtoken = (char*)malloc(TOKENSIZE * sizeof(char));
    tokencursor = *currtoken;
    
    /*Skip the beginning whitespaces, before handling*/
    while (*curr == (char)' ')
        ++curr;
    
    while ((*curr) && (*curr != '\n')) {
        switch (*curr) {
        /* Double-quote case */
        case '"':
            if (!doublequote) { /* Already inside a double quote */
                if (tokencursor == *currtoken) { /* if it is a new token */
                    *tokencursor = *curr;
                    ++curr;
                    ++tokenlen;
                    ++tokencursor;
                    doublequote = 1;
                    break;
                }
                *tokencursor = 0;   /* End current token to start a quoted one */
            }
            else {
                *tokencursor = *curr;   /* End current quoted one */
                ++tokenlen;
                if (tokenlen >= TOKENSIZE) {
                    fprintf(stderr, "ERROR: TOKEN TOO LARGE");
                    return NULL;
                }
                ++tokencursor;
                *tokencursor = 0;
            }
            /* Start a new token */
            ++currtoken;
            ++tokencount;
            if (tokencount >= currmaxtoken) {
                currmaxtoken += NUMTOKENS;
                tokens = (char**)realloc(tokens, currmaxtoken * sizeof(char*));
                if (!tokens)
                    fprintf(stderr, 
                    "ALLOC_FAILURE: Cannot allocate memory by realloc.\n");
                currtoken = (char**)(tokens + tokencount * sizeof(char*));
            }
            *currtoken = (char*)malloc(TOKENSIZE * sizeof(char));
            if (!currtoken)
                fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
            tokencursor = *currtoken;
            tokenlen = 0;
            if (!doublequote) {
                doublequote = 1;
                *tokencursor = *curr;
                ++tokenlen;
                ++tokencursor;
                ++curr;
            }
            else {
                doublequote = 0;
                ++curr;
                while (*curr == ' ')
                    ++curr;
            }
            break;
 
        /* Non-whitespace separation characters */
        case '>':
        case '<': 
        case '|':
            if (!doublequote) {
                if (tokencursor != *currtoken) {
                /* Not in a new token; need to end the current one */
                    *tokencursor = 0;
                    ++currtoken;
                    ++tokencount;
                    if (tokencount >= currmaxtoken) {
                        currmaxtoken += NUMTOKENS;
                        tokens = (char**)realloc(tokens, currmaxtoken * sizeof(char*));
                        if (!tokens)
                            fprintf(stderr, 
                            "ALLOC_FAILURE: Cannot allocate memory by realloc.\n");
                        currtoken = (char**)(tokens + tokencount * sizeof(char*));
                    }
                    *currtoken = (char*)malloc(2 * sizeof(char));
                    if (!currtoken)
                        fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                    tokencursor = *currtoken;
                }
                *tokencursor = *curr;
                ++tokencursor;
                ++curr;
                /* No need to break here, since we'll use the space case code */
            }
            /* If already inside a double-quote, will go straight down to default */

        /* White space handling */
        case ' ':
            if (!doublequote) {
                if (tokencursor != *currtoken) {
                    /* Consider if the command starts with a space */
                    *tokencursor = 0;
                    ++currtoken;
                    ++tokencount;
                    if (tokencount >= currmaxtoken) {
                        currmaxtoken += NUMTOKENS;
                        tokens = (char**)realloc(tokens, currmaxtoken * sizeof(char*));
                        if (!tokens)
                            fprintf(stderr, 
                            "ALLOC_FAILURE: Cannot allocate memory by realloc.\n");
                        currtoken = (char**)((int)tokens + tokencount * sizeof(char*));
                    }
                    tokenlen = 0;
                    *currtoken = (char*)malloc(TOKENSIZE * sizeof(char));
                    if (!currtoken)
                        fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                    tokencursor = *currtoken;
                }
                while (*curr == ' ')
                    ++curr;
                break;
            }
            /* If already inside a double-quote, will go straight down to default */    

        default:
            *tokencursor = *curr;
            ++tokenlen;
            ++curr;
            ++tokencursor;
            if (tokenlen > TOKENSIZE) {
                fprintf(stderr, "ERROR: TOKEN TOO LARGE");
                return NULL;
            }
        }
    }
    /* the command-line prompt always ends with "\n", but we don't want it */
    if (tokencursor != *currtoken) {
        *tokencursor = 0; 
        ++currtoken;
        ++tokencount;
        if (tokencount >= currmaxtoken) {
            ++currmaxtoken;
            tokens = (char**)realloc(tokens, currmaxtoken * sizeof(char*));
            if (!tokens)
                fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by realloc.\n");
            currtoken = (char**)(tokens + tokencount * sizeof(char*));
        }
    }
    else
        free(*currtoken);
    *currtoken = (char*)NULL;
    currtoken = tokens;
    return tokens;
}

/* mysh_initcommand
 *
 * Description:
 * This function initializes the command structures based on tokens passed in.
 *
 * Arguments:
 * -tokens: an array of token strings, presumably parsed by mysh_parse
 *
 * Return Values:
 * -commands: an array of shellCommand Structs, each specifying an executable
 *            task
 *
 * Possible Errors:
 * -SYNTAX_ERROR: happens when the tokens violate certain rules
 * -ALLOC_FAILURE: system cannot allocate memory by malloc or realloc
 */
shellCommand ** mysh_initcommand(char ** tokens) {
    shellCommand **commands, **currcommand;
    int i;
    int cmdcount = 0;
    int argcount = 0;
    int newcommand = 1;
    char **currtoken, **arghead, **currarg;
    
    currtoken = tokens;
    while (*currtoken != NULL) {
        if (**currtoken != 0) {
            if (newcommand) {
                switch (**currtoken) {
                case '>':
                case '<':
                case '|':
                    fprintf(stderr, "SYNTAX_ERROR: expecting command around | character\n");
                    return (shellCommand**)NULL;
                default:
                    ++cmdcount;
                    ++currtoken;
                    newcommand = 0;
                }
            }
            else if (strcmp(*currtoken, "|") == 0) {
                newcommand = 1;
                ++currtoken;
            }
            else {
                ++currtoken;
            }
        }
    }
    commandcount = cmdcount;    

    commands = (shellCommand **)malloc((cmdcount + 1) * sizeof(shellCommand*));
    if (!commands)
        fprintf(stderr, "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                
    currtoken = tokens;
    currcommand = commands;
    
    while (1) {
        *currcommand = (shellCommand *)malloc(sizeof(shellCommand));
        if (!currcommand)
            fprintf(stderr, "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
        (*currcommand)->function = strdup(*currtoken);
        ++currtoken;
        argcount = 0;
        if (*currtoken != NULL)
            arghead = currtoken;
        while ((*currtoken != NULL) && (**currtoken != 0) && (**currtoken != '|') \
                && (**currtoken != '>') && (**currtoken != '<')) {
            ++argcount;
            ++currtoken;
        }
        (*currcommand)->args = (char **) NULL;
        (*currcommand)->infile = NULL;
        (*currcommand)->outfile = NULL;
        (*currcommand)->errorfile = NULL;
        (*currcommand)->argc = argcount;
        (*currcommand)->append = 0;
        if (argcount) {
            (*currcommand)->args = (char **)malloc((argcount+1) * sizeof(char*));
            currtoken = arghead;
            currarg = (*currcommand)->args;
            for (i = 0; i < argcount; i++) {
                *currarg = strdup(*currtoken);
                ++currarg;
                ++currtoken;
            }
            *currarg = (char *)NULL; 
        }
        while ((*currtoken != NULL) && (**currtoken != 0) && (**currtoken != '|')) {
            switch (**currtoken) {
            case ('>'):
                (*currcommand)->append = 0;
                ++currtoken;
                if ((*currtoken == NULL) || (**currtoken == 0) || \
                    (**currtoken == '|')) {
                    fprintf(stderr, "ERROR: syntax error around > character\n");
                    return NULL;
                } else if (**currtoken == '>') {
                    (*currcommand)->append = 1;
                    ++currtoken;
                    if ((*currtoken == NULL) || (**currtoken == 0) || \
                        (**currtoken == '|')) {
                        fprintf(stderr, "ERROR: syntax error around > character\n");
                        return NULL;
                    }
                }
                (*currcommand)->outfile = strdup(*currtoken);
                break;
            case ('<'):
                ++currtoken;
                if ((*currtoken == NULL) || (**currtoken == 0) || \
                    (**currtoken == '|')) {
                    fprintf(stderr, "ERROR: syntax error around < character\n");
                    return NULL;
                }
                (*currcommand)->infile = strdup(*currtoken);
                break;
            default:
                fprintf(stderr, "ERROR: syntax error\n");
            }
            ++currtoken;
        }
        if ((*currtoken == NULL) || (**currtoken == 0))
            break;
        ++currtoken; 
        ++currcommand;
    }
    ++currcommand;
    *currcommand = NULL;
    return commands;
}

/* mysh_exec
 *
 * Description:
 * This function executes the sequence of commands by invoking programs specified, 
 * or running internal commands (cd & chdir).
 *
 * Arguments:
 * -tasks: pointer to an array of shellCommand structs that specify the task sequence
 *
 * Return Value:
 * 0 on success, -1 on failure.
 *
 * Possible Error:
 * -PIPE_FAILURE
 * -OPEN_FAILURE
 * -DUP2_FAILURE
 * -FORK_FAILURE
 * -EXECVE_FAILURE
 * -ALLOC_FAILURE
 */ 

int mysh_exec(shellCommand **tasks) {
    pid_t childpid;
    char **argv;
    int i, taskremain, haveprev, currchild;
    int currfd[2], prevfd[2];
    char *function, *path, *login;
    int in_fd, out_fd;

    shellCommand **currtask = tasks;
    taskremain = commandcount;
    haveprev = 0;
    currchild = 0;
    while (*currtask != NULL) {
        --taskremain;
        if (taskremain) {
            /* Create new pipe if there are tasks remaining */
            if (pipe(currfd))
                perror("ERROR");
        }
        argv = (char **)malloc(((*currtask)->argc + 2) * sizeof(char*));
        if (!argv)
            fprintf(stderr, "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
        function = strdup((*currtask)->function);
        if ((strcmp(function, "cd") == 0) || (strcmp(function, "chdir") == 0)) {
            /* cd and chdir implementation */
            if (!(*currtask)->argc) {
                login = getlogin();
                path = (char *)malloc((strlen(login)+strlen("/home/")) * sizeof(char));
                if (!path)
                    fprintf(stderr, "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                strcpy(path, "/home/");
                strcat(path, login);
                /* cd home if address not specified */
            }
            else {
                path = strdup(*((*currtask)->args));
            }
            chdir(path);
            return 0;
        }
        for (i = 0; i < (*currtask)->argc; i++) {
            /* set up arguments */
            argv[i+1] = strdup(((*currtask)->args)[i]);
        }
        argv[i+1] = NULL;
        argv[0] = (char *)malloc((strlen(function)+5) * sizeof(char));
        if (!(argv[0]))
            fprintf(stderr, "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
        if (function && (*function != '/') && (*function != '.')) {
            /* If the function doesn't specify a path, add 'bin' before it */
            strcpy(argv[0], "/bin/");
            strcat(argv[0], function);
        }
        else {  /* If the function specifies a path, use it directly */
            strcpy(argv[0], function);
        }
        childpid = fork();
        if (childpid == (pid_t)0){
            if ((*currtask)->infile) {
                in_fd = open((*currtask)->infile, O_RDONLY);
                if (!in_fd)
                    perror("ERROR");
                if (dup2(in_fd, STDIN_FILENO))
                    perror("ERROR");
                close(in_fd);
            }
            if ((*currtask)->outfile) {
                if ((*currtask)->append) {
                    out_fd = open((*currtask)->outfile, O_CREAT | O_APPEND | O_WRONLY, \
                              S_IRUSR | S_IWUSR);
                }
                else {
                     out_fd = open((*currtask)->outfile, O_CREAT | O_TRUNC | O_WRONLY, \
                              S_IRUSR | S_IWUSR);
                }
                if (!out_fd)
                    perror("ERROR");
                if (dup2(out_fd, STDOUT_FILENO) < 0)
                    perror("ERROR");
                close(out_fd);
            }
            if (taskremain) {
                if (dup2(currfd[1], STDOUT_FILENO) < 0)
                    perror("ERROR");
                close(currfd[0]);
                close(currfd[1]);
            }
            if (haveprev) {
                if (dup2(prevfd[0], STDIN_FILENO) < 0)
                    perror("ERROR");
                close(prevfd[1]);
                close(prevfd[0]);
            }
            execve(argv[0], &argv[0], NULL);
            perror("ERROR");
            exit(2);
        }
        else if (childpid < (pid_t) 0) {
            perror("ERROR");
            return -1;
        }
        else {
            if (taskremain) 
                close(currfd[1]);
            wait(&childpid);
            if (haveprev) {
                close(prevfd[0]);  
            }
            prevfd[0] = currfd[0];
            prevfd[1] = currfd[1];
        }
        ++currchild;
        ++currtask;
        haveprev = 1;
    }
    if (commandcount > 1) {
        close(currfd[0]);
        close(currfd[1]);
    }
    return 0;
}

/* mysh_free
 * 
 * Description:
 * This function frees the memory allocated for the token array and command array.
 *
 * Arguments:
 * -tokens: the array of tokens to be free'd
 * -commands: the array of commands to be free'd
 *
 * Return Value:
 * None.
 *
 * Possible Errors:
 * None.
 */ 

void mysh_free(char **tokens, shellCommand **commands) {
    char **curtoken = tokens;
    shellCommand **curcmd = commands;
    int i;
    
    /*First Free the tokens.*/
    if (tokens) {
        while (*curtoken){
            free(*curtoken);
            ++curtoken;
        }
        free(*curtoken);
        free(tokens);
    }
    
    /*Then free commands*/
    if (commands) {
        while (*curcmd){
            free((*curcmd)->function);
            if ((*curcmd)->infile)
                free((*curcmd)->infile);
            if ((*curcmd)->outfile)
                free((*curcmd)->outfile);
            if ((*curcmd)->errorfile)
                free((*curcmd)->errorfile);
            for (i = 0; i < (*curcmd)->argc; i++) {
                free(((*curcmd)->args)[i]);
            }
            free((*curcmd)->args);
            free(*curcmd);
            ++curcmd;
        }
        free(*curcmd);
        free(commands);
    }
}

int main(int argc, char ** argv) {
    
    char *login, *cwd;
    char command[COMMANDSIZE];
    shellCommand **tasks;
    char **tokens;
    
    while (1) {
        tokens = NULL;
        tasks = NULL;
        login = getlogin();
        if ((cwd = getcwd(NULL, 64)) == NULL) {
            fprintf(stderr, "Fatal error: cannot find current path\n");
            exit(2);
        }
        printf("%s:%s> ", login, cwd);
        if (fgets(command, COMMANDSIZE, stdin) != NULL) {
            if (((tokens = mysh_parse(command)) != NULL) && (*tokens)) {
                if (strcmp(*tokens, "exit") == 0) {
                    mysh_free(tokens, tasks);
                    printf("Exiting shell... \n");
                    exit(0);
                }
                if ((tasks = mysh_initcommand(tokens)) != NULL) {
                    mysh_exec(tasks);
                }
            }
        }
        mysh_free(tokens, tasks);
    }
    
    return 1;
}
