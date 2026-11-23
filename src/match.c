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

// Exact match first; then, for inputs of 4+ characters, the single closest
// phrase within 1 edit (2 for 8+ characters). A tie between choices is no match.
Match match_input(const Variant *v, const char *in) {
    Match m = { -1, 0, 0, NULL };
    for (int c = 0; c < v->nchoices; c++)
        for (int p = 0; p < v->choices[c].nph; p++)
            if (!strcmp(in, v->choices[c].ph[p].text)) {
                m.choice = c; m.tier = v->choices[c].ph[p].tier; m.phrase = v->choices[c].ph[p].text;
                return m;
            }
    int len = (int)strlen(in);
    if (len < 4) return m;
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
    }
    return m;
}

