/*! An implementation of command shell
    
    Author: Jianchi Chen, Taokun Zheng
    
    The command shell provides basic shell functionality such as 
    external program execution, several built-in commands, I/O 
    redirection, command pipeline. 
    Command maximum size is defined in COMMAND_SIZE, currently set at
    1023 characters. It can be changed by simply resetting the definition
    below.
    Incoming command input will be parsed into "tokens", which will then
    be taken to initialize shell_command struct. Any token must be
    smaller than TOKEN_SIZE characters in size, which is currently set
    at 63. This can be changed by simply resetting the definition below.
    The shell can (in theory) take infinite amount of tokens. 
    After tokens have been processed into individual tasks, the shell
    will check if the task indicates a built-in command or external
    program. The shell will fork a child process to execute the program
    if it is the latter. 
    The program path can be an absolute path, or simply the program name
    if the program is in the /bin/ directory. A "no such file or 
    directory" error will be returned if the program path is invalid.  
    The user can exit the shell by invoking the "exit" command. 

    Two Extra-Credit features have been added to the shell:
    1) ">>" instead of ">" redirection. This will redirect output to
       append to an existing file rather than truncating it. 
    2) "history" and "!n" command. The shell has a command archive that
       stores previous commands since it's started. "history" command
       will display all previous commands and their indices. "!n" will
       re-execute the command at index <n> in history. 
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "mysh.h"

#define COMMAND_SIZE 1023        /* Maximum command length */
#define TOKEN_SIZE 63            /* Maximum length of each token */
#define NUM_TOKENS 10            /* Initial number of tokens allocated;
                                  * doesn't really matter */
#define INIT_ARCHIVE_SIZE 10     /* Initial number of commands allocated in
                                  * the archive; doesn't really matter */

int task_count;
int command_count;
char **command_archive;

/*!mysh_parse
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
 * -TOKEN_TOO_LARGE: individual token exceeding set TOKEN_SIZE
 * -ALLOC_FAILURE: system cannot allocate memory by malloc or realloc
 */

char ** mysh_parse(char *command) {

    /*Tokenize our input command string*/
    
    char **tokens;
    char **currtoken;
    int currmaxtoken = NUM_TOKENS;
    int tokenlen = 0;
    int tokencount = 0;
    int doublequote = 0;
    char *curr = command;
    char *tokencursor;

    tokens = (char**)malloc(currmaxtoken * sizeof(char*));
    if (!tokens) {
        fprintf(stderr, "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
        exit(1);
    }
    currtoken = tokens;
    *currtoken = (char*)malloc((TOKEN_SIZE + 1) * sizeof(char));
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
                *tokencursor = 0;   /* End current token */
            }
            else {
                *tokencursor = *curr;   /* End current quoted one */
                ++tokenlen;
                if (tokenlen > TOKEN_SIZE) {
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
                currmaxtoken += NUM_TOKENS;
                tokens = (char**)realloc(tokens, currmaxtoken * sizeof(char*));
                if (!tokens) {
                    fprintf(stderr, 
                    "ALLOC_FAILURE: Cannot allocate memory by realloc.\n");
                    exit(1);
                }
                currtoken = (char**)(tokens + tokencount * sizeof(char*));
            }
            *currtoken = (char*)malloc(TOKEN_SIZE * sizeof(char));
            if (!currtoken) {
                fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                exit(1);
            } 
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
                        currmaxtoken += NUM_TOKENS;
                        tokens = (char**)realloc(tokens, 
                                                 currmaxtoken * sizeof(char*));
                        if (!tokens) {
                            fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by realloc\n");
                            exit(1);
                        }
                        currtoken = (char**)(tokens + \
                                             tokencount * sizeof(char*));
                    }
                    *currtoken = (char*)malloc(2 * sizeof(char));
                    if (!currtoken) {
                        fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                        exit(1);
                    }
                    tokencursor = *currtoken;
                }
                *tokencursor = *curr;
                ++tokencursor;
                ++curr;
                /* No need to break here, since we'll need whitespace code */
            }
            /* If already inside a double-quote, will go to default */

        /* White space handling */
        case ' ':
            if (!doublequote) {
                if (tokencursor != *currtoken) {
                    /* Consider if the command starts with a space */
                    *tokencursor = 0;
                    ++currtoken;
                    ++tokencount;
                    if (tokencount >= currmaxtoken) {
                        currmaxtoken += NUM_TOKENS;
                        tokens = (char**)realloc(tokens, 
                                                 currmaxtoken * sizeof(char*));
                        if (!tokens) {
                            fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by realloc.\n");
                            exit(1);
                        } 
                        currtoken = (char**)((int)tokens + \
                                              tokencount * sizeof(char*));
                    }
                    tokenlen = 0;
                    *currtoken = (char*)malloc(TOKEN_SIZE * sizeof(char));
                    if (!currtoken) {
                        fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                        exit(1);
                    }
                    tokencursor = *currtoken;
                }
                while (*curr == ' ')
                    ++curr;
                break;
            }
            /* If already inside a double-quote, will go to default */    

        default:
            *tokencursor = *curr;
            ++tokenlen;
            ++curr;
            ++tokencursor;
            if (tokenlen > TOKEN_SIZE) {
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
            if (!tokens) {
                fprintf(stderr, 
                        "ALLOC_FAILURE: Cannot allocate memory by realloc.\n");
                exit(1);
            }
            currtoken = (char**)((int)tokens + tokencount * sizeof(char*));
        }
    }
    else
        free(*currtoken);
    *currtoken = (char*)NULL;
    currtoken = tokens;
    return tokens;
}

