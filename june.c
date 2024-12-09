#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#define JUNE_VERSION "June 1.1 rev 0"

#define JUNE_USAGE "Usage: june [opts] [-f <file>] [rules]\n"
#define JUNE_FILE  "jfile"

#define MAX_RULES 256
#define MAX_VARS  1024


typedef struct {
    int is_patern;
    union {
        char *name;
        struct {
            char *src_ext;
            char *dst_ext;
        } patern;
    };
    char **deps;
    char **cmds;
} rule_t;

typedef struct {
    char *name;
    char *value;
} var_t;

typedef struct {
    char *name;
    char *(*func)(int, char **);
} jsf_t;

typedef struct {
    int virtual;
    int debug;
    char *file;
    char **rules;
} juneopt_t;

rule_t *g_rules;
var_t *g_vars;
juneopt_t g_opt;

/*********************************
 *                              *
 *   Variable Access Functions  *
 *                              *
*********************************/

char *get_var(char *name) {
    for (int i = 0; g_vars[i].name; i++) {
        if (!strcmp(g_vars[i].name, name))
            return g_vars[i].value;
    }

    return NULL;
}

int set_var(char *name, char *value) {
    for (int i = 0; g_vars[i].name; i++) {
        if (!strcmp(g_vars[i].name, name)) {
            free(g_vars[i].value);
            g_vars[i].value = value;
            free(name);
            return 0;
        }
    }

    for (int i = 0; i < MAX_VARS; i++) {
        if (!g_vars[i].name) {
            g_vars[i].name = name;
            g_vars[i].value = value;
            return 0;
        }
    }

    return 1;
}

void free_globals() {
    for (int i = 0; g_vars[i].name; i++) {
        free(g_vars[i].name);
        free(g_vars[i].value);
    }
    for (int i = 0; g_rules[i].name; i++) {
        if (g_rules[i].is_patern) {
            free(g_rules[i].patern.src_ext);
            free(g_rules[i].patern.dst_ext);
        } else {
            free(g_rules[i].name);
        }
        for (int j = 0; g_rules[i].deps[j]; j++)
            free(g_rules[i].deps[j]);
        free(g_rules[i].deps);
        if (!g_rules[i].cmds)
            continue;
        for (int j = 0; g_rules[i].cmds[j]; j++)
            free(g_rules[i].cmds[j]);
        free(g_rules[i].cmds);
    }
}

void print_rule(rule_t *rule) {
    if (rule->is_patern) {
        fprintf(stderr, "RULE: %s -> %s\n", rule->patern.src_ext, rule->patern.dst_ext);
    } else {
        fprintf(stderr, "RULE: %s\n", rule->name);
    }

    for (int i = 0; rule->deps[i]; i++)
        fprintf(stderr, "  -> '%s'\n", rule->deps[i]);

    for (int i = 0; rule->cmds && rule->cmds[i]; i++)
        fprintf(stderr, "  [%d] %s\n", i, rule->cmds[i]);

    fprintf(stderr, "\n");
}

/*********************************
 *                              *
 *    File Utility Functions    *
 *                              *
*********************************/

char *read_line(FILE *f) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    if ((len = getline(&line, &cap, f)) <= 0) {
        free(line);
        return NULL;
    }

    return line;
}

int file_exists(char *name) {
    if (!g_opt.virtual && access(name, F_OK) == -1)
        return 0;
    return 1;
}

FILE *chdir_and_open(char *name, char *mode) {
    char *dir = strdup(name);
    char *tmp = strrchr(dir, '/');
    FILE *f = fopen(name, mode);
    if (tmp) {
        *tmp = '\0';
        chdir(dir);
    }
    free(dir);
    return f;
}

long file_last_modif(char *name) {
    struct stat buf;
    if (stat(name, &buf) == -1)
        return -1;
    return buf.st_mtime;
}

/*********************************
 *                              *
 *   String Utility Functions   *
 *                              *
*********************************/

int is_valid_varname(char *name) {
    if (*name == '\0')
        return 0;

    if (!isalpha(*name) && *name != '_')
        return 0;

    for (int i = 1; name[i]; i++) {
        if (!isalnum(name[i]) && name[i] != '_')
            return 0;
    }

    return 1;
}

int is_valid_filename(char *name) {
    if (*name == '\0')
        return 0;

    for (int i = 0; name[i]; i++) {
        if (!isalnum(name[i]) &&
            name[i] != '.' &&
            name[i] != '_' &&
            name[i] != '-' &&
            name[i] != '/'
        ) return 0;
    }

    return 1;
}

