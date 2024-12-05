#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

rule_t *g_rules;
var_t *g_vars;

#define MAX_RULES 256
#define MAX_VARS  1024

char *get_var(char *name) {
    for (int i = 0; g_vars && g_vars[i].name; i++) {
        if (!strcmp(g_vars[i].name, name))
            return g_vars[i].value;
    }

    return NULL;
}

int set_var(char *name, char *value) {
    for (int i = 0; g_vars && g_vars[i].name; i++) {
        if (!strcmp(g_vars[i].name, name)) {
            free(g_vars[i].value);
            g_vars[i].value = strdup(value);
            return 0;
        }
    }

    for (int i = 0; g_vars && g_vars[i].name; i++) {
        if (!g_vars[i].name) {
            g_vars[i].name = strdup(name);
            g_vars[i].value = strdup(value);
            return 0;
        }
    }

    return 1;
}

void print_rule(rule_t *rule) {
    if (rule->is_patern) {
        printf("RULE: %s -> %s\n", rule->patern.src_ext, rule->patern.dst_ext);
    } else {
        printf("RULE: %s\n", rule->name);
    }

    for (int i = 0; rule->deps[i]; i++)
        printf("  dep %d: %s\n", i, rule->deps[i]);

    for (int i = 0; rule->cmds[i]; i++)
        printf("  cmd %d: %s\n", i, rule->cmds[i]);

    printf("\n\n");
}

bool tream_line(char *sline, char **line) {
    bool indent;
    char *tmp;

    *line = sline;

    indent = false;
    while (isspace(**line)) {
        indent = true;
        (*line)++;
    }
    
    int len = strlen(*line);
    while (len > 0 && isspace((*line)[len - 1]))
        (*line)[--len] = '\0';

    if ((tmp = strchr(*line, '/')) && tmp[1] == '/')
        *tmp = '\0';

    if ((tmp = strchr(*line, '#')))
        *tmp = '\0';

    return indent;
}

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

        if ((value = get_var(name))) {
            char *tmp = malloc(strlen(line) + strlen(value));
            strncpy(tmp, line, start - 2);
            strcpy(tmp + start - 1, value);
            strcpy(tmp + start - 1 + strlen(value), line + i);
            free(line);
            line = tmp;
            free(name);
        } else {
            printf("June: line %d: %s: Undefined variable\n", lnb, name);
            free(name);
            free(line);
            return NULL;
        }        
    }

    return line;
}

int interp_file(FILE *f) {
    char *line, *sline = NULL;
    bool indent;
    int lnb = 0;

    while ((free(sline), lnb++, sline = read_line(f))) {
        indent = tream_line(sline, &line);

        if (*line == '\0')
            continue;

        printf("%d: %s\n", indent, line);

        if (indent == false) {
            line = expand_vars(line, lnb);
            if (!line) {
                free(sline);
                return 1;
            }
            printf("-> %s\n", line);
            free(line);
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: june <file>\n");
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
        free(g_rules);
        free(g_vars);
        return 1;
    }

    fclose(f);

    for (int i = 0; g_rules && g_rules[i].name; i++)
        print_rule(g_rules + i);

    for (int i = 0; g_vars && g_vars[i].name; i++)
        printf("VAR: '%s' = '%s'\n", g_vars[i].name, g_vars[i].value);

    free(g_rules);
    free(g_vars);

    return 0;
}
