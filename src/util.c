// util: small helpers: memory, text normalization, file parsing, error reporting
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#include "util.h"

double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void shuffle(int *a, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1), t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) { fputs("out of memory\n", stderr); exit(1); }
    return q;
}

char *xstrdup(const char *s) {
    char *d = strdup(s);
    if (!d) { fputs("out of memory\n", stderr); exit(1); }
    return d;
}

// Lowercase, drop apostrophes, turn other punctuation into spaces, collapse spaces:
// "I'm good!" -> "im good". Phrases from files and the player's input both go through it.
void normalize(const char *in, char *out, size_t cap) {
    size_t n = 0;
    int space = 0;
    for (; *in && n + 1 < cap; in++) {
        unsigned char c = (unsigned char)*in;
        if (c == '\'') continue;
        if (isalnum(c)) {
            if (space && n > 0) {
                if (n + 2 >= cap) break;
                out[n++] = ' ';
            }
            space = 0;
            out[n++] = (char)tolower(c);
        } else {
            space = 1;
        }
    }
    out[n] = '\0';
}

// Adds a phrase to a list; a duplicate keeps the higher tier.
void phrase_push(Phrase **arr, int *n, int *cap, const char *text, int tier) {
    for (int i = 0; i < *n; i++)
        if (!strcmp((*arr)[i].text, text)) {
            if (tier > (*arr)[i].tier) (*arr)[i].tier = tier;
            return;
        }
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *arr = xrealloc(*arr, (size_t)*cap * sizeof **arr);
    }
    (*arr)[*n].text = xstrdup(text);
    (*arr)[*n].tier = tier;
    (*n)++;
}

__attribute__((format(printf, 3, 4), noreturn))
void die_at(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

// Trims spaces/tabs/newlines on both ends, in place.
char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
    return s;
}

// Splits s on sep in place and trims each part. Returns the number of parts;
// max + 1 means there were more than max.
int split_on(char *s, char sep, char **parts, int max) {
    int n = 0;
    while (n < max) {
        char *bar = strchr(s, sep);
        if (bar) *bar = '\0';
        parts[n++] = trim(s);
        if (!bar) return n;
        s = bar + 1;
    }
    return n + 1;
}

static int is_ascii_text(const char *s) {
    for (; *s; s++)
        if ((unsigned char)*s > 126 || ((unsigned char)*s < 32 && *s != '\t' && *s != '\n' && *s != '\r'))
            return 0;
    return 1;
}

int all_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

int name_ok(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!(islower((unsigned char)*s) || isdigit((unsigned char)*s) || *s == '_')) return 0;
    return 1;
}

int parse_int(const char *s, int lo, int hi, int *out) {
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s || *end || errno || v < lo || v > hi) return 0;
    *out = (int)v;
    return 1;
}

// Reads one non-empty, non-comment line as key + value. Returns 0 at end of file.
int next_kv(FILE *f, const char *path, int *lineno, char *buf, char **key, char **val) {
    while (fgets(buf, LINE_BUF, f)) {
        (*lineno)++;
        if (!strchr(buf, '\n') && !feof(f))
            die_at(path, *lineno, "line too long (max %d characters)", LINE_BUF - 2);
        char *line = trim(buf);
        if (*line == '\0' || *line == '#') continue;
        if (!is_ascii_text(line))
            die_at(path, *lineno, "only plain ASCII text is supported for now (no Cyrillic, no fancy quotes or dashes)");
        char *colon = strchr(line, ':');
        if (!colon) die_at(path, *lineno, "expected 'key: value'");
        *colon = '\0';
        *key = trim(line);
        *val = trim(colon + 1);
        return 1;
    }
    return 0;
}