char *rm_ext(char *name) {
    char *tmp = strdup(name);
    char *ext = strrchr(tmp, '.');
    if (ext)
        *ext = '\0';
    return tmp;
}

int is_right_ext(char *name, char *ext) {
    // ext does not contain the dot
    char *tmp = strrchr(name, '.');

    if (!tmp)
        return 0;

    return !strcmp(tmp + 1, ext);
}



char *str_trim(char *str) {
    int len = strlen(str);
    while (len > 0 && isspace(str[len - 1]))
        str[--len] = '\0';
    return str;
}

char *str_triml(char *str) {
    while (isspace(*str))
        str++;
    return str;
}

char **str_split(char *s, char c) {
    // if consecutive c, only one split
    // allocate each string

	char	**res;
	int		start;
	int		i_tab;
	int		i;

	i = 0;
	start = 0;
	i_tab = 0;
	res = malloc(sizeof(char *) * (strlen(s) + 1));
	while (s[i])
	{
		while (s[i] && c == s[i])
			i++;
		start = i;
		while (s[i] && c != s[i])
			i++;
		if (start < i)
			res[i_tab++] = strndup(s + start, i - start);
	}
	res[i_tab] = NULL;
	return (res);
}

/*********************************
 *                              *
 *      June SubFunctions       *
 *                              *
*********************************/

char *jsf_find(int lnb, char **argv) {
    (void)lnb;
    // usage: find <dir> <pattern>
    for (int i = 0; argv[i]; i++)
        printf("  %s\n", argv[i]);
    return strdup("abc");
}

char *jsf_nick(int lnb, char **argv) {
    (void)lnb;
    // usage: nick [-e ext] [-d parent] <name>
    for (int i = 0; argv[i]; i++)
        printf("  %s\n", argv[i]);
    return strdup("def");
}

jsf_t g_jsf[] = {
    {"find", jsf_find},
    {"nick", jsf_nick},
    {NULL, NULL}
};

void *get_jsf(char *name) {
    for (int i = 0; g_jsf[i].name; i++) {
        if (!strcmp(g_jsf[i].name, name))
            return g_jsf[i].func;
    }
    return NULL;
}

/*********************************
 *                              *
 *     File Interpretation      *
 *                              *
*********************************/

int tream_line(char *sline, char **line) {
    int indent;
    char *tmp;

    *line = sline;

    *line = str_triml(*line);
    indent = *line != sline;

    if ((tmp = strchr(*line, '/')) && tmp[1] == '/')
        *tmp = '\0';

    if ((tmp = strchr(*line, '#')))
        *tmp = '\0';

    str_trim(*line);

    // replace white spaces with ' '
    for (int i = 0; (*line)[i]; i++) {
        if (isspace((*line)[i]))
            (*line)[i] = ' ';
    }

    return indent;
}

char *expand_vars(char *src, int lnb) {
    char *line = strdup(src);
    char *value;

    for (int i = 0; line[i]; i++) {
        if (line[i] != '$')
            continue;

        int start = ++i;

        if (line[i] == '\0') {
            fprintf(stderr, "June: line %d: Invalid variable name\n", lnb);
            free(line);
            return NULL;
        }

        if (line[i] == '$') {
            char *tmp = malloc(strlen(line));
            strncpy(tmp, line, i - 1);
            strcpy(tmp + i - 1, line + i + 1);
            free(line);
            line = tmp;
            continue;
        }

        if (line[i] == '[') {
            char *tmp, *subfunc, **args;

            // find the closing bracket
            int count = 1;
            int j = i + 1;
            while (line[j] && count) {
                if (line[j] == '[')
                    count++;
                else if (line[j] == ']')
                    count--;
                j++;
            }

            if (count) {
                fprintf(stderr, "June: line %d: Invalid subfunction\n", lnb);
                free(line);
                return NULL;
            }

            tmp = strndup(line + i + 1, j - i - 2); // cause malloc is cool
            subfunc = expand_vars(str_triml(str_trim(tmp)), lnb);
            free(tmp);
            args = str_split(subfunc, ' ');
            free(subfunc);

            if (!args) {
                fprintf(stderr, "June: line %d: Invalid subfunction\n", lnb);
                free(line);
                return NULL;
            }

            char *(*func)(int, char **) = get_jsf(args[0]);

            if (!func) {
                fprintf(stderr, "June: line %d: '%s': Subfunction not found\n", lnb, args[0]);
                for (int j = 0; args[j]; j++)
                    free(args[j]);
                free(args);
                free(line);
                return NULL;
            }

            value = func(lnb, args);

            if (!value) {
                fprintf(stderr, "June: line %d: '%s': Subfunction failed\n", lnb, args[0]);
                for (int j = 0; args[j]; j++)
                    free(args[j]);
                free(args);
                free(line);
                return NULL;
            }

            for (int j = 0; args[j]; j++)
                free(args[j]);
            free(args);

            int len = strlen(value);
            char *tmp2 = malloc(strlen(line) + len);
            strncpy(tmp2, line, start - 1);
            strcpy(tmp2 + start - 1, value);

            strcpy(tmp2 + start - 1 + len, line + j);
            free(value);
            free(line);

            i = start + len - 2;
            line = tmp2;
            continue;
        }

        while (line[i] && isalnum(line[i]))
            i++;

        if (i == start) {
            fprintf(stderr, "June: line %d: Invalid variable name\n", lnb);
            free(line);
            return NULL;
        }

        char *name = strndup(line + start, i - start);

        if (strcmp(name, "0") == 0) {
            free(name);
            continue;
        } else if ((value = get_var(name))) {
            int len = strlen(value);
            char *tmp = malloc(strlen(line) + len);
            strncpy(tmp, line, start - 1);
            strcpy(tmp + start - 1, value);
            strcpy(tmp + start - 1 + len, line + i);
            free(line);
            free(name);
            i = start + len - 2;
            line = tmp;
        } else {
            fprintf(stderr, "June: line %d: %s: Undefined variable\n", lnb, name);
            free(name);
            free(line);
            return NULL;
        }
    }

    return line;
}

