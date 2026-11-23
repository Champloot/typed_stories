#!/usr/bin/env python3
"""Loader test: every broken pack/lexicon file must be refused with a clear
'file:line: message', and valid files must load. Run from the project root:
    python3 tests/loader_test.py        (or: make test-quick)
"""
import os, shutil, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
S = tempfile.mkdtemp(prefix="typed_stories_loader_")
os.environ["HOME"] = S          # never touch the real style/records files

V = """pack: T
verdicts: a | b | c

scenario: S1
time: 3
variant: cue one
choice: yes | ok | 1
choice: no | meh
timeout: late | 0
"""
P = """pack: T
verdicts: a | b
phases: Morning 1 | Evening

scenario: S1
phase: Morning
time: 3
choice: yes | ok
timeout: late

scenario: S2
phase: Evening
time: 3
choice: yes | ok
timeout: late
"""

def run(name, files):
    d = os.path.join(S, name)
    shutil.rmtree(d, ignore_errors=True); os.makedirs(d)
    for fn, body in files.items():
        with open(f"{d}/{fn}", "w", encoding="utf-8") as f: f.write(body)
    r = subprocess.run([GAME, d], capture_output=True, text=True, timeout=3, input="", start_new_session=True)
    return r, d

res = []
def bad(name, text, expect, files=None):
    files = files or {"01-t.txt": text}
    r, d = run(name, files or {"01-t.txt": text})
    msg = r.stderr.strip().replace(d + "/", "")
    ok = r.returncode == 1 and expect in r.stderr
    res.append(ok)
    print(("PASS " if ok else "FAIL ") + name.ljust(24), "|", msg.splitlines()[0] if msg else f"(rc={r.returncode}, no stderr)")

# --- new: variants
bad("variant_no_timeout", V.replace("timeout: late | 0\n", ""), "01-t.txt:6: scenario 'S1': missing 'timeout'")
bad("variant_no_choice", V + "variant: cue two\ntimeout: x\n", "01-t.txt:10: scenario 'S1': needs at least one 'choice'")
bad("second_timeout", V + "timeout: again\n", "second 'timeout' in the same variant")
bad("timeout_mark_bad", V.replace("late | 0", "late | soon"), "mark must be an integer from -10 to 10")
bad("timeout_mark_range", V.replace("late | 0", "late | 11"), "mark must be an integer from -10 to 10")
bad("timeout_3_fields", V.replace("late | 0", "late | 0 | x"), "'timeout' is: what happens | mark")
bad("choice_mark_bad", V.replace("ok | 1", "ok | lots"), "mark must be an integer")
bad("same_phrase_twice", V.replace("choice: no | meh", "choice: Yes! | meh"), "both accept \"yes\"")
bad("weight_bad", V + "weight: 0\n", "'weight' must be an integer from 1 to 100")
bad("too_many_variants", V + "".join(f"variant: v{i}\nchoice: a | b\ntimeout: t\n" for i in range(8)), "at most 8 variants")
bad("five_choices", V + "".join(f"choice: w{i} | r\n" for i in range(3)), "at most 4 choices per variant")
# --- new: verdicts
bad("one_verdict", V.replace("a | b | c", "a"), "'verdicts' needs 2 to 8 entries")
bad("nine_verdicts", V.replace("a | b | c", " | ".join("v%d" % i for i in range(9))), "'verdicts' needs 2 to 8 entries")
bad("second_verdicts", V.replace("verdicts: a | b | c\n", "verdicts: a | b\nverdicts: c | d\n"), "second 'verdicts' line")
# --- new: phases
bad("phase_not_listed", P.replace("phase: Evening", "phase: Night"), "phase 'Night' is not listed in 'phases:'")
bad("phase_without_phases", V.replace("time: 3", "time: 3\nphase: Morning"), "the pack has no 'phases:' line")
bad("missing_phase", P.replace("phase: Evening\n", ""), "scenario 'S2': missing 'phase'")
bad("slot_with_phases", P.replace("phase: Evening", "phase: Evening\nslot: last"), "'slot' can't be used together with 'phases'")
bad("empty_phase", P.replace("phases: Morning 1 | Evening", "phases: Morning 1 | Evening | Night"), "phase 'Night' has no scenarios")
bad("phase_twice", P.replace("phases: Morning 1 | Evening", "phases: Morning | Morning 2"), "phase 'Morning' is listed twice")
bad("phase_count_zero", P.replace("Morning 1", "Morning 0"), "phase count must be from 1 to 100")
bad("phases_after_scen", P.replace("phases: Morning 1 | Evening\n", "") .replace("phase: Morning\n", "").replace("phase: Evening\n", "") + "phases: A\n", "'phases' must come before the first scenario")
bad("empty_phase_name", P.replace("Morning 1 | Evening", "Morning 1 | | Evening"), "empty phase name")
# --- new: ASCII text
bad("cyrillic_text", V.replace("ok | 1", "привет | 1"), "only plain ASCII text")
bad("fancy_quote", V.replace("ok | 1", "it’s ok | 1"), "only plain ASCII text")
import subprocess as sp
try:
    run("cyrillic_comment_ok", {"01-t.txt": "# комментарий по-русски\n" + V})
    ok = False   # exited: loader must have refused it
