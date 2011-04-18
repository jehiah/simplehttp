#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "simplehttp/options.h"
#include "simplehttp/uthash.h"

struct Option {
    char *option_name;
    int option_type;
    int required;
    char *value;
    
    int default_int;
    char *default_str;
    int *dest_int;
    char **dest_str;
    int(*cb_int)(int value);
    int(*cb_str)(char *value);
    
    char *help;
    UT_hash_handle hh;
};

struct Option *options;


int option_parse_command_line(int argc, char **argv) {
    // scanf --{name}={value}
    // strip quotes
    return 1;
}

struct Option *new_option(const char *option_name, int required, const char *help){
    struct Option *option;
    HASH_FIND_STR(options, option_name, option);
    if (option){
        fprintf(stderr, "option %s is already defined\n", option_name);
        return NULL;
    }
    option = malloc(sizeof(struct Option));
    option->option_name = strdup(option_name);
    option->required = required;
    option->cb_int = NULL;
    option->default_int = NULL;
    option->dest_int = NULL;
    option->cb_str = NULL;
    option->default_str = NULL;
    option->dest_str = NULL;
    option->help = NULL;
    if (help) {
        option->help = strdup(help);
    }
    return option;
}

int option_define_int(const char *option_name, int required, int default_val, int *dest, int(*cb)(int value), const char *help) {
    struct Option *option = new_option(option_name, required, help);
    if (!option) {return -1;}
    option->option_type = OPT_INT;
    option->default_int = default_val;
    option->dest_int = dest;
    option->cb_int = cb;
    return 1;
}

int option_define_str(const char *option_name, int required, char *default_val, char **dest, int(*cb)(int value), const char *help) {
    struct Option *option = new_option(option_name, required, help);
    if (!option) {return -1;}
    option->option_type = OPT_STR;
    if (default_val) {
        option->default_str = strdup(default_val);
    }
    option->dest_str = dest;
    option->cb_str = cb;
    return 1;
}


void option_help() {
    struct Option *option
}

void free_options() {
    struct Option *option, *tmp_option;
    HASH_ITER(hh, options, options, tmp_option) {
        HASH_DELETE(hh, options, option);
        free(option->option_name);
        free(option->help);
        free(option->default_str);
        free(option);
    }
}