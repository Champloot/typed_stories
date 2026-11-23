// lexicon: lexicon.lex: intents and the phrases that express them
#include <errno.h>

#include "lexicon.h"
#include "util.h"

Intent *lex;
static int     nlex, lexcap;

const Intent *find_intent(const char *name) {
    for (int i = 0; i < nlex; i++)
        if (!strcmp(lex[i].name, name)) return &lex[i];
    return NULL;
}

// Every intent needs phrases and a hint. Typing the hint itself also works:
// it is the plainest possible answer, so it counts as tier 1.
static void finish_intent(const char *path, int line, Intent *in) {
    if (in->n == 0) die_at(path, line, "intent '%s' has no phrases", in->name);
    if (!in->hint)  die_at(path, line, "intent '%s' has no 'hint'", in->name);
    char norm[LINE_BUF];
    normalize(in->hint, norm, sizeof norm);
    phrase_push(&in->ph, &in->n, &in->cap, norm, 1);
}

// lexicon.lex lives next to the packs. Without it every choice is a literal phrase.
void load_lexicon(const char *dir) {
    char path[1024];
    snprintf(path, sizeof path, "%s/lexicon.lex", dir);
    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno == ENOENT) return;
        fprintf(stderr, "cannot open '%s': %s\n", path, strerror(errno));
        exit(1);
    }
    char buf[LINE_BUF], *key, *val;
    int lineno = 0;
    Intent *cur = NULL;
    while (next_kv(f, path, &lineno, buf, &key, &val)) {
        if (!strcmp(key, "intent")) {
            if (cur) finish_intent(path, lineno, cur);
            if (!name_ok(val)) die_at(path, lineno, "intent name must use lowercase letters, digits and '_'");
            if (find_intent(val)) die_at(path, lineno, "intent '%s' is defined twice", val);
            if (nlex == lexcap) {
                lexcap = lexcap ? lexcap * 2 : 32;
                lex = xrealloc(lex, (size_t)lexcap * sizeof *lex);
            }
            cur = &lex[nlex++];
            memset(cur, 0, sizeof *cur);
            cur->name = xstrdup(val);
        } else if (!strcmp(key, "1") || !strcmp(key, "2") || !strcmp(key, "3")) {
            if (!cur) die_at(path, lineno, "phrases before the first 'intent:'");
            int tier = key[0] - '0';
            char *parts[MAX_SPLIT + 1];
            int n = split_on(val, '|', parts, MAX_SPLIT);
            if (n > MAX_SPLIT) die_at(path, lineno, "too many phrases on one line (max %d)", MAX_SPLIT);
            for (int i = 0; i < n; i++) {
                char norm[LINE_BUF];
                normalize(parts[i], norm, sizeof norm);
                if (!*norm) die_at(path, lineno, "empty phrase (check the '|' signs)");
                for (int j = 0; j < cur->n; j++)
                    if (!strcmp(cur->ph[j].text, norm))
                        die_at(path, lineno, "phrase '%s' is listed twice in intent '%s'", parts[i], cur->name);
                phrase_push(&cur->ph, &cur->n, &cur->cap, norm, tier);
            }
        } else if (!strcmp(key, "hint")) {
            if (!cur) die_at(path, lineno, "'hint' before the first 'intent:'");
            if (cur->hint) die_at(path, lineno, "second 'hint' in intent '%s'", cur->name);
            char norm[LINE_BUF];
            normalize(val, norm, sizeof norm);
            if (!*norm) die_at(path, lineno, "empty hint");
            cur->hint = xstrdup(val);
        } else {
            die_at(path, lineno, "unknown key '%s' (expected 'intent', 'hint', '1', '2' or '3')", key);
        }
    }
    fclose(f);
    if (cur) finish_intent(path, lineno, cur);
}