int compute_patern(char *name, int lnb, char **src_ext, char **dst_ext) {
    // name: src_ext -> dst_ext || dst_ext <- src_ext
    char *tmp;

    if ((tmp = strstr(name, "->"))) {
        *tmp = '\0';
        *src_ext = strdup(str_trim(name));
        *dst_ext = strdup(str_triml(tmp + 2));
    } else if ((tmp = strstr(name, "<-"))){
        *tmp = '\0';
        *dst_ext = strdup(str_trim(name));
        *src_ext = strdup(str_triml(tmp + 2));
    } else {
        fprintf(stderr, "June: line %d: '%s': Invalid patern\n", lnb, name);
        return 1;
    }

    if (!is_valid_filename(*src_ext)) {
        fprintf(stderr, "June: line %d: '%s': Invalid source extension\n", lnb, *src_ext);
        free(*src_ext);
        free(*dst_ext);
        return 1;
    }

    if (!is_valid_filename(*dst_ext)) {
        fprintf(stderr, "June: line %d: '%s': Invalid destination extension\n", lnb, *dst_ext);
        free(*src_ext);
        free(*dst_ext);
        return 1;
    }

    return 0;
}

int interp_file(FILE *f) {
    char *line, *sline = NULL;
    rule_t *rule = NULL;
    int indent;
    int lnb = 0;

    while ((free(sline), lnb++, sline = read_line(f))) {
        indent = tream_line(sline, &line);

        if (*line == '\0')
            continue;

        line = expand_vars(line, lnb);

        if (!line) {
            free(sline);
            return 1;
        }

        if (indent == 0) {
            char *tmp;
            if ((tmp = strchr(line, '='))) {
                *tmp = '\0';
                char *name = strdup(str_trim(line));
                if (!is_valid_varname(name)) {
                    fprintf(stderr, "June: line %d: '%s': Invalid variable name\n", lnb, name);
                    free(sline);
                    free(line);
                    free(name);
                    return 1;
                }
                set_var(name, strdup(str_triml(tmp + 1)));
                rule = NULL;
            } else if ((tmp = strchr(line, ':'))) {
                *tmp = '\0';
                char *src_ext, *dst_ext, *name = str_trim(line);
                int is_patern = name[0] == '[' && name[strlen(name) - 1] == ']';

                if (is_patern) {
                    tmp--;
                    *tmp = '\0';
                    if (compute_patern(str_triml(str_trim(++name)), lnb, &src_ext, &dst_ext)) {
                        free(sline);
                        free(line);
                        return 1;
                    }
                } else {
                    if (!is_valid_filename(name)) {
                        fprintf(stderr, "June: line %d: '%s': Invalid rule name\n", lnb, name);
                        free(sline);
                        free(line);
                        return 1;
                    }
                    name = strdup(name);
                }

                char **deps = str_split(tmp + 1, ' ');
                for (int i = 0; deps[i]; i++) {
                    if (!is_valid_filename(deps[i])) {
                        fprintf(stderr, "June: line %d: '%s': Invalid dependency name\n", lnb, deps[i]);
                        if (is_patern) {
                            free(src_ext);
                            free(dst_ext);
                        } else {
                            free(name);
                        }
                        for (int j = 0; deps[j]; j++)
                            free(deps[j]);
                        free(sline);
                        free(line);
                        free(deps);
                        return 1;
                    }
                }

                rule = g_rules;
                while (rule->name)
                    rule++;
                rule->is_patern = is_patern;
                if (is_patern) {
                    rule->patern.src_ext = src_ext;
                    rule->patern.dst_ext = dst_ext;
                } else {
                    rule->name = name;
                }
                rule->deps = deps;
                rule->cmds = NULL;
            } else {
                fprintf(stderr, "June: line %d: Invalid statement\n", lnb);
                free(sline);
                free(line);
                return 1;
            }
            free(line);
        } else {
            if (!rule) {
                fprintf(stderr, "June: line %d: Command without rule\n", lnb);
                free(sline);
                free(line);
                return 1;
            }

            int count = 0;

            if (!rule->cmds) {
                rule->cmds = malloc(sizeof(char *) * 2);
            } else {
                for (count = 0; rule->cmds[count]; count++);
                rule->cmds = realloc(rule->cmds, sizeof(char *) * (count + 2));
            }

            rule->cmds[count] = line;
            rule->cmds[count + 1] = NULL;
        }
    }

    return 0;
}