except sp.TimeoutExpired:
    ok = True    # the game started and sat in the menu: loader accepted the file
res.append(ok); print(("PASS " if ok else "FAIL ") + "cyrillic_comment_ok".ljust(24), "| loader accepted it")
# --- carried over
bad("missing_time", V.replace("time: 3\n", ""), "missing 'time'")
bad("unknown_key_base", V + "base: 150\n", "unknown key 'base'")
bad("key_before_scenario", "pack: T\nverdicts: a|b\ntime: 3\n", "before the first 'scenario:'")
bad("no_scenarios", "pack: T\nverdicts: a|b\n", "no scenarios")
bad("missing_pack", V.replace("pack: T\n", ""), "missing 'pack:'")
bad("missing_verdicts", V.replace("verdicts: a | b | c\n", ""), "missing 'verdicts:'")
bad("two_first", V + "slot: first\n\nscenario: S2\nslot: first\ntime: 2\nchoice: a | b\ntimeout: t\n", "at most one 'slot: first'")
bad("bad_time", V.replace("time: 3", "time: soon"), "'time' must be")
bad("bad_on_timeout", V + "on_timeout: explode\n", "'on_timeout' must be")
bad("non_ascii_word", V.replace("choice: no | meh", "choice: nö | meh"), "only plain ASCII text")
bad("empty_word", V.replace("choice: no | meh", "choice:  | meh"), "empty part in choice")
bad("no_colon", V + "just words\n", "expected 'key: value'")
bad("line_too_long", V.replace("timeout: late", "timeout: " + "x" * 2000), "line too long")
bad("no_txt_files", "", "no pack files", files={"readme.md": "x"})

# --- lexicon
LEX = """intent: yes
hint: agree
1: yes | ok
2: of course
intent: no
hint: refuse
1: no | nope
"""
def lexbad(name, lex, expect, pack=None):
    bad(name, None, expect, files={"01-t.txt": pack or V, "lexicon.lex": lex})
lexbad("lex_unknown_key", LEX + "4: wow\n", "unknown key '4' (expected 'intent', 'hint', '1', '2' or '3')")
lexbad("lex_phrase_before_intent", "1: yes\n" + LEX, "phrases before the first 'intent:'")
lexbad("lex_bad_name", LEX + "intent: Yes Please\nhint: h\n1: x\n", "intent name must use lowercase letters")
lexbad("lex_intent_twice", LEX + "intent: yes\nhint: h\n1: x\n", "intent 'yes' is defined twice")
lexbad("lex_no_phrases_mid", "intent: a\nhint: h\nintent: b\nhint: h\n1: x\n", "lexicon.lex:3: intent 'a' has no phrases")
lexbad("lex_no_phrases_end", LEX + "intent: c\n", "intent 'c' has no phrases")

lexbad("lex_dup_phrase", LEX.replace("2: of course", "2: of course | Of Course!"), "phrase 'Of Course!' is listed twice in intent 'yes'")
lexbad("lex_empty_phrase", LEX.replace("1: no | nope", "1: no | | nope"), "empty phrase")
lexbad("lex_non_ascii", LEX.replace("of course", "\u0434\u0430"), "lexicon.lex:4: only plain ASCII text")
# --- choices built from intents
PI = """pack: T
verdicts: a | b

scenario: S1
time: 3
choice: CHOICE_A | ok | 1
choice: CHOICE_B | meh
timeout: late
"""
def pi(a, b): return PI.replace("CHOICE_A", a).replace("CHOICE_B", b)
lexbad("unknown_intent", LEX, "unknown intent 'get_up' (not in lexicon.lex)", pack=pi("get_up", "no"))
lexbad("intent_overlap_literal", LEX, "choices 'agree' and 'of course' both accept \"of course\"", pack=pi("yes", "of course"))
lexbad("intent_overlap_intent", LEX + "intent: maybe\nhint: perhaps\n1: maybe | ok\n", "choices 'agree' and 'perhaps' both accept \"ok\"", pack=pi("yes", "maybe"))
lexbad("empty_plus_part", LEX, "empty part in choice (check the '+' signs)", pack=pi("yes +", "no"))
lexbad("punct_only_literal", LEX, "choice '!!!' has no letters or digits to type", pack=pi("yes", "!!!"))
lexbad("too_many_plus", LEX, "at most 8 parts joined with '+'", pack=pi("yes", " + ".join("w%d" % i for i in range(9))))

