// util: small helpers: memory, text normalization, file parsing, error reporting
#ifndef UTIL_H
#define UTIL_H

#include "common.h"

double now(void);
void shuffle(int *a, int n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
void normalize(const char *in, char *out, size_t cap);
void phrase_push(Phrase **arr, int *n, int *cap, const char *text, int tier);
__attribute__((format(printf, 3, 4), noreturn))
void die_at(const char *file, int line, const char *fmt, ...);
char *trim(char *s);
int split_on(char *s, char sep, char **parts, int max);
int all_digits(const char *s);
int name_ok(const char *s);
int parse_int(const char *s, int lo, int hi, int *out);
int next_kv(FILE *f, const char *path, int *lineno, char *buf, char **key, char **val);

#endif
