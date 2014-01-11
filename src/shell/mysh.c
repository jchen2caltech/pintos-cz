#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "mysh.h"

#define COMMANDSIZE 256
#define TOKENSIZE 50
#define NUMTOKENS 10
#define NUMCOMMANDS 5

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
                exit(2);
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
    shellCommand ** commands;
    int commandcount = 0;
    int argcount = 0;
    int newcommand = 1;
    char **args, **currtoken;
    
    currtoken = tokens;
    while (*currtoken != NULL) {
        if (**currtoken != NULL) {
            if (newcommand) {
                if (strcmp(*currtoken, "|") == 0) {
                    fprintf(stderr, "ERROR: expecting command around "|" character\n");
                    return (shellCommand**)NULL;
                }   
                ++commandcount;
                ++currtoken;
                newcommand = 0;
            }
            else if (strcmp(*currtoken, "|") == 0) {
                newcommand = 1;
                ++currtoken;
            }
        }
    }
    printf("%d commands recorded\n", commandcount);
    return (shellCommand**)NULL;
}

int main(int argc, char ** argv) {
    
    char *login, *cwd;
    char command[COMMANDSIZE];
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
            tokens = mysh_parse(command);
            if (strcmp(*tokens, "exit") == 0) {
                printf("Exitting shell... \n");
                running = 0;
            }
        }
    }
    
    return 1;
}