lexbad("lex_missing_hint", LEX + "intent: z\n1: zz\n", "intent 'z' has no 'hint'")
lexbad("lex_hint_before_intent", "hint: h\n" + LEX, "'hint' before the first 'intent:'")
lexbad("lex_second_hint", LEX + "hint: again\n", "second 'hint' in intent 'no'")
lexbad("lex_empty_hint", LEX.replace("hint: refuse", "hint: ?!"), "empty hint")
lexbad("hint_collides", LEX + "intent: maybe\nhint: of course\n1: maybe\n", "choices 'agree' and 'of course' both accept \"of course\"", pack=pi("yes", "maybe"))
lexbad("choice_five_fields", LEX, "'choice' is: words | what happens | mark | hint", pack=PI.replace("CHOICE_A | ok | 1", "yes | ok | 1 | take it | extra").replace("CHOICE_B", "no"))
lexbad("choice_empty_hint", LEX, "empty hint", pack=PI.replace("CHOICE_A | ok | 1", "yes | ok | 1 | ...").replace("CHOICE_B", "no"))
lexbad("choice_hint_collides", LEX, "both accept \"no\"", pack=PI.replace("CHOICE_A | ok | 1", "yes | ok | 1 | no").replace("CHOICE_B", "no"))


# --- flags
F = """pack: T
verdicts: a | b
phases: Day | Night

scenario: S1
phase: Day
time: 3
choice: yes | ok | 1
sets: said_yes
choice: no | meh
timeout: late

scenario: S2
phase: Night
needs: said_yes
time: 3
choice: yes | ok
timeout: late
"""
bad("sets_before_outcome", F.replace("phase: Day\ntime: 3\n", "phase: Day\ntime: 3\nsets: x\n"), "'sets' must come right after a 'choice' or 'timeout' line")
bad("bad_flag_name", F.replace("sets: said_yes", "sets: Said Yes"), "flag name 'Said Yes' must use lowercase letters")
bad("checked_never_set", F.replace("needs: said_yes", "needs: said_yess"), "01-t.txt:15: flag 'said_yess' is checked here, but no outcome sets it")
bad("negated_never_set", F.replace("needs: said_yes", "needs: !nobody_sets_me"), "flag 'nobody_sets_me' is checked here")
bad("second_needs", F.replace("needs: said_yes", "needs: said_yes\nneeds: said_yes"), "second 'needs' in one scenario")
bad("second_if", F.replace("needs: said_yes", "if: said_yes\nif: said_yes"), "second 'if' in one variant")
bad("too_many_sets", F.replace("sets: said_yes", "sets: a, b, c, d, e"), "at most 4 flags in one 'sets'")
bad("sets_overflow", F.replace("sets: said_yes", "sets: a, b, c\nsets: d, e"), "at most 4 flags per outcome")
bad("empty_condition", F.replace("needs: said_yes", "needs: said_yes,"), "empty condition")
bad("dangling_or", F.replace("needs: said_yes", "needs: said_yes or"), "flag name 'said_yes or' must use")
bad("too_many_or", F.replace("needs: said_yes", "needs: said_yes or said_yes or said_yes or said_yes or said_yes"), "at most 4 flags joined with 'or'")
bad("empty_sets_name", F.replace("sets: said_yes", "sets: said_yes,"), "empty flag name")
bad("bang_only", F.replace("needs: said_yes", "needs: !"), "empty flag name in condition")
bad("too_many_flags", F + "".join("\nscenario: X%d\nphase: Day\ntime: 3\nchoice: go | ok\nsets: f%d\ntimeout: t\n" % (i, i) for i in range(64)), "at most 64 different flags per pack")
import subprocess as sp
for nm, txt in (("flags_valid", F.replace("needs: said_yes", "needs: said_yes or !said_yes, said_yes")),
                ("night_only_needs", F)):
    try:
        run(nm, {"01-t.txt": txt}); ok = False
    except sp.TimeoutExpired:
        ok = True
    res.append(ok); print(("PASS " if ok else "FAIL ") + nm.ljust(24), "| loader accepted it")

# a valid pack + lexicon must load (game then sits in the menu)
import subprocess as sp
try:
    run("lex_valid", {"01-t.txt": pi("yes + of course", "no"), "lexicon.lex": LEX.replace("2: of course\n", "")})
    ok = False
