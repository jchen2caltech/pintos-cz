typedef struct _shell_command
{
    char *function;
    char **args;
    char *infile;
    char *outfile;

    int argc;
    int append;
} shell_command;

char ** mysh_parse(char *command);
shell_command ** mysh_initcommand(char **tokens);
int mysh_exec(shell_command **tasks);
void mysh_free(char **tokens, shell_command **commands);
