#include "progress.h"
#include "util.h"

typedef struct {
    char *pack;
    char *medal;        // NULL: a 'done' line
} Line;

static Line *lines;
static int   nlines, cap;
static char  file[1024];

static int find(const char *pack, const char *medal) {
    for (int i = 0; i < nlines; i++)
        if (!strcmp(lines[i].pack, pack) && (medal ? lines[i].medal && !strcmp(lines[i].medal, medal) : !lines[i].medal))
            return i;
    return -1;
}

static void add(const char *pack, const char *medal) {
    if (find(pack, medal) >= 0) return;
    if (nlines == cap) {
        cap = cap ? cap * 2 : 32;
        lines = xrealloc(lines, (size_t)cap * sizeof *lines);
    }
    lines[nlines].pack = xstrdup(pack);
    lines[nlines].medal = medal ? xstrdup(medal) : NULL;
    nlines++;
}

void progress_load(void) {
    if (!data_path(file, sizeof file, "progress.txt")) return;
    FILE *f = fopen(file, "r");
    if (!f) return;
    char buf[LINE_BUF];
    while (fgets(buf, sizeof buf, f)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        char *kind = strtok(buf, "\t"), *pack = strtok(NULL, "\t"), *medal = strtok(NULL, "\t");
        if (!kind || !pack) continue;           // a broken line: skip it, keep the rest
        if (!strcmp(kind, "done")) add(pack, NULL);
        else if (!strcmp(kind, "medal") && medal) add(pack, medal);
    }
    fclose(f);
}

void progress_save(void) {
    if (!*file) return;
    mkdirs_for(file);
    FILE *f = fopen(file, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        if (lines[i].medal) fprintf(f, "medal\t%s\t%s\n", lines[i].pack, lines[i].medal);
        else fprintf(f, "done\t%s\n", lines[i].pack);
    }
    fclose(f);
}

int  progress_done(const char *pack) { return find(pack, NULL) >= 0; }
void progress_set_done(const char *pack) { add(pack, NULL); }
int  progress_medal(const char *pack, const char *medal) { return find(pack, medal) >= 0; }
void progress_add_medal(const char *pack, const char *medal) { add(pack, medal); }