except sp.TimeoutExpired:
    ok = True
res.append(ok); print(("PASS " if ok else "FAIL ") + "lex_valid_loads".ljust(24), "| loader accepted it")


# --- meters, endings, effects
M = """pack: T
verdicts: a | b
meters: Nerves 5 | Money 3
ending: Nerves | You snap.
ending: money | Broke.

scenario: S1
time: 3
choice: yes | ok | 1
effects: nerves -1, MONEY +2
choice: no | meh
timeout: late
effects: money -1
"""
bad("meters_after_scenario", V + "meters: Luck\n", "'meters' must come before the first scenario")
bad("meter_start_zero", M.replace("Money 3", "Money 0"), "a meter starts at 1 to 10")
bad("meter_start_big", M.replace("Money 3", "Money 11"), "a meter starts at 1 to 10")
bad("meter_name_digits", M.replace("Money 3", "M0ney 3"), "meter name 'M0ney' must be one word of letters")
bad("meter_twice", M.replace("Money 3", "nerves 3"), "meter 'nerves' is listed twice")
bad("five_meters", M.replace("Money 3", "Money 3 | A | B | C"), "at most 4 meters")
bad("missing_ending", M.replace("ending: money | Broke.\n", ""), "meter 'Money' has no 'ending:'")
bad("ending_unknown_meter", M.replace("ending: money | Broke.", "ending: luck | Unlucky."), "no meter 'luck' (declare it in 'meters:' first)")
bad("second_ending", M.replace("ending: money | Broke.", "ending: money | Broke.\nending: Money | Again."), "second 'ending' for meter 'Money'")
bad("ending_no_text", M.replace("ending: money | Broke.", "ending: money"), "'ending' is: meter | what happens when it hits zero")
bad("effects_no_meters", V + "effects: nerves -1\n", "'effects' used, but the pack has no 'meters:' line")
bad("effects_before_outcome", M.replace("time: 3\n", "time: 3\neffects: nerves -1\n"), "'effects' must come right after a 'choice' or 'timeout' line")
bad("effects_unknown_meter", M.replace("effects: money -1", "effects: luck -1"), "no meter 'luck' (declare it in 'meters:')")
bad("effects_zero", M.replace("effects: money -1", "effects: money 0"), "meter change must be a non-zero integer")
bad("effects_too_big", M.replace("effects: money -1", "effects: money -11"), "meter change must be a non-zero integer")
bad("effects_no_number", M.replace("effects: money -1", "effects: money"), "'effects' is: meter +1, meter -2")
bad("effects_five", M.replace("effects: money -1", "effects: money -1, money -1, money -1, money -1, money -1"), "at most 4 meters in one 'effects'")
bad("effects_overflow", M.replace("effects: money -1", "effects: money -1, money -1, money -1\neffects: money -1, money -1"), "at most 4 meter changes per outcome")
# --- built-in flags
bad("sets_builtin", F.replace("sets: said_yes", "sets: wordsmith"), "'wordsmith' is set by the game itself, packs can only check it")
bad("sets_builtin_today", F.replace("sets: said_yes", "sets: today"), "'today' is set by the game itself")
# --- words.lst
def wordbad(name, words, expect):
    bad(name, None, expect, files={"01-t.txt": V, "words.lst": words})
wordbad("word_uppercase", "Hedgehog\n", "words.lst:1: a word of the day is 3 to 20 lowercase letters")
wordbad("word_short", "ox\n", "a word of the day is 3 to 20 lowercase letters")
wordbad("word_two_words", "sea urchin\n", "a word of the day is 3 to 20 lowercase letters")
wordbad("word_twice", "# words\nhedgehog\nhedgehog\n", "words.lst:3: 'hedgehog' is listed twice")
wordbad("word_in_answers", "hedgehog\nyes\n", "words.lst:2: 'yes' is part of an answer (\"yes\"), pick another word")
import subprocess as sp
for nm, files in (("meters_valid", {"01-t.txt": M}),
                  ("needs_builtin_valid", {"01-t.txt": F.replace("needs: said_yes", "needs: wordsmith or today")}),
                  ("words_valid", {"01-t.txt": V, "words.lst": "# fine\nhedgehog\nnarwhal\n"})):
    try:
        run(nm, files); ok = False
    except sp.TimeoutExpired:
        ok = True
    res.append(ok); print(("PASS " if ok else "FAIL ") + nm.ljust(24), "| loader accepted it")

print(f"\n{sum(res)}/{len(res)} loader cases passed")
shutil.rmtree(S, ignore_errors=True)
sys.exit(0 if all(res) else 1)