/*********************************
 *                              *
 *        Rule Execution        *
 *                              *
*********************************/

char *expend_var0(char *line, char *val) {
    char *tmp, *start = strstr(line, "$0");

    if (!start) {
        return line;
    }

    tmp = malloc(strlen(line) + strlen(val));
    strncpy(tmp, line, start - line);
    strcpy(tmp + (start - line), val);
    strcpy(tmp + (start - line) + strlen(val), start + 2);

    free(line);
    return expend_var0(tmp, val);
}

int is_up_to_date(char *name, rule_t *rule) {
    if (g_opt.virtual) {
        return 0;
    }

    if (!rule->is_patern && !file_exists(name)) {
        return 0;
    }

    for (int i = 0; rule->deps[i]; i++) {
        if (!file_exists(rule->deps[i])) {
            return 0;
        }
        if (file_last_modif(name) < file_last_modif(rule->deps[i])) {
            return 0;
        }
    }

    if (!rule->is_patern) {
        printf("June: %s: Up to date\n", name);
        return 1;
    }

    char *noext = rm_ext(name);
    char *in = malloc(strlen(noext) + strlen(rule->patern.src_ext) + 2);
    char *out = malloc(strlen(noext) + strlen(rule->patern.dst_ext) + 2);

    sprintf(in, "%s.%s", noext, rule->patern.src_ext);
    sprintf(out, "%s.%s", noext, rule->patern.dst_ext);

    if (!file_exists(in) || !file_exists(out) || file_last_modif(in) > file_last_modif(out)) {
        free(noext);
        free(in);
        free(out);
        return 0;
    }

    printf("June: %s: Up to date\n", out);
    free(noext);
    free(in);
    free(out);

    return 1;
}

int exec_rule_rec(rule_t *rule, int depth, char *fname) {
    /*
    for (int j = 0; j < depth; j++)
        putchar(' ');
    if (rule->is_patern)
        printf("[%s.%s -> %s.%s]\n", fname, rule->patern.src_ext, fname, rule->patern.dst_ext);
    else
        printf("[%s]\n", rule->name);
    */

    if (depth > 100) {
        fprintf(stderr, "June: %s: Recursion limit reached\n", rule->name);
        return 1;
    }

    for (int i = 0; rule->deps[i]; i++) {
        rule_t *dep = NULL;
        for (int j = 0; g_rules[j].name; j++) {
            if (!g_rules[j].is_patern && !strcmp(g_rules[j].name, rule->deps[i])) {
                dep = g_rules + j;
                break;
            }
        }

        if (dep) {
            if (exec_rule_rec(dep, depth + 1, dep->name))
                return 1;
            continue;
        }

        char *noext = rm_ext(rule->deps[i]);

        // search for patern
        for (int j = 0; g_rules[j].name; j++) {
            if (g_rules[j].is_patern &&
                    is_right_ext(rule->deps[i], g_rules[j].patern.dst_ext)
            ) {
                char *src = malloc(strlen(noext) + strlen(g_rules[j].patern.src_ext) + 2);
                sprintf(src, "%s.%s", noext, g_rules[j].patern.src_ext);

                if (file_exists(src)) {
                    dep = g_rules + j;
                    free(src);
                    break;
                }
                free(src);
            }
        }

        if (!dep) {
            fprintf(stderr, "June: %s: %s: Rule not found\n", rule->name, rule->deps[i]);
            free(noext);
            return 1;
        }

        if (exec_rule_rec(dep, depth + 1, noext)) {
            free(noext);
            return 1;
        }

        free(noext);
    }

    if (is_up_to_date(fname, rule)) {
        return 0;
    }

    if (!rule->cmds) {
        printf("  No commands\n");
        return 0;
    }

    for (int i = 0; rule->cmds[i]; i++) {
        char *cmd = expend_var0(strdup(rule->cmds[i]), fname);
        printf("%s\n", cmd);
        if (system(cmd)) {
            fprintf(stderr, "June: %s: Command failed\n", rule->name);
            free(cmd);
            return 1;
        }
        free(cmd);
    }

    return 0;
}