/*!mysh_initcommand
 *
 * Description:
 * This function initializes the command structures based on tokens passed in.
 *
 * Arguments:
 * -tokens: an array of token strings, presumably parsed by mysh_parse
 *
 * Return Values:
 * -commands: an array of shell_command Structs, each specifying an executable
 *            task
 *
 * Possible Errors:
 * -SYNTAX_ERROR: happens when the tokens violate certain rules
 * -ALLOC_FAILURE: system cannot allocate memory by malloc or realloc
 */

shell_command ** mysh_initcommand(char ** tokens) {
    shell_command **commands, **currcommand;
    int i;
    int cmdcount = 0;
    int argcount = 0;
    int newcommand = 1;
    char **currtoken, **arghead, **currarg;
    
    currtoken = tokens;
    while (currtoken && *currtoken) {
        /* count number of commands */
        if (**currtoken != 0) {
            if (newcommand) {
                switch (**currtoken) {
                case '>':
                case '<':
                case '|':
                    fprintf(stderr, 
                    "SYNTAX_ERROR: expecting command around | character\n");
                    return (shell_command**)NULL;
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
    if (newcommand && tokens && (*tokens)) {
        fprintf(stderr, 
        "SYNTAX_ERROR: expecting command around | character\n");
        return NULL;
    }
    task_count = cmdcount;    

    commands = (shell_command **)malloc((cmdcount + 1) * \
               sizeof(shell_command*));
    if (!commands)
        fprintf(stderr, "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                
    currtoken = tokens;
    currcommand = commands;
    
    while (1) {
        /* Initialize a new command struct */
        *currcommand = (shell_command *)malloc(sizeof(shell_command));
        if (!currcommand) {
            fprintf(stderr, 
            "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
            exit(1);
        }
        (*currcommand)->function = strdup(*currtoken);
        ++currtoken;
        argcount = 0;
        if (*currtoken != NULL)
            arghead = currtoken;
        while ((*currtoken != NULL) && (**currtoken != 0) && \
                (**currtoken != '|') && (**currtoken != '>') && \
                (**currtoken != '<')) {
            /* Count the number of arguments */
            ++argcount;
            ++currtoken;
        }
        (*currcommand)->args = (char **) NULL;
        (*currcommand)->infile = NULL;
        (*currcommand)->outfile = NULL;
        (*currcommand)->argc = argcount;
        (*currcommand)->append = 0;
        if (argcount) {
            /* Fill in the argument array */
            (*currcommand)->args = (char **)malloc((argcount+1) * \
                                   sizeof(char*));
            currtoken = arghead;
            currarg = (*currcommand)->args;
            for (i = 0; i < argcount; i++) {
                *currarg = strdup(*currtoken);
                ++currarg;
                ++currtoken;
            }
            *currarg = (char *)NULL; 
        }
        while ((*currtoken != NULL) && (**currtoken != 0) && \
               (**currtoken != '|')) {
            /* Set up the I/O redirection */
            switch (**currtoken) {
            case ('>'):
                (*currcommand)->append = 0;
                ++currtoken;
                if ((*currtoken == NULL) || (**currtoken == 0) || \
                    (**currtoken == '|')) {
                    fprintf(stderr, 
                    "ERROR: syntax error around > character\n");
                    return NULL;
                } 
                else if (**currtoken == '>') {
                    /* Append to existing file instead of truncating */
                    (*currcommand)->append = 1;
                    ++currtoken;
                    if ((*currtoken == NULL) || (**currtoken == 0) || \
                        (**currtoken == '|')) {
                        fprintf(stderr, 
                        "ERROR: syntax error around > character\n");
                        return NULL;
                    }
                }
                (*currcommand)->outfile = strdup(*currtoken);
                break;
            case ('<'):
                ++currtoken;
                if ((*currtoken == NULL) || (**currtoken == 0) || \
                    (**currtoken == '|')) {
                    fprintf(stderr, 
                    "ERROR: syntax error around < character\n");
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

/*!mysh_exec
 *
 * Description:
 * This function executes the sequence of commands by invoking 
 * programs specified, or running internal commands (cd & chdir).
 *
 * Arguments:
 * -tasks: pointer to an array of shell_command structs that specify 
 *         the task sequence
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

int mysh_exec(shell_command **tasks) {
    pid_t childpid;
    char **argv;
    int i, taskremain, haveprev, currchild;
    int currfd[2], prevfd[2];
    char *function, *path, *login;
    int in_fd, out_fd;

    shell_command **currtask = tasks;
    taskremain = task_count;
    haveprev = 0;
    currchild = 0;
    while (*currtask != NULL) {
        --taskremain;
        if (taskremain) {
            /* Create new pipe if there are tasks remaining */
            if (pipe(currfd) < 0)
                perror("pipe failure");
        }
        argv = (char **)malloc(((*currtask)->argc + 2) * sizeof(char*));
        if (!argv) {
            fprintf(stderr, 
            "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
            exit(1);
        }
        function = strdup((*currtask)->function);
        if ((strcmp(function, "cd") == 0) || \
            (strcmp(function, "chdir") == 0)) {
            /* cd and chdir implementation */
            if (!(*currtask)->argc) {
                login = getlogin();
                path = (char *)malloc((strlen(login) + \
                        strlen("/home/") + 1) * sizeof(char));
                if (!path) {
                    fprintf(stderr, 
                    "ALLOC_FAILURE: Cannot allocate memory by malloc.\n");
                    exit(1);
                }
                strncpy(path, "/home/", 6);
                strncat(path, login, strlen(login));
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
        argv[0] = strdup(function);
        free(function);
        childpid = fork();
        if (childpid == (pid_t)0){
            /* Set up redirections */
            if ((*currtask)->infile) {
                in_fd = open((*currtask)->infile, O_RDONLY);
                if (!in_fd)
                    perror("cannot open input file");
                if (dup2(in_fd, STDIN_FILENO))
                    perror("cannot set up input redirection");
                close(in_fd);
            }
            if ((*currtask)->outfile) {
                if ((*currtask)->append) {
                    out_fd = open((*currtask)->outfile, \
                             O_CREAT | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR);
                }
                else {
                    out_fd = open((*currtask)->outfile, \
                             O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
                }
                if (!out_fd)
                    perror("cannot open output file");
                if (dup2(out_fd, STDOUT_FILENO) < 0)
                    perror("cannot set up output redirection");
                close(out_fd);
            }
            /* Set up piping if there is any */
            if (taskremain) {
                if (dup2(currfd[1], STDOUT_FILENO) < 0)
                    perror("cannot set up output pipeline");
                close(currfd[0]);
                close(currfd[1]);
            }
            if (haveprev) {
                if (dup2(prevfd[0], STDIN_FILENO) < 0)
                    perror("cannot set up input pipeline");
                close(prevfd[1]);
                close(prevfd[0]);
            }
            execvp(argv[0], &argv[0]);
            perror("execution error");
            exit(2);
        }
        else if (childpid < (pid_t) 0) {
            perror("fork failure");
            return -1;
        }
        else {
            if (taskremain) 
                close(currfd[1]);
            if (waitpid(childpid, NULL, 0) < 0) {
                perror("process failure");
                return -1;
            }
            if (haveprev) {
                close(prevfd[0]);  
            }
            prevfd[0] = currfd[0];
            prevfd[1] = currfd[1];
            for (i = 0; i < (*currtask)->argc + 1; i++) {
                free(argv[i]);
            }
            free(argv);
        }
        ++currchild;
        ++currtask;
        haveprev = 1;
    }
    if (task_count > 1) {
        close(currfd[0]);
        close(currfd[1]);
    }
    return 0;
}

/*!mysh_free
 * 
 * Description:
 * This function frees the memory allocated for the token array and 
 * command array.
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

void mysh_free(char **tokens, shell_command **commands) {
    char **curtoken = tokens;
    shell_command **curcmd = commands;
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

int main(void) {
    
    char *login, *cwd;
    char command[COMMAND_SIZE + 1];
    shell_command **tasks;
    char **tokens;
    int archive_volume, i;

    /* Setting up history command archive */
    command_archive = (char**)malloc(INIT_ARCHIVE_SIZE * sizeof(char*));
    command_count = 0;
    archive_volume = INIT_ARCHIVE_SIZE;
    while (1) {
        tokens = NULL;
        tasks = NULL;
        login = getlogin();
        if ((cwd = getcwd(NULL, 64)) == NULL) {
            fprintf(stderr, "Fatal error: cannot find current path\n");
            exit(2);
        }
        printf("%s:%s> ", login, cwd); /* print username and current path */
        if (fgets(command, COMMAND_SIZE, stdin) != NULL) {
            if (*command == '!') {     /* Execute a history command */
                if (command[1]) {
                    i = atoi(&(command[1]));
                    if ((i == 0) && (command[1] != '0')) {
                        /* character following ! is not a number */
                        fprintf(stderr, 
                        "SYNTAX_ERROR: expecting integer after ! \n");
                        mysh_free(tokens, tasks);
                        tokens = NULL; /* Avoid following executions */
                    }
                    else {
                        if (i > command_count) {
                            fprintf(stderr, 
                        "SYNTAX_ERROR: integer out of archive range \n");
                            tokens = NULL; /* Avoid following executions */
                        }
                        else {
                            strncpy(command, command_archive[i],
                                  COMMAND_SIZE);
                            printf("%s", command);
                            tokens = mysh_parse(command);
                        }
                    }
                }
            }
            else {
                tokens = mysh_parse(command);
            }
            if ((tokens) && (*tokens)) {
                if (tokens && (*tokens)) {
                    if (strcmp(*tokens, "exit") == 0) {
                        /* Exit shell */
                        mysh_free(tokens, tasks);
                        for (i = 0; i < command_count; i++) {
                            free(command_archive[i]);
                        }
                        free(command_archive);
                        printf("Exiting shell... \n");
                        exit(0);
                    }
                    else if (strcmp(*tokens, "history") == 0) {
                        /* Display command history */
                        for (i = 0; i < command_count; i++) {
                            printf("%d  %s", i, command_archive[i]);
                        }
                    }
                    else if ((tasks = mysh_initcommand(tokens)) != NULL) {
                        mysh_exec(tasks);
                    }
                }
            }
        }
        mysh_free(tokens, tasks);
        command_archive[command_count] = strdup(command);
        ++command_count;
        if (command_count >= archive_volume) {
            archive_volume += INIT_ARCHIVE_SIZE;
            command_archive = (char **)realloc(command_archive, \
                                               archive_volume * sizeof(char*));
            if (!command_archive) {
                fprintf(stderr, "ALLOC_FAILURE: cannot allocate memory \
                                 by realloc");
                exit(1);
            }
        }
    }
    
    return 1;
}
