typedef struct _shellCommand
{
    char *function;
    char **args;
    char *infile;
    char *outfile;

    int argc;
    int append;
} shellCommand;