int exec_rule(char *name) {
    rule_t *rule;

    if (!name) {
        rule = g_rules;
        while (rule->is_patern)
            rule++;
        if (!rule->name) {
            fprintf(stderr, "June: No default rule found\n");
            return 1;
        }
    } else {
        rule = NULL;
        for (int i = 0; g_rules[i].name; i++) {
            if (!strcmp(g_rules[i].name, name) && !g_rules[i].is_patern) {
                rule = g_rules + i;
                break;
            }
        }
        if (!rule) {
            fprintf(stderr, "June: '%s': Rule not found\n", name);
            return 1;
        }
    }

    return exec_rule_rec(rule, 0, rule->name);
}

/*********************************
 *                              *
 *        User Interface        *
 *                              *
*********************************/

void print_help(void) {
    puts(JUNE_USAGE "Options:\n"
        "  -h    Print this message\n"
        "  -v    Print version\n"
        "  -n    Do not use file system\n"
        "  -f    Specify the file to interpret\n"
        "  -d    Print debug informations\n"
    );
}

void paseargs(int argc, char **argv) {
    int i = 1;

    memset(&g_opt, 0, sizeof(juneopt_t));

    while (i < argc) {
        if (argv[i][0] != '-') {
            break;
        }

        if (strlen(argv[i]) != 2) {
            fprintf(stderr, "June: Invalid option %s\n" JUNE_USAGE, argv[i]);
            exit(1);
        }

        switch (argv[i][1]) {
            case 'h':
                print_help();
                exit(0);
            case 'v':
                puts(JUNE_VERSION);
                exit(0);
            case 'n':
                g_opt.virtual = 1;
                break;
            case 'd':
                g_opt.debug = 1;
                break;
            case 'f':
                if (i + 1 >= argc) {
                    fprintf(stderr, "June: Missing argument for option 'f'\n" JUNE_USAGE);
                    exit(1);
                }
                if (g_opt.file) {
                    fprintf(stderr, "June: File already specified\n" JUNE_USAGE);
                    exit(1);
                }
                g_opt.file = argv[++i];
                break;
            default:
                fprintf(stderr, "June: Invalid option -- '%c'\n" JUNE_USAGE, argv[i][1]);
                exit(1);
        }

        i++;
    }

    if (g_opt.file == NULL)
        g_opt.file = JUNE_FILE;
    g_opt.rules = argv + i;
}

#define main_error() {ret = 1; goto main_end;}

int main(int argc, char **argv) {
    paseargs(argc, argv);

    g_rules = calloc(MAX_RULES, sizeof(rule_t));
    g_vars = calloc(MAX_VARS, sizeof(var_t));

    FILE *f = chdir_and_open(g_opt.file, "r");

    int ret = 0;

    if (!f) {
        printf("June: %s: Failed to open file\n", g_opt.file);
        main_error();
    }

    if (interp_file(f)) {
        fclose(f);
        main_error();
    }

    fclose(f);

    if (g_opt.debug) {
        fprintf(stderr, "============ Variables ============\n\n");
        for (int i = 0; g_vars[i].name; i++)
            fprintf(stderr, "%s\t= %s\n", g_vars[i].name, g_vars[i].value);
        fprintf(stderr, "\n============ Rules ============\n\n");
        for (int i = 0; g_rules && g_rules[i].name; i++)
            print_rule(g_rules + i);
        fprintf(stderr, "================================\n\n");
    }

    if (*g_opt.rules) {
        for (int i = 0; g_opt.rules[i]; i++)
            if (exec_rule(g_opt.rules[i]))
                main_error();
    }

    else if (exec_rule(NULL))
        main_error();

    main_end:

    free_globals();
    free(g_rules);
    free(g_vars);

    return ret;
}
