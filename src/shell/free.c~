void mysh_free(char **tokens, shellCommand **commands) {
    char **curtoken = tokens;
    shellCommand **curcmd = commands;
    
    /*First Free the tokens.*/
    while (*curtoken != NULL){
        free(*curtoken);
        ++curtoken;
    }
    free(*curtoken);
    free(tokens);
    
    /*Then free commands*/
    while (*curcmd != NULL){
        free(*curcmd);
        ++curcmd;
    }
    free(*curcmd);
    free(commands);
}
