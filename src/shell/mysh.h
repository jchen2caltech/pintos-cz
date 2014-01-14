typedef struct _shell_command
{
    char *function;
    char **args;
    char *infile;
    char *outfile;

    int argc;
    int append;
} shell_command;


