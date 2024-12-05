#include <stdlib.h>
#include <stdio.h>

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

rule_t *g_rules = NULL;

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

int main(void) {
    FILE *f = fopen("build.jn", "r");

    if (!f) {
        printf("Error: Could not open file\n");
        return 1;
    }

    parse_file(f);
    fclose(f);

    for (int i = 0; g_rules[i]; i++)
        print_rule(g_rules[i]);
    for (int i = 0; g_vars[i]; i++)
        printf("VAR: '%s' = '%s'\n", g_vars[i]->name, g_vars[i]->value);

    return 0;
}
