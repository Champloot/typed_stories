// lexicon: lexicon.lex: intents and the phrases that express them
#ifndef LEXICON_H
#define LEXICON_H

#include "common.h"

extern Intent *lex;

const Intent *find_intent(const char *name);
void load_lexicon(const char *dir);

#endif
