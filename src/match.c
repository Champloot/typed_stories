// match: turning what the player typed into one of the choices
#include "match.h"

// Edit distance, giving up early once it is surely above cap.
static int lev(const char *a, const char *b, int cap) {
    static int prev[LINE_BUF + 1], curr[LINE_BUF + 1];
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la > LINE_BUF || lb > LINE_BUF || abs(la - lb) > cap) return cap + 1;
    for (int j = 0; j <= lb; j++) prev[j] = j;
    for (int i = 1; i <= la; i++) {
        curr[0] = i;
        int rowmin = i;
        for (int j = 1; j <= lb; j++) {
            int best = prev[j] + 1;
            if (curr[j - 1] + 1 < best) best = curr[j - 1] + 1;
            int diag = prev[j - 1] + (a[i - 1] != b[j - 1]);
            if (diag < best) best = diag;
            curr[j] = best;
            if (best < rowmin) rowmin = best;
        }
        if (rowmin > cap) return cap + 1;
        memcpy(prev, curr, (size_t)(lb + 1) * sizeof *prev);
    }
    return prev[lb];
}

// Does the phrase appear in the input as whole words ("of course" in "oh of course thanks")?
static int contains_phrase(const char *in, const char *ph) {
    size_t n = strlen(ph);
    for (const char *p = in; (p = strstr(p, ph)); p++)
        if ((p == in || p[-1] == ' ') && (p[n] == '\0' || p[n] == ' ')) return 1;
    return 0;
}

// Short phrases ("no", "go", "y") are too easy to find by accident inside a
// sentence, so only phrases of 4+ characters or several words count there.
static int loose_ok(const char *ph) {
    return strlen(ph) >= 4 || strchr(ph, ' ');
}

/* In order:
   1. the whole answer is a phrase;
   2. for answers of 4+ characters, the single closest phrase within 1 edit
      (2 for 8+ characters): a typo;
   3. the answer is a sentence with phrases of exactly one choice in it.
   Ties between choices are no match: better "Huh?" than a wrong guess. */
Match match_input(const Variant *v, const char *in) {
    Match m;
    memset(&m, 0, sizeof m);
    m.choice = -1;
    if (!*in) return m;
    for (int c = 0; c < v->nchoices; c++)
        for (int p = 0; p < v->choices[c].nph; p++)
            if (!strcmp(in, v->choices[c].ph[p].text)) {
                m.choice = c; m.tier = v->choices[c].ph[p].tier; m.phrase = v->choices[c].ph[p].text;
                return m;
            }
    int len = (int)strlen(in);
    if (len >= 4) {
        int cap = len >= 8 ? 2 : 1;
        int best = cap + 1, who = -1, tie = 0, tier = 0;
        const char *phrase = NULL;
        for (int c = 0; c < v->nchoices; c++)
            for (int p = 0; p < v->choices[c].nph; p++) {
                const Phrase *ph = &v->choices[c].ph[p];
                int d = lev(in, ph->text, cap);
                if (d < best) {
                    best = d; who = c; tie = 0; tier = ph->tier; phrase = ph->text;
                } else if (d == best && d <= cap) {
                    if (c != who) tie = 1;
                    else if (ph->tier > tier) { tier = ph->tier; phrase = ph->text; }
                }
            }
        if (who >= 0 && best <= cap && !tie) {
            m.choice = who; m.tier = tier; m.typo = 1; m.phrase = phrase;
            return m;
        }
    }
    // a sentence: the best phrase found wins, if all found phrases agree on the choice
    int who = -1, tier = 0;
    const char *phrase = NULL;
    for (int c = 0; c < v->nchoices; c++)
        for (int p = 0; p < v->choices[c].nph; p++) {
            const Phrase *ph = &v->choices[c].ph[p];
            if (!loose_ok(ph->text) || !contains_phrase(in, ph->text)) continue;
            if (who >= 0 && who != c) return m;         // two choices in one sentence
            if (who < 0 || ph->tier > tier || (ph->tier == tier && strlen(ph->text) > strlen(phrase))) {
                who = c; tier = ph->tier; phrase = ph->text;
            }
        }
    if (who >= 0) {
        m.choice = who; m.tier = tier; m.loose = 1; m.phrase = phrase;
    }
    return m;
}

Match match_answer(const Variant *v, const char *in, const char *word) {
    char rest[LINE_BUF];
    size_t n = 0;
    int found = 0;
    rest[0] = '\0';
    if (word) {
        // keep every word of the answer except the word of the day
        const char *p = in;
        while (*p) {
            const char *w = p;
            while (*p && *p != ' ') p++;
            size_t wl = (size_t)(p - w);
            if (wl == strlen(word) && !strncmp(w, word, wl)) found = 1;
            else if (n + wl + 2 < sizeof rest) {
                if (n) rest[n++] = ' ';
                memcpy(rest + n, w, wl);
                n += wl;
                rest[n] = '\0';
            }
            while (*p == ' ') p++;
        }
    }
    Match m = match_input(v, found ? rest : in);
    m.word = found;
    m.only_word = found && !*rest;
    return m;
}
