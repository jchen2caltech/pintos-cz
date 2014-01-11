#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define COMMANDSIZE 100
#define TOKENSIZE 25;
#define NUMTOKENS 10;

char ** mysh_parse(char *command) {

    char **tokens;
    int currtoken = 0;
    int currmaxtoken = NUMTOKENS;
    int tokenlen = 0;
    int doublequote = 0;
    int curr = 0;

    tokens = malloc(sizeof(char*) * NUMTOKENS);
    tokens[0] = malloc(sizeof(char) * TOKENSIZE);
    while (command[curr] == " ")
        ++curr;
    while (command[curr] != NULL) {
        if (command[curr] != " ") {
            tokens[currtoken][tokenlen] = curr;
            ++tokenlen;
            ++curr;
            if (tokenlen > TOKENSIZE) {
                fprintf(stderr, "token too large");
                exit(2);
            }
        }
        else {
            tokens[currtoken][tokenlen] = NULL;
            ++currtoken;
            if (currtoken >= currmaxtoken) {
                currmaxtoken += NUMTOKENS;
                tokens = relloc(tokens, sizeof(char*) * currmaxtoken);
            }
            tokenlen = 0;
            tokens[currtoken] = malloc(sizeof(char) * TOKENSIZE);
            while (command[curr] == " ")
                ++curr;
        }
    }
    tokens[currtoken][tokenlen] = NULL;
    tokens[currtoken + 1] = NULL;
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
        if (fgets(command, COMMANDSIZE, stdin) != NULL) {
            tokens = mysh_parse(command);
            if (strcmp(tokens[0], "exit") == 0) {
                exit(EXIT_SUCCESS);
            }
        }
    }
    
    return 1;
}
