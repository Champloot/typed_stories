// records: best results per pack and per date, and the days the word of the day was used
#include "records.h"
#include "util.h"

/* records.txt, one record per line, fields separated by tabs:
     best  <pack>  <score> <rating> <style>
     day   <date>  <pack>  <score>  <rating> <style>
     word  <date>                                       */

typedef struct {
    char   kind;        // 'b' best, 'd' day, 'w' word
    char  *date;        // "" for 'b'
    char  *pack;        // "" for 'w'
    Result r;
} Rec;

static Rec *recs;
static int  nrecs, reccap;
static char rec_file[1024];

static Rec *find(char kind, const char *date, const char *pack) {
    for (int i = 0; i < nrecs; i++)
        if (recs[i].kind == kind && !strcmp(recs[i].date, date) && !strcmp(recs[i].pack, pack)) return &recs[i];
    return NULL;
}

static Rec *add(char kind, const char *date, const char *pack) {
    if (nrecs == reccap) {
        reccap = reccap ? reccap * 2 : 32;
        recs = xrealloc(recs, (size_t)reccap * sizeof *recs);
    }
    Rec *r = &recs[nrecs++];
    memset(r, 0, sizeof *r);
    r->kind = kind;
    r->date = xstrdup(date);
    r->pack = xstrdup(pack);
    return r;
}

void records_load(void) {
    if (!data_path(rec_file, sizeof rec_file, "records.txt")) return;
    FILE *f = fopen(rec_file, "r");
    if (!f) return;
    char line[LINE_BUF];
    while (fgets(line, sizeof line, f)) {
        char *fl[7];
        int n = split_on(trim(line), '\t', fl, 6);
        Result r = {0, 0, 0};
        if (n == 5 && !strcmp(fl[0], "best")
            && parse_int(fl[2], -1000000, 1000000, &r.score) && parse_int(fl[3], 0, 10, &r.rating)
            && parse_int(fl[4], 0, 1000000, &r.style)) {
            if (!find('b', "", fl[1])) add('b', "", fl[1])->r = r;
        } else if (n == 6 && !strcmp(fl[0], "day")
            && parse_int(fl[3], -1000000, 1000000, &r.score) && parse_int(fl[4], 0, 10, &r.rating)
            && parse_int(fl[5], 0, 1000000, &r.style)) {
            if (!find('d', fl[1], fl[2])) add('d', fl[1], fl[2])->r = r;
        } else if (n == 2 && !strcmp(fl[0], "word")) {
            if (!find('w', fl[1], "")) add('w', fl[1], "");
        }
    }
    fclose(f);
}

// Best effort, like style: without a writable data folder records are not kept.
void records_save(void) {
    if (!*rec_file) return;
    mkdirs_for(rec_file);
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", rec_file);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int i = 0; i < nrecs; i++) {
        const Rec *r = &recs[i];
        if (r->kind == 'b') fprintf(f, "best\t%s\t%d\t%d\t%d\n", r->pack, r->r.score, r->r.rating, r->r.style);
        else if (r->kind == 'd') fprintf(f, "day\t%s\t%s\t%d\t%d\t%d\n", r->date, r->pack, r->r.score, r->r.rating, r->r.style);
        else fprintf(f, "word\t%s\n", r->date);
    }
    if (fclose(f) == 0) rename(tmp, rec_file);
    else remove(tmp);
}

const Result *record_best(const char *pack) {
    Rec *r = find('b', "", pack);
    return r ? &r->r : NULL;
}

const Result *record_day(const char *date, const char *pack) {
    Rec *r = find('d', date, pack);
    return r ? &r->r : NULL;
}

void record_result(const char *date, const char *pack, Result res, int daily, int *new_best, int *new_day) {
    *new_best = *new_day = 0;
    Rec *b = find('b', "", pack);
    if (!b || res.score > b->r.score) {
        if (!b) b = add('b', "", pack);
        b->r = res;
        *new_best = 1;
    }
    if (daily) {
        Rec *d = find('d', date, pack);
        if (!d || res.score > d->r.score) {
            if (!d) d = add('d', date, pack);
            d->r = res;
            *new_day = 1;
        }
    }
}

void record_word_day(const char *date) {
    if (!find('w', date, "")) add('w', date, "");
}

int word_days(void) {
    int n = 0;
    for (int i = 0; i < nrecs; i++) n += recs[i].kind == 'w';
    return n;
}
