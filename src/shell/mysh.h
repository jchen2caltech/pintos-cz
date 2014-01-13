typedef struct _shellCommand
{
    char *function;
    char **args;
    char *infile;
    char *outfile;
    char *errorfile;

    int argc;
    int append;
} shellCommand;


