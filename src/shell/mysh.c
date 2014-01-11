#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define COMMANDSIZE 100
#define TOKENSIZE 50
#define NUMTOKENS 10

char ** mysh_parse(char *command) {

    char **tokens;
    char **currtoken;
    int currmaxtoken = NUMTOKENS;
    int tokenlen = 0;
    int tokencount = 0;
    int doublequote = 0;
    char *curr = command;
    char *tokencurser;

    tokens = (char**)malloc(currmaxtoken * sizeof(char*));
    currtoken = tokens;
    *currtoken = (char*)malloc(TOKENSIZE * sizeof(char));
    tokencurser = *currtoken;
    while (*curr == (char)' ')
        ++curr;
    while (*curr) {
        if (*curr != (char)' ') {
            *tokencurser = *curr;
            ++tokenlen;
            ++curr;
            ++tokencurser;
        if (tokenlen > TOKENSIZE) {
                fprintf(stderr, "token too large");
                exit(2);
            }
        }
        else {
            *tokencurser = 0;
            ++currtoken;
            ++tokencount;
            if (tokencount >= currmaxtoken) {
                currmaxtoken += NUMTOKENS;
                tokens = (char**)realloc(tokens, currmaxtoken * sizeof(char*));
            }
            tokenlen = 0;
            *currtoken = (char*)malloc(TOKENSIZE * sizeof(char));
            tokencurser = *currtoken;
            while (*curr == 32)
                ++curr;
        }
    }
    --tokencurser;
    *tokencurser = 0;
    ++currtoken;
    ++tokencount;
    *currtoken = (char*)NULL;
    return tokens;
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
        if (fgets(command, 64, stdin) != NULL) {
            tokens = mysh_parse(command);
            if (strcmp(*tokens, "exit") == 0) {
                printf("Exitting shell... \n");
                running = 0;
            }
        }
    }
    
    return 1;
}
