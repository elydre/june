#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#define VIRTUAL 1

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

rule_t *g_rules;
var_t *g_vars;

#define MAX_RULES 256
#define MAX_VARS  1024

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
        printf("RULE: %s -> %s\n", rule->patern.src_ext, rule->patern.dst_ext);
    } else {
        printf("RULE: %s\n", rule->name);
    }

    for (int i = 0; rule->deps[i]; i++)
        printf("  dep %d: %s\n", i, rule->deps[i]);

    for (int i = 0; rule->cmds && rule->cmds[i]; i++)
        printf("  cmd %d: %s\n", i, rule->cmds[i]);

    printf("\n");
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
    if (!VIRTUAL && access(name, F_OK) == -1)
        return 0;
    return 1;
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

char *jsf_find(int argc, char **argv) {
    // usage: find <dir> <pattern>
    printf("find: %d\n", argc);
    for (int i = 0; i < argc; i++)
        printf("  %s\n", argv[i]);
    return strdup("find");
}

char *jsf_nick(int argc, char **argv) {
    // usage: nick [-e ext] [-d parent] <name>
    printf("nick: %d\n", argc);
    for (int i = 0; i < argc; i++)
        printf("  %s\n", argv[i]);
    return strdup("nick");
}

jsf_t g_jsf[] = {
    {"find", jsf_find},
    {"nick", jsf_nick},
    {NULL, NULL}
};

/*********************************
 *                              *
 *     File Interpretation      *
 *                              *
*********************************/

bool tream_line(char *sline, char **line) {
    bool indent;
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
            printf("June: line %d: Invalid variable name\n", lnb);
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
            // todo
            continue;
        }

        while (line[i] && isalnum(line[i]))
            i++;

        if (i == start) {
            printf("June: line %d: Invalid variable name\n", lnb);
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
            printf("June: line %d: %s: Undefined variable\n", lnb, name);
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
        printf("June: line %d: '%s': Invalid patern\n", lnb, name);
        return 1;
    }

    if (!is_valid_filename(*src_ext)) {
        printf("June: line %d: '%s': Invalid source extension\n", lnb, *src_ext);
        free(*src_ext);
        free(*dst_ext);
        return 1;
    }

    if (!is_valid_filename(*dst_ext)) {
        printf("June: line %d: '%s': Invalid destination extension\n", lnb, *dst_ext);
        free(*src_ext);
        free(*dst_ext);
        return 1;
    }

    return 0;
}

int interp_file(FILE *f) {
    char *line, *sline = NULL;
    rule_t *rule = NULL;
    bool indent;
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

        if (indent == false) {
            printf("%s\n", line);

            char *tmp;
            if ((tmp = strchr(line, '='))) {
                *tmp = '\0';
                char *name = strdup(str_trim(line));
                if (!is_valid_varname(name)) {
                    printf("June: line %d: '%s': Invalid variable name\n", lnb, name);
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
                        printf("June: line %d: '%s': Invalid rule name\n", lnb, name);
                        free(sline);
                        free(line);
                        return 1;
                    }
                    name = strdup(name);
                }

                char **deps = str_split(tmp + 1, ' ');
                for (int i = 0; deps[i]; i++) {
                    if (!is_valid_filename(deps[i])) {
                        printf("June: line %d: '%s': Invalid dependency name\n", lnb, deps[i]);
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
                printf("June: line %d: Invalid statement\n", lnb);
                free(sline);
                free(line);
                return 1;
            }
            free(line);
        } else {
            printf("   %s\n", line);
            if (!rule) {
                printf("June: line %d: Command without rule\n", lnb);
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

int exec_rule_rec(rule_t *rule, int depth, char *fname) {
    for (int j = 0; j < depth; j++)
        putchar(' ');
    if (rule->is_patern)
        printf("[%s.%s -> %s.%s]\n", fname, rule->patern.src_ext, fname, rule->patern.dst_ext);
    else
        printf("[%s]\n", rule->name);

    if (depth > 100) {
        printf("June: %s: Recursion limit reached\n", rule->name);
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
                char *src = malloc(strlen(noext) + strlen(g_rules[j].patern.src_ext) + 1);
                strcpy(src, noext);
                strcat(src, g_rules[j].patern.src_ext);

                if (file_exists(src)) {
                    dep = g_rules + j;
                    free(src);
                    break;
                }

                free(src);
            }
        }

        if (!dep) {
            printf("June: %s: Dependency '%s' not found\n", rule->name, rule->deps[i]);
            free(noext);
            return 1;
        }

        if (exec_rule_rec(dep, depth + 1, noext)) {
            free(noext);
            return 1;
        }

        free(noext);
    }

    if (!rule->cmds) {
        printf("  No commands\n");
        return 0;
    }

    for (int i = 0; rule->cmds[i]; i++) {
        for (int j = 0; j < depth; j++)
            putchar(' ');
        char *cmd = expend_var0(strdup(rule->cmds[i]), fname);
        printf("%s\n", cmd);
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
            printf("June: No default rule found\n");
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
            printf("June: '%s': Rule not found\n", name);
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

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        printf("Usage: june <file> [rule]\n");
        return 1;
    }

    g_rules = calloc(MAX_RULES, sizeof(rule_t));
    g_vars = calloc(MAX_VARS, sizeof(var_t));

    FILE *f = fopen(argv[1], "r");

    if (!f) {
        printf("June: %s: Failed to open file\n", argv[1]);
        free(g_rules);
        free(g_vars);
        return 1;
    }

    if (interp_file(f)) {
        fclose(f);
        free_globals();
        free(g_rules);
        free(g_vars);
        return 1;
    }

    fclose(f);

    printf("\n============ Rules ============\n");
    for (int i = 0; g_rules && g_rules[i].name; i++)
        print_rule(g_rules + i);
    printf("================================\n\n");

    exec_rule(argv[2]);

    free_globals();
    free(g_rules);
    free(g_vars);

    return 0;
}
