#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "mysh.h"

#define COMMANDSIZE 256
#define TOKENSIZE 50
#define NUMTOKENS 10
#define NUMCOMMANDS 5

int commandcount;

char ** mysh_parse(char *command) {

    char **tokens;
    char **currtoken;
    int currmaxtoken = NUMTOKENS;
    int tokenlen = 0;
    int tokencount = 0;
    int doublequote = 0;
    char *curr = command;
    char *tokencursor;

    tokens = (char**)malloc(currmaxtoken * sizeof(char*));
    currtoken = tokens;
    *currtoken = (char*)malloc(TOKENSIZE * sizeof(char));
    tokencursor = *currtoken;
    while (*curr == (char)' ')
        ++curr;
    while (*curr) {
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
                currtoken = (char**)(tokens + tokencount * sizeof(char*));
            }
            *currtoken = (char*)malloc(TOKENSIZE * sizeof(char));
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
                        currtoken = (char**)(tokens + tokencount * sizeof(char*));
                    }
                    *currtoken = (char*)malloc(2 * sizeof(char));
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
                        currtoken = (char**)(tokens + tokencount * sizeof(char*));
                    }
                    tokenlen = 0;
                    *currtoken = (char*)malloc(TOKENSIZE * sizeof(char));
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
    --tokencursor;
    *tokencursor = 0;
    ++currtoken;
    ++tokencount;
    if (tokencount >= currmaxtoken) {
        ++currmaxtoken;
        tokens = (char**)realloc(tokens, currmaxtoken * sizeof(char*));
        currtoken = (char**)(tokens + tokencount * sizeof(char*));
    }
    *currtoken = (char*)NULL;
    printf("There are %d tokens\n", tokencount);
    return tokens;
}

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
                    fprintf(stderr, "ERROR: expecting command around | character\n");
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
    printf("%d commands recorded\n", cmdcount);
    commandcount = cmdcount;    

    commands = (shellCommand **)malloc((cmdcount + 1) * sizeof(shellCommand*));
    currtoken = tokens;
    currcommand = commands;
    
    while (1) {
        *currcommand = (shellCommand *)malloc(sizeof(shellCommand));
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
        (*currcommand)->argc = argcount;
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
                ++currtoken;
                if ((*currtoken == NULL) || (**currtoken == 0) || \
                    (**currtoken == '|')) {
                    fprintf(stderr, "ERROR: syntax error around > character\n");
                    return NULL;
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
        ++currcommand;
    }
    ++currcommand;
    *currcommand = NULL;
    return commands;
}

void mysh_exec(shellCommand **tasks) {
    pid_t childpid;
    char **argv;
    int i;
    char *function, *temp;
    shellCommand **currtask = tasks;

    argv = (char **)malloc(((*currtask)->argc + 2) * sizeof(char*));
    function = strdup((*currtask)->function);
    if (strcmp(function, "exit") == 0)
        return;
    for (i = 0; i < (*currtask)->argc; i++) {
        argv[i+1] = strdup(((*currtask)->args)[i]);
    }
    argv[i+1] = NULL;
    argv[0] = (char *)malloc((strlen(function)+5) * sizeof(char));
    strcpy(argv[0], "/bin/");
    strcat(argv[0], function);
    childpid = fork();
    if (childpid == 0){
        execve(argv[0], &argv[0], NULL);
    }
    if (childpid) {
        wait(&childpid);
    }
}


int main(int argc, char ** argv) {
    
    char *login, *cwd;
    char command[COMMANDSIZE];
    shellCommand **tasks;
    int running = 1;
    char **tokens;
    
    while (running) {
        login = getlogin();
        if ((cwd = getcwd(NULL, 64)) == NULL) {
            fprintf(stderr, "Fatal error: cannot find current path\n");
            exit(2);
        }
        printf("%s:%s> ", login, cwd);
        if (fgets(command, COMMANDSIZE, stdin) != NULL) {
            if ((tokens = mysh_parse(command)) != NULL) {
                if (strcmp(*tokens, "exit") == 0) {
                    printf("Exitting shell... \n");
                    running = 0;
                }
                if ((tasks = mysh_initcommand(tokens)) != NULL) {
                    mysh_exec(tasks);
                }
            }
        }
    }
    
    return 1;
}
