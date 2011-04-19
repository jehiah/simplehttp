#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "options.h"
#include "uthash.h"

enum option_type {
    OPT_BOOL = 1,
    OPT_STR = 2,
    OPT_INT = 3,
    OPT_FLOAT = 4
};

struct Option {
    char *option_name;
    int option_type;
    int required;
    int found;
    char *value_str;
    int value_int;
    
    int default_int;
    char *default_str;
    int *dest_int;
    char **dest_str;
    int(*cb_int)(int value);
    int(*cb_str)(char *value);
    
    char *help;
    UT_hash_handle hh;
};

struct Option *options = NULL;


/*
handle the following option formats
-p {value} -p={value} (for single character options)
--port {value} --port={value} (for longer options)
--debug (implicit true)
--debug=true|false

@return 1 when parsing was successful. 0 when not
*/
int option_parse_command_line(int argc, char **argv) {
    int i;
    char *option_str;
    char *option_name;
    size_t option_name_len;
    char *value;
    struct Option *option, *tmp_option;
    
    for (i=1; i < argc; i++) {
        option_str = argv[i];
        // find the option_name
        if (strncmp(option_str, "--", 2) != 0 || strchr(option_str, '=') == NULL) {
            fprintf(stderr, "invalid argument \"%s\"\n", option_str);
            fprintf(stderr, "%d\n", strncmp(option_str, "--", 2));
            fprintf(stderr, "%p\n", strchr(option_str, '='));
            return 0;
        }
        option_name = strchr(option_str, '-');
        option_name++;
        option_name = strchr(option_name, '-');
        option_name++;
        value = strchr(option_name, '=');
        option_name_len = strlen(option_str) - (option_name - option_str) - ((option_str + strlen(option_str)) - value);
        fprintf(stderr, "%lu\n", option_name_len);
        HASH_FIND(hh, options, option_name, option_name_len, option);
        if (!option) {
            *value = '\0';
            fprintf(stderr, "unknown argument \"%s\"\n", option_name); // option_str ?
            return 0;
        }
        value++; // move past +
        // TODO: strip quotes from value
        
        switch(option->option_type) {
        case OPT_STR:
            option->value_str = value;
            if (option->cb_str) {
                // TODO: error check
                option->cb_str(value);
            }
            if (option->dest_str) {
                *(option->dest_str) = strdup(value);
            }
            break;
        case OPT_BOOL:
            if (strcasecmp(value, "false") == 0) {
                option->value_int = 0;
            } else if (strcasecmp(value, "true") == 0) {
                option->value_int = 1;
            } else {
                fprintf(stderr, "unknown value for --%s (%s). should be \"true\" or \"false\"\n", option->option_name, value);
                return 0;
            }
            if (option->cb_int) {
                // TODO: error check
                option->cb_int(option->value_int);
            }
            if (option->dest_int) {
                *(option->dest_int) = option->value_int;
            }
            break;
        case OPT_INT:
            option->value_int = atoi(value);
            if (option->cb_int) {
                // TODO: error check
                option->cb_int(option->value_int);
            }
            if (option->dest_int) {
                *(option->dest_int) = option->value_int;
            }
            break;
        }
        option->found++;
    }
    
    // check for not found entries
    HASH_ITER(hh, options, option, tmp_option) {
        if (option->required == OPT_REQUIRED && option->found == 0) {
            fprintf(stderr, "ERROR: required option --%s not present\n", option->option_name);
            return 0;
        }
    }
    return 1;
}

/* 
@returns -1 if option not found or not defined
*/
int option_get_int(const char *option_name) {
    struct Option *option;
    HASH_FIND_STR(options, option_name, option);
    if (!option) {
        return -1;
    }
    if (option->found) {
        return option->value_int;
    }
    return option->default_int;
}

char *option_get_str(const char *option_name) {
    struct Option *option;
    HASH_FIND_STR(options, option_name, option);
    if (!option) {return NULL;}
    if (option->found) {
        return option->value_str;
    }
    return option->default_str;
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
    option->found = 0;
    option->value_str = NULL;
    option->value_int = 0;
    option->cb_int = NULL;
    option->default_int = 0;
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

int option_define_str(const char *option_name, int required, char *default_val, char **dest, int(*cb)(char *value), const char *help) {
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

int option_define_bool(const char *option_name, int required, int default_val, int *dest, int(*cb)(int value), const char *help) {
    struct Option *option = new_option(option_name, required, help);
    if (!option) {return -1;}
    option->option_type = OPT_BOOL;
    option->default_int = default_val;
    option->dest_int = dest;
    option->cb_int = cb;
    return 1;
}

int option_help(int arg) {
    struct Option *option, *tmp_option;
    fprintf(stdout, "--------------\n");
    // SORT
    HASH_ITER(hh, options, option, tmp_option) {
        fprintf(stdout, "\t--%-20s", option->option_name);
        switch(option->option_type) {
        case OPT_STR:
            if (option->default_str) {
                fprintf(stdout, "=%s", option->default_str);
            } else {
                fprintf(stdout, "=<str>");
            }
            break;
        case OPT_INT:
            if (option->default_int) {
                fprintf(stdout, "=%d", option->default_int);
            } else {
                fprintf(stdout, "=<int>");
            }
            break;
        case OPT_BOOL:
            fprintf(stdout, "=True|False");
            break;
        default:
            fprintf(stdout, "=<value>");
            break;
        }
        fprintf(stdout, "\n");
        if (option->help) {
            fprintf(stdout, "\t\t%s\n", option->help);
        }
    }
    return 0;
}

void free_options() {
    struct Option *option, *tmp_option;
    HASH_ITER(hh, options, option, tmp_option) {
        HASH_DELETE(hh, options, option);
        free(option->option_name);
        free(option->help);
        free(option->default_str);
        free(option);
    }
}

// format_option_name(char *option_name)
// return option_name.lower().replace('-','_')
