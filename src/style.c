// style: style points and phrase heat, kept between runs
#include "config.h"
#include "style.h"
#include "util.h"

/* Every phrase has a "heat": +1 each time it is used, halved at the start of
   every run. Points are divided by (1 + heat), so a phrase used every day
   pays about a third, and recovers if you leave it alone for a while. */

typedef struct { char *phrase; double heat; } Heat;

static Heat *heats;
static int   nheats, heatcap;
static char  style_file[1024];

double *heat_of(const char *phrase, int create) {
    for (int i = 0; i < nheats; i++)
        if (!strcmp(heats[i].phrase, phrase)) return &heats[i].heat;
    if (!create) return NULL;
    if (nheats == heatcap) {
        heatcap = heatcap ? heatcap * 2 : 64;
        heats = xrealloc(heats, (size_t)heatcap * sizeof *heats);
    }
    heats[nheats].phrase = xstrdup(phrase);
    heats[nheats].heat = 0;
    return &heats[nheats++].heat;
}

void style_load(void) {
    if (!data_path(style_file, sizeof style_file, "style.txt")) return;
    FILE *f = fopen(style_file, "r");
    if (!f) return;
    char line[LINE_BUF];
    while (fgets(line, sizeof line, f)) {
        char *sp = strchr(line, ' ');
        if (!sp) continue;
        *sp = '\0';
        char *end;
        double h = strtod(line, &end);
        if (end == line || *end || h <= 0 || h > 1000) continue;
        char *ph = trim(sp + 1);
        if (*ph) *heat_of(ph, 1) = h;
    }
    fclose(f);
}

// Best effort: if the file can't be written, style simply doesn't remember.
void style_save(void) {
    if (!*style_file) return;
    mkdirs_for(style_file);
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", style_file);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int i = 0; i < nheats; i++)
        if (heats[i].heat >= 0.01) fprintf(f, "%.4f %s\n", heats[i].heat, heats[i].phrase);
    if (fclose(f) == 0) rename(tmp, style_file);
    else remove(tmp);
}

void style_new_run(void) {
    for (int i = 0; i < nheats; i++) heats[i].heat *= cfg.heat_decay;
}

int style_points(int tier, int typo, double heat) {
    const int base[4] = { 0, cfg.tier1, cfg.tier2, cfg.tier3 };
    double p = base[tier] / (1.0 + heat);
    if (typo) p *= cfg.typo_factor;
    return (int)(p + 0.5);
}

char style_rank(int pts) {
    return pts >= 50 ? 'S' : pts >= 35 ? 'A' : pts >= 20 ? 'B' : pts >= 10 ? 'C' : 'D';
}

