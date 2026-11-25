// daily: today's date and the word of the day (words.lst)
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "daily.h"
#include "pack.h"
#include "rng.h"
#include "util.h"

static char **words;
static int    nwords, wordcap;

Date today(void) {
    Date d;
    struct tm tmv;
    memset(&tmv, 0, sizeof tmv);
    const char *o = getenv("TYPED_STORIES_DATE");
    int y, m, dd;
    if (o && sscanf(o, "%d-%d-%d", &y, &m, &dd) == 3) {
        tmv.tm_year = y - 1900;
        tmv.tm_mon = m - 1;
        tmv.tm_mday = dd;
        tmv.tm_hour = 12;
        mktime(&tmv);
    } else {
        time_t t = time(NULL);
#ifdef _WIN32
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
    }
    strftime(d.iso, sizeof d.iso, "%Y-%m-%d", &tmv);
    strftime(d.pretty, sizeof d.pretty, "%a %d %b %Y", &tmv);
    return d;
}

// Is w one of the space-separated tokens of phrase?
static int has_token(const char *phrase, const char *w) {
    size_t n = strlen(w);
    for (const char *p = phrase; (p = strstr(p, w)); p++)
        if ((p == phrase || p[-1] == ' ') && (p[n] == '\0' || p[n] == ' ')) return 1;
    return 0;
}

static const char *used_in_answers(const char *w) {
    for (int i = 0; i < npacks; i++)
        for (int s = 0; s < packs[i].count; s++) {
            const Scenario *sc = &packs[i].scen[s];
            for (int k = 0; k < sc->nvariants; k++)
                for (int c = 0; c < sc->variants[k].nchoices; c++) {
                    const Choice *ch = &sc->variants[k].choices[c];
                    for (int p = 0; p < ch->nph; p++)
                        if (has_token(ch->ph[p].text, w)) return ch->ph[p].text;
                }
        }
    return NULL;
}

void load_words(const char *dir) {
    char path[1024];
    snprintf(path, sizeof path, "%s/words.lst", dir);
    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno == ENOENT) return;
        fprintf(stderr, "cannot open '%s': %s\n", path, strerror(errno));
        exit(1);
    }
    char buf[LINE_BUF];
    int lineno = 0;
    while (fgets(buf, sizeof buf, f)) {
        lineno++;
        char *w = trim(buf);
        if (!*w || *w == '#') continue;
        size_t n = strlen(w);
        int ok = n >= 3 && n <= 20;
        for (const char *p = w; *p; p++)
            if (!islower((unsigned char)*p)) ok = 0;
        if (!ok) die_at(path, lineno, "a word of the day is 3 to 20 lowercase letters, one per line");
        for (int i = 0; i < nwords; i++)
            if (!strcmp(words[i], w)) die_at(path, lineno, "'%s' is listed twice", w);
        const char *ph = used_in_answers(w);
        if (ph) die_at(path, lineno, "'%s' is part of an answer (\"%s\"), pick another word", w, ph);
        if (nwords == wordcap) {
            wordcap = wordcap ? wordcap * 2 : 32;
            words = xrealloc(words, (size_t)wordcap * sizeof *words);
        }
        words[nwords++] = xstrdup(w);
    }
    fclose(f);
}

const char *word_of_day(const Date *d) {
    if (!nwords) return NULL;
    return words[rng_mix(rng_hash(d->iso)) % (uint64_t)nwords];
}
