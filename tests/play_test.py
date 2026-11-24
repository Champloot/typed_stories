#!/usr/bin/env python3
"""Play test: plays whole days through a terminal emulator and checks every
screen against a Python model of the game: matching, style, flags, meters,
word of the day, rating, score and records.

    python3 tests/play_test.py                every check below (several minutes)
    python3 tests/play_test.py days           three ordinary days: best, worst, mixed
    python3 tests/play_test.py decay          knight three times: repeated words pay less
    python3 tests/play_test.py narrow         a mixed day in a 40-column terminal
    python3 tests/play_test.py consequences   many days: unlocked scenes, conditional variants, early endings
    python3 tests/play_test.py stories        every story for several days: finales and early endings
    python3 tests/play_test.py today          Today mode gives the same day for the same date
    python3 tests/play_test.py secret         5 days with the word of the day unlock the secret scene

Needs pyte (pip install pyte, or python3Packages.pyte in the Nix dev shell).
"""
import fcntl, json, os, pty, re, select, shutil, signal, struct, sys, tempfile, termios, time
try:
    import pyte
except ImportError:
    sys.exit("play_test needs pyte: pip install pyte (or python3Packages.pyte in the dev shell)")

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACKS = os.path.join(PROJ, "packs")
DATE = "2026-11-24"
norm_ws = lambda t: re.sub(r"\s+", " ", t).strip()

# ------------------------------------------------------------------ model of the files

def normalize(s):
    out, space = [], False
    for ch in s:
        if ch == "'": continue
        if ch.isascii() and ch.isalnum():
            if space and out: out.append(" ")
            space = False
            out.append(ch.lower())
        else:
            space = True
    return "".join(out)[:60]

def push(lst, raw, text, tier):
    for p in lst:
        if p[0] == text:
            p[1] = max(p[1], tier)
            return
    lst.append([text, tier, raw])

def kv_lines(path):
    for raw in open(path):
        line = raw.strip()
        if not line or line.startswith("#"): continue
        k, v = [x.strip() for x in line.split(":", 1)]
        yield k, v

def parse_lex(path):
    lex, cur = {}, None
    def finish():
        if cur: push(cur["ph"], cur["hint"], normalize(cur["hint"]), 1)
    if not os.path.exists(path): return lex
    for k, v in kv_lines(path):
        if k == "intent":
            finish(); cur = lex[v] = dict(hint=None, ph=[])
        elif k == "hint": cur["hint"] = v
        else:
            for part in v.split("|"):
                part = part.strip(); push(cur["ph"], part, normalize(part), int(k))
    finish()
    return lex

def parse_words(path):
    return [l.strip() for l in open(path) if l.strip() and not l.startswith("#")] if os.path.exists(path) else []

def cond(v):
    return [[(lit.strip().lstrip("!").strip(), lit.strip().startswith("!")) for lit in clause.split(" or ")] for clause in v.split(",")]

def parse_pack(path, lex):
    pk = dict(name=None, verdicts=[], phases=[], scen=[], meters=[], endings={}, finales=[], finale_if=[])
    cur = None
    def newvar(detail=None):
        cur["variants"].append(dict(detail=detail, choices=[], timeout=None, mode="text", tsets=[], tfx=[], cond=None, last=None))
        return cur["variants"][-1]
    def var():
        return cur["variants"][-1] if cur["variants"] else newvar()
    def outcome(v):
        if v["last"] == "timeout": return (v["tsets"], v["tfx"])
        if v["last"] == "fail": return (v["choices"][-1]["fsets"], v["choices"][-1]["ffx"])
        return (v["choices"][-1]["sets"], v["choices"][-1]["fx"])
    for k, v in kv_lines(path):
        if k == "pack": pk["name"] = v
        elif k == "verdicts": pk["verdicts"] = [norm_ws(x) for x in v.split("|")]
        elif k == "phases":
            for part in v.split("|"):
                bits = part.strip().rsplit(" ", 1)
                pk["phases"].append((bits[0].strip(), int(bits[1])) if len(bits) == 2 and bits[1].isdigit() else (part.strip(), 0))
        elif k == "meters":
            for part in v.split("|"):
                bits = part.strip().rsplit(" ", 1)
                pk["meters"].append((bits[0].strip(), int(bits[1])) if len(bits) == 2 and bits[1].isdigit() else (part.strip(), 5))
        elif k == "ending":
            m, text = [x.strip() for x in v.split("|", 1)]
            pk["endings"][m.lower()] = norm_ws(text)
        elif k == "finale":
            t, text = [x.strip() for x in v.split("|", 1)]
            pk["finales"].append((norm_ws(t), norm_ws(text)))
        elif k == "finale_if":
            c, t, text = [x.strip() for x in v.split("|", 2)]
            pk["finale_if"].append((cond(c), norm_ws(t), norm_ws(text)))
        elif k == "scenario":
            cur = dict(title=norm_ws(v), time=0, phase=None, slot="any", variants=[], needs=None); pk["scen"].append(cur)
        elif k == "time": cur["time"] = float(v)
        elif k == "phase": cur["phase"] = v
        elif k == "slot": cur["slot"] = v
        elif k == "needs": cur["needs"] = cond(v)
        elif k == "if": var()["cond"] = cond(v)
        elif k == "variant": newvar(norm_ws(v) or None)
        elif k == "on_timeout": var()["mode"] = v
        elif k == "play": cur["play"] = v
        elif k == "act":
            bits = v.split(" ", 2); n = None; text = bits[2] if len(bits) > 2 else ""
            if text.split(" ")[0].isdigit(): n = int(text.split(" ")[0]); text = text.split(" ", 1)[1] if " " in text else ""
            var()["choices"][-1]["act"] = dict(kind=bits[0], secs=float(bits[1]), n=n, text=text)
        elif k == "fail":
            p = [x.strip() for x in v.split("|")]
            c = var()["choices"][-1]; c["fail"] = (norm_ws(p[0]), int(p[1]) if len(p) > 1 else 0); c["fsets"] = []; c["ffx"] = []
            var()["last"] = "fail"
        elif k == "goal": var()["choices"][-1]["goal"] = v[-1] if v.startswith("aisle") else "k"
        elif k in ("stage", "props", "show"): pass
        elif k == "weight": var()
        elif k == "sets": outcome(var())[0].extend(x.strip() for x in v.split(","))
        elif k == "effects":
            for part in v.split(","):
                name, d = part.strip().rsplit(" ", 1)
                idx = [m.lower() for m, _ in pk["meters"]].index(name.strip().lower())
                outcome(var())[1].append((idx, int(d)))
        elif k == "timeout":
            p = [x.strip() for x in v.split("|")]
            var()["timeout"] = (norm_ws(p[0]), int(p[1]) if len(p) > 1 else -1); var()["last"] = "timeout"
        elif k == "choice":
            p = [x.strip() for x in v.split("|")]
            c = dict(label=None, ph=[], result=norm_ws(p[1]), mark=int(p[2]) if len(p) > 2 else 0, sets=[], fx=[])
            if len(p) > 3: c["label"] = p[3]; push(c["ph"], p[3], normalize(p[3]), 1)
            for tok in [t.strip() for t in p[0].split("+")]:
                if tok in lex:
                    for text, tier, raw in lex[tok]["ph"]: push(c["ph"], raw, text, tier)
                    c["label"] = c["label"] or lex[tok]["hint"]
                else:
                    push(c["ph"], tok, normalize(tok), 1); c["label"] = c["label"] or tok
            var()["choices"].append(c); var()["last"] = "choice"
    return pk

# ------------------------------------------------------------------ model of the rules

def lev(a, b):
    prev = list(range(len(b) + 1))
    for i in range(1, len(a) + 1):
        cur = [i] + [0] * len(b)
        for j in range(1, len(b) + 1):
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1]))
        prev = cur
    return prev[len(b)]

def contains(inp, ph):
    return f" {ph} " in f" {inp} "

def match(v, inp):
    """(choice, tier, typo, phrase, loose) or None, like match_input()."""
    if not inp: return None
    for ci, c in enumerate(v["choices"]):
        for text, tier, _ in c["ph"]:
            if text == inp: return (ci, tier, 0, text, 0)
    L = len(inp)
    if L >= 4:
        cap = 2 if L >= 8 else 1
        best, who, tie, tier, ph = cap + 1, -1, False, 0, None
        for ci, c in enumerate(v["choices"]):
            for text, t, _ in c["ph"]:
                d = lev(inp, text) if abs(len(inp) - len(text)) <= cap else cap + 1
                if d < best: best, who, tie, tier, ph = d, ci, False, t, text
                elif d == best and d <= cap:
                    if ci != who: tie = True
                    elif t > tier: tier, ph = t, text
        if who >= 0 and best <= cap and not tie: return (who, tier, 1, ph, 0)
    who, tier, ph = -1, 0, None
    for ci, c in enumerate(v["choices"]):
        for text, t, _ in c["ph"]:
            if not (len(text) >= 4 or " " in text) or not contains(inp, text): continue
            if who >= 0 and who != ci: return None
            if who < 0 or t > tier or (t == tier and len(text) > len(ph)): who, tier, ph = ci, t, text
    return (who, tier, 0, ph, 1) if who >= 0 else None

def match_answer(v, inp, word):
    """(match or None, used_word, only_word) like match_answer()."""
    if word and word in inp.split(" "):
        rest = " ".join(t for t in inp.split(" ") if t != word)
        return match(v, rest), True, not rest
    return match(v, inp), False, False

def holds(c, flags):
    return all(any((name in flags) != neg for name, neg in clause) for clause in c)

def open_variants(sc, flags):
    conds = [v for v in sc["variants"] if v["cond"] and holds(v["cond"], flags)]
    return conds if conds else [v for v in sc["variants"] if not v["cond"]]

def scen_open(sc, flags):
    if sc["needs"] and not holds(sc["needs"], flags): return False
    return any(not v["cond"] or holds(v["cond"], flags) for v in sc["variants"])

BASE = {1: 10, 2: 30, 3: 60}
def style_pts(tier, typo, heat):
    p = BASE[tier] / (1.0 + heat)
    if typo: p *= 0.5
    return int(p + 0.5)
rank = lambda p: "S" if p >= 50 else "A" if p >= 35 else "B" if p >= 20 else "C" if p >= 10 else "D"

class World:
    """What persists between runs: phrase heat, word days, records."""
    def __init__(self):
        self.heat, self.word_days, self.best, self.day = {}, set(), {}, {}

# ------------------------------------------------------------------ the terminal

class Game:
    def __init__(self, home, cols=80, rows=40, date=DATE):
        self.screen = pyte.Screen(cols, rows); self.stream = pyte.ByteStream(self.screen); self.cols = cols
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
            os.chdir(PROJ); os.environ.update(TERM="xterm-color", HOME=home, TYPED_STORIES_DATE=date)
            for k in ("XDG_DATA_HOME", "LINES", "COLUMNS"): os.environ.pop(k, None)
            os.execv(os.path.join(PROJ, "game-tty"), ["./game-tty", PACKS])
        self.pump(0.5)
    def pump(self, w):
        end = time.time() + w
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.03)
            if r:
                try: self.stream.feed(os.read(self.fd, 65536))
                except OSError: return
    def lines(self): return [l.rstrip() for l in self.screen.display]
    def text(self): return norm_ws(" ".join(self.lines()))
    def send(self, b, gap=0.02):
        for c in b: os.write(self.fd, bytes([c])); time.sleep(gap)
    def quit(self):
        self.send(b"q"); t0 = time.time()
        while time.time() - t0 < 3:
            pid, _ = os.waitpid(self.pid, os.WNOHANG)
            if pid: return True
            self.pump(0.1)
        os.kill(self.pid, signal.SIGKILL); os.waitpid(self.pid, 0); return False

def block(L, i):
    out = []
    while i < len(L) and L[i].strip(): out.append(L[i].strip()); i += 1
    return norm_ws(" ".join(out)), i

# ------------------------------------------------------------------ one run

def play_run(g, world, packfile, key, strategy, today=False, log=False):
    strategy_fn = strategy
    lex = parse_lex(f"{PACKS}/lexicon.lex"); pk = parse_pack(f"{PACKS}/{packfile}", lex)
    words = parse_words(f"{PACKS}/words.lst")
    probs = []
    def chk(c, what):
        if not c: probs.append(what)
    index = {norm_ws(sc["title"] + (" " + v["detail"] if v["detail"] else "")): (sc, v) for sc in pk["scen"] for v in sc["variants"]}

    # the menu and the start of the day
    menu = g.text()
    chk("choose who you are today" in menu, "menu not shown")
    want_mode = "today" if today else "free play"
    if f"mode: {want_mode}" not in menu: g.send(b"t"); g.pump(0.3)
    chk(f"mode: {want_mode}" in g.text(), f"menu mode is not {want_mode}")
    m = re.search(r"Word of the day: (\w+)", g.text()); word = m.group(1) if m else None
    chk((word is not None) == bool(words), f"word of the day {word!r} vs words.lst")
    g.send(key.encode()); g.pump(0.4)
    start = g.text()
    chk(pk["name"] in start and "Press Enter to start" in start, f"no day start screen: {start[:120]!r}")
    if word: chk(f"Word of the day: {word.upper()}" in start, "day start does not show the word")
    chk(("Everyone gets the same day" in start) == today, "day start shows the wrong mode")
    g.send(b"\n"); g.pump(0.4)

    for k in list(world.heat): world.heat[k] *= 0.5
    st = dict(marks=0, best=0, worst=0, typed=0, erased=0, timeouts=0, misses=0, style=0, n=0)
    flags = set()
    if len(world.word_days) >= 5: flags.add("wordsmith")
    if today: flags.add("today")
    meters = [s for _, s in pk["meters"]]; ended = None; word_used = False
    phase_names = [n.upper() for n, _ in pk["phases"]]
    cur_phase, snap, seen_in_phase, seen, last_total, played = None, set(), [], [], None, 0

    def status_expected():
        s = "  ".join(f"{pk['meters'][i][0]} {meters[i]}{'!' if meters[i] <= 2 else ''}" for i in range(len(meters)))
        if word: s += ("    " if s else "") + f"word: {word}" + (" (used)" if word_used else "")
        return s[: g.cols - 6]
    def apply(fx, sets):
        nonlocal ended
        flags.update(sets); parts = []
        for mi, d in fx:
            meters[mi] = max(0, min(10, meters[mi] + d)); parts.append(f"{pk['meters'][mi][0]} {d:+d}")
        if ended is None:
            ended = next((i for i, x in enumerate(meters) if x == 0), None)
        return " ".join(parts)
    def check_phase(name, snapshot, seen_here):
        ph = next(n for n, _ in pk["phases"] if n.upper() == name); pick = dict(pk["phases"])[ph]
        reg = [s for s in pk["scen"] if s["phase"] == ph and not s["needs"] and scen_open(s, snapshot)]
        unl = {s["title"] for s in pk["scen"] if s["phase"] == ph and s["needs"] and scen_open(s, snapshot)}
        got_unl = {t for t, needs in seen_here if needs}; got_reg = [t for t, needs in seen_here if not needs]
        chk(got_unl == unl, f"phase {name}: unlocked {sorted(got_unl)} != expected {sorted(unl)}")
        chk(len(got_reg) == (min(pick, len(reg)) if pick else len(reg)), f"phase {name}: {len(got_reg)} regular scenes")
    def skipped(upto):
        start_i = phase_names.index(cur_phase) + 1 if cur_phase else 0
        for name in phase_names[start_i:upto]:
            ph = next(n for n, _ in pk["phases"] if n.upper() == name)
            chk(not any(s["phase"] == ph and scen_open(s, flags) for s in pk["scen"]), f"phase {name} skipped but had open scenes")

    i = 0
    finale_seen = None
    games = []          # (scene number, won) for every minigame played
    while True:
        L = g.lines(); T = g.text()
        if "RUN COMPLETE" in T or "DAY OVER EARLY" in T: break
        if "HIT ZERO. THE DAY IS OVER." in T:
            chk(ended is not None, "ending screen without a meter at zero")
            if ended is not None:
                mname = pk["meters"][ended][0]
                chk(f"{mname.upper()} HIT ZERO" in T and pk["endings"][mname.lower()] in T, f"wrong ending screen {T[:120]!r}")
            g.send(b" "); g.pump(0.4); continue
        if "THE END:" in T:
            chk(ended is None, "a finale after the day ended early")
            span, got = st["best"] - st["worst"], st["marks"] - st["worst"]
            exp = next(((t, x) for c, t, x in pk["finale_if"] if holds(c, flags)), None)
            if exp is None:
                n = len(pk["finales"])
                exp = pk["finales"][min(n - 1, max(0, got * n // span)) if span > 0 else n - 1]
            chk(f"THE END: {exp[0].upper()}" in T and exp[1] in T, f"finale: expected {exp[0]!r}, screen {T[:160]!r}")
            finale_seen = exp[0]
            g.send(b" "); g.pump(0.4); continue
        if "Press Enter when ready" not in T: probs.append(f"#{i}: unexpected screen {T[:120]!r}"); break
        chk(ended is None, "the day went on after a meter hit zero")
        chk(L[0][4:] == status_expected(), f"#{i}: status {L[0][4:]!r} != {status_expected()!r}")
        hm = re.match(r"\s*\[(\d+)/(\d+)\]\s*(\S*)", L[1])
        chk(hm and int(hm.group(1)) == played + 1, f"#{i}: header {L[1]!r}")
        ph_name = hm.group(3) or None if hm else None; last_total = int(hm.group(2)) if hm else None
        text, _ = block(L, 3)
        if text not in index: probs.append(f"#{i}: unknown screen text {text!r}"); break
        sc, v = index[text]; seen.append((sc["title"], v["detail"]))
        if pk["phases"] and ph_name != cur_phase:
            if cur_phase: check_phase(cur_phase, snap, seen_in_phase)
            skipped(phase_names.index(ph_name))
            cur_phase, snap = ph_name, set(flags); seen_in_phase.clear()
        seen_in_phase.append((sc["title"], bool(sc["needs"])))
        if sc["needs"]: chk(holds(sc["needs"], snap if pk["phases"] else flags), f"#{i}: unlocked {sc['title'][:30]!r} without its flags")
        chk(v in open_variants(sc, flags), f"#{i}: variant {v['detail']!r} shown, flags {sorted(flags)}")
        played += 1; st["n"] += 1
        ms = [c["mark"] for c in v["choices"]] + ([v["timeout"][1]] if v["mode"] == "text" else []) + [c["fail"][1] for c in v["choices"] if c.get("fail")]
        st["best"] += max(ms); st["worst"] += min(ms)
        g.send(b"\n"); g.pump(0.35)
        L = g.lines()
        inp = next((j for j, l in enumerate(L) if l.startswith("    > ")), None)
        if inp is None: probs.append(f"#{i}: no input line"); break
        hint = norm_ws(" ".join(l.strip() for l in L[block(L, 3)[1]:inp] if l.strip()))
        chk(hint == norm_ws("  /  ".join(c["label"] for c in v["choices"])), f"#{i}: hints {hint!r}")

        target, tech = strategy(i, sc, v)
        tag = f"#{i:2d} {tech:9s} {sc['title'][:26]!r:29s} {(v['detail'] or '-')[:20]!r:23s}"
        if target is None:
            t0 = time.time()
            while time.time() - t0 < sc["time"] + 3:
                g.pump(0.2)
                if "TIME'S UP" in g.text(): break
            L = g.lines(); st["timeouts"] += 1
            a = next((j for j, l in enumerate(L) if "TIME'S UP" in l), None)
            if a is None: probs.append(f"{tag}: no timeout"); break
            b = next(j for j, l in enumerate(L) if "(press any key)" in l)
            txt = norm_ws(" ".join(l.strip() for l in L[a + 1:b] if l.strip()))
            fxs = apply(v["tfx"], v["tsets"])
            if v["mode"] == "text":
                mk = v["timeout"][1]; exp = norm_ws(v["timeout"][0] + " " + fxs)
                ok = txt == exp
            else:
                fate = next((c for c in v["choices"] if txt.startswith(norm_ws(v["timeout"][0] + " " + c["result"]))), None)
                ok = fate is not None; mk = fate["mark"] if fate else 0
                if fate:
                    fxs = norm_ws(fxs + " " + apply(fate["fx"], fate["sets"]))
                    ok = txt == norm_ws(v["timeout"][0] + " " + fate["result"] + " " + fxs)
            st["marks"] += mk
            if log: print(f"{tag} timeout ({mk:+d}) {fxs or ''} {'OK' if ok else 'MISMATCH ' + repr(txt)}")
            chk(ok, f"{tag}: timeout screen {txt!r}")
            chk(g.lines()[0][4:] == status_expected(), f"{tag}: status after timeout {g.lines()[0][4:]!r} != {status_expected()!r}")
            g.send(b" "); g.pump(0.35); i += 1; continue

        c = v["choices"][target]
        tiers = sorted(c["ph"], key=lambda p: -p[1])
        def lands(raw, loose=False):
            mm, _, _ = match_answer(v, normalize(raw), word)
            return mm and mm[0] == target and (mm[4] == 1) == loose and mm[2] == 0
        fancy = next(p[2] for p in tiers if lands(p[2]))
        typed = c["label"] if tech == "plain" else fancy
        if tech == "typo":
            for text_, tier_, raw_ in tiers:
                if len(text_) >= 6:
                    t2 = text_[: len(text_) // 2] + text_[len(text_) // 2 + 1:]
                    mm = match(v, normalize(t2))
                    if mm and mm[0] == target and mm[2] == 1: typed = t2; break
        elif tech == "shout": typed = fancy.upper() + "!!"
        elif tech == "word" and word: typed = (f"{fancy}, {word}!" if i % 2 else f"{word} {fancy}")
        elif tech == "sentence":
            cand = next((f"well {p[2]} i guess" for p in tiers if lands(f"well {p[2]} i guess", loose=True)), None)
            typed = cand or fancy
        if tech == "huh":
            g.send(b"qzxv\n"); g.pump(0.3); st["typed"] += 4; st["misses"] += 1
            chk("Huh? Nobody gets 'qzxv'. Try again." in g.text(), f"{tag}: no Huh message")
        if tech == "word" and word and i % 3 == 0:
            g.send(word.encode() + b"\n"); g.pump(0.3); st["typed"] += len(word); st["misses"] += 1
            chk("Nice word. But what do you answer?" in g.text(), f"{tag}: the word alone was not refused")
        if tech == "backspace":
            g.send(typed.encode() + b"xq\x7f\x7f"); st["typed"] += len(typed) + 2; st["erased"] += 2
        elif tech == "ctrlw":
            g.send(b"blah wrong\x17\x17"); st["typed"] += 10; st["erased"] += 10
            g.send(typed.encode()); st["typed"] += len(typed)
        else:
            g.send(typed.encode()); st["typed"] += len(typed)
        g.pump(0.1)
        for _ in range(10):         # let the screen catch up with the typing
            inp_line = next((l for l in g.lines() if l.startswith("    > ")), "")
            if inp_line.strip() == "> " + typed + "_": break
            g.pump(0.05)
        chk(inp_line.strip() == "> " + typed + "_", f"{tag}: input line {inp_line!r}")
        g.send(b"\n"); g.pump(0.45)
        won = True
        if "ACT: " in g.text():             # a minigame: win it, or lose it on purpose
            chk(c.get("act") is not None, f"{tag}: a minigame after a choice without 'act:'")
            won = minigame(g, c["act"], lose=getattr(strategy_fn, "lose_games", False))
            games.append((len(seen) - 1, won))
            if log: print(f"{tag} minigame {c['act']['kind']}: {'won' if won else 'lost'}")
            g.pump(1.3)
        elif c.get("act"): probs.append(f"{tag}: no minigame screen after an 'act:' choice"); break
        if "GET TO" in g.text():            # an activity: walk to the spot the answer named
            chk(sc.get("play") is not None, f"{tag}: a walk in a scene without 'play:'")
            if not walk(g, c.get("goal")): probs.append(f"{tag}: could not walk to {c.get('goal')!r}"); break
            g.pump(0.4)
        L = g.lines()
        mm, used, _ = match_answer(v, normalize(typed), word)
        if not mm or mm[0] != target: probs.append(f"{tag}: model says {typed!r} -> {mm}"); break
        _, tier, typo, phrase, loose = mm
        before = world.heat.get(phrase, 0.0); pts = style_pts(tier, typo, before); world.heat[phrase] = before + 1.0
        bonus = 100 if used and not word_used else 0
        if bonus: word_used = True; world.word_days.add(DATE if not hasattr(g, "date") else g.date)
        if won:
            st["style"] += pts + bonus; st["marks"] += c["mark"]
            fxs = apply(c["fx"], c["sets"]); result = c["result"]
        else:       # the choice's 'fail:' happens, or the timeout if it has none
            st["style"] += pts + bonus
            if c.get("fail"):
                st["marks"] += c["fail"][1]; fxs = apply(c["ffx"], c["fsets"]); result = c["fail"][0]
            else:
                st["marks"] += v["timeout"][1]; fxs = apply(v["tfx"], v["tsets"]); result = v["timeout"][0]
        q = next((j for j, l in enumerate(L) if l.strip() == f'"{typed}"'), None)
        sl = next((j for j, l in enumerate(L) if re.match(r"\s*style [SABCD] \+\d+", l)), None)
        if q is None or sl is None: probs.append(f"{tag}: result screen broken {g.text()[:150]!r}"); break
        body = norm_ws(" ".join(l.strip() for l in L[q + 1:sl] if l.strip()))
        exp_body = norm_ws(result + " " + fxs)
        note = block(L, sl)[0]
        sm = re.match(r"style ([SABCD]) \+(\d+)(\s+\(typo\))?(\s+\(heard that before\))?(\s+WORD OF THE DAY \+100)?(\s+\(word already used today\))?\s+\d+\.\ds$", note)
        got = (sm.group(1), int(sm.group(2)), bool(sm.group(3)), bool(sm.group(4)), bool(sm.group(5)), bool(sm.group(6))) if sm else None
        exp = (rank(pts), pts, bool(typo), before >= 0.5, bool(bonus), bool(used and not bonus))
        ok = body == exp_body and got == exp
        if log: print(f"{tag} {typed!r:30s} t{tier}{' typo' if typo else ''}{' loose' if loose else ''}{' WORD' if used else ''} {fxs}: {'OK' if ok else 'MISMATCH ' + str(got) + ' vs ' + str(exp) + ' ' + repr(body)}")
        chk(ok, f"{tag}: got {got} / {body!r}, expected {exp} / {exp_body!r}")
        chk(L[0][4:] == status_expected(), f"{tag}: status {L[0][4:]!r} != {status_expected()!r}")
        g.send(b" "); g.pump(0.35); i += 1

    # end of the day
    chk((finale_seen is not None) == (ended is None and bool(pk["finales"])), f"finale shown: {finale_seen!r}, ended early: {ended is not None}")
    if pk["phases"] and ended is None:
        if cur_phase: check_phase(cur_phase, snap, seen_in_phase)
        skipped(len(phase_names))
    if ended is None and last_total is not None: chk(last_total == played, f"last header total {last_total}, played {played}")
    T = g.text()
    span, got = st["best"] - st["worst"], st["marks"] - st["worst"]
    rating = (got * 20 + span) // (2 * span) if span > 0 else 10
    nv = len(pk["verdicts"]); vi = min(nv - 1, max(0, got * nv // span)) if span > 0 else nv - 1
    avg = (st["style"] + st["n"] // 2) // st["n"] if st["n"] else 0
    acc = 100 * (st["typed"] - st["erased"]) / st["typed"] if st["typed"] else 100
    score = rating * 100 + st["style"] + 10 * sum(meters)
    if log: print(f"  END: {T[:200]}")
    chk(("DAY OVER EARLY" in T) == (ended is not None), "summary title does not match an early end")
    chk(f": {pk['name']}" in T, "summary does not name the pack")
    mr = re.search(r"Rating: (\d+)/10", T); chk(mr and int(mr.group(1)) == rating, f"rating {mr and mr.group(1)} != {rating}")
    chk(pk["verdicts"][vi] in T, f"verdict {pk['verdicts'][vi]!r} missing")
    ms_ = re.search(r"Style: ([SABCD]) \(avg (\d+) per answer\)", T)
    chk(ms_ and (ms_.group(1), int(ms_.group(2))) == (rank(avg), avg), f"style {ms_ and ms_.groups()} != {(rank(avg), avg)}")
    if word: chk(f"Word: {word}, {'used (+100)' if word_used else 'not used'}" in T, "word line")
    if pk["meters"]:
        chk("Meters: " + " ".join(f"{pk['meters'][k][0]} {meters[k]}" for k in range(len(meters))) in T, "meters line")
    ma = re.search(r"Typing: (\d+)% accuracy, (\d+) wpm", T); chk(ma and abs(int(ma.group(1)) - acc) <= 1, f"accuracy {ma and ma.group(1)} != {acc:.0f}")
    mt = re.search(r"Timeouts: (\d+), not understood: (\d+)", T)
    chk(mt and (int(mt.group(1)), int(mt.group(2))) == (st["timeouts"], st["misses"]), f"timeouts/misses {mt and mt.groups()}")
    prev = world.best.get(pk["name"])
    if prev is None: exp_score = f"Score: {score} (first record)"
    elif score > prev: exp_score = f"Score: {score} NEW RECORD! (was {prev})"
    else: exp_score = f"Score: {score} (best {prev})"
    chk(exp_score in T, f"score line: expected {exp_score!r} in {T[-200:]!r}")
    if prev is None or score > prev: world.best[pk["name"]] = score
    if today:
        dprev = world.day.get((DATE, pk["name"]))
        if dprev is None or score > dprev: world.day[(DATE, pk["name"])] = score
        chk(f"Today: best {world.day[(DATE, pk['name'])]}" in T, "today line")
    g.send(b" "); g.pump(0.4)
    chk("choose who you are today" in g.text(), "did not return to the menu")
    return probs, dict(seen=seen, rating=rating, score=score, ended=ended, avg=avg, flags=flags, finale=finale_seen, games=games)

def minigame(g, act, lose=False):
    """Plays the minigame on screen and returns whether it was won. With lose,
    it waits for the time to run out instead."""
    kind = act["kind"]
    if lose:
        for _ in range(400):
            if "MISSED!" in g.text() or "ACT: " not in g.text(): return False
            g.pump(0.05)
        return False
    def done():
        T = g.text()
        return "DONE!" in T or "MISSED!" in T or "ACT: " not in T
    if kind == "mash":
        g.send(b" " * (act["n"] or 12) + b"    ", gap=0.01)
    elif kind == "lyrics":
        line = next(l for l in g.lines() if l.strip().startswith("SING: "))
        g.send(line.strip()[6:].encode(), gap=0.02)
    elif kind == "timing":
        prev = None
        for _ in range(600):
            if done(): break
            L = g.lines()
            tr = next((m.group(1) for l in L for m in [re.match(r"\s*\[([-=|]{20,})\]\s*$", l)] if m and "|" in m.group(1)), None)
            if tr:
                nx = tr.index("|"); lo = tr.find("="); hi = tr.rfind("=")
                if lo < 0: lo = hi = nx          # the needle covers the whole zone
                lo = min(lo, nx) if tr[nx - 1:nx] == "=" or tr[nx + 1:nx + 2] == "=" else lo
                hi = max(hi, nx) if tr[nx - 1:nx] == "=" or tr[nx + 1:nx + 2] == "=" else hi
                d = 0 if prev is None else (1 if nx > prev else -1 if nx < prev else 0)
                # press inside the zone, with room before the needle leaves it
                if (d > 0 and lo <= nx <= hi - 2) or (d < 0 and lo + 2 <= nx <= hi):
                    g.send(b" "); g.pump(0.15); prev = None; continue
                prev = nx
            g.pump(0.008)
    elif kind in ("cross", "catch"):
        for _ in range(1500):
            if done(): break
            L = g.lines()
            rows = [l[4:] for l in L if l[4:5] in "<>|" and l.rstrip().endswith("|") and len(l.rstrip()) > 8]
            if kind == "cross":
                me = next(((x, y) for y, r in enumerate(rows) for x, ch in enumerate(r) if ch == "@"), None)
                if not me: g.pump(0.02); continue
                x, y = me; W = len(rows[0]) - 2
                def lane_safe(yy, reach):
                    r = rows[yy]; d = {"<": -1, ">": 1}.get(r[0], 0)
                    if d == 0: return True
                    cells = r[1:-1]
                    for k in range(0, reach + 1):
                        if cells[(x - 1 - d * k) % W] in "bc": return False
                    return True
                if lane_safe(y - 1, 3): g.send(b"w"); g.pump(0.05); continue
                if not lane_safe(y, 1) and lane_safe(y + 1, 2): g.send(b"s"); g.pump(0.05); continue
                g.pump(0.02)
            else:
                me = next(((x, y) for y, r in enumerate(rows) for x, ch in enumerate(r) if ch == "U"), None)
                apples = [(y, x) for y, r in enumerate(rows) for x, ch in enumerate(r) if ch == "o"]
                if not me or not apples: g.pump(0.02); continue
                ty, tx = max(apples)
                if tx > me[0]: g.send(b"d")
                elif tx < me[0]: g.send(b"a")
                g.pump(0.03)
    for _ in range(100):
        T = g.text()
        if "DONE!" in T: return True
        if "MISSED!" in T or "ACT: " not in T: return False
        g.pump(0.03)
    return False

def walk(g, goal):
    """Walks the terminal map to the goal: a fresh shortest path around shelves
    and carts after every step, one key press per step."""
    from collections import deque
    for _ in range(200):
        L = g.lines()
        if "GET TO" not in " ".join(L): return True
        rows = [(j, l[4:]) for j, l in enumerate(L) if l[4:5] == "#" and set(l[4:].rstrip()) <= set("# .0123456789Kkic@")]
        grid = [r for _, r in rows]
        me = next(((x, y) for y, r in enumerate(grid) for x, ch in enumerate(r) if ch == "@"), None)
        if me is None: return False
        prev = {me: None}; q = deque([me]); end = None
        while q:
            x, y = q.popleft()
            if grid[y][x] == goal and (x, y) != me: end = (x, y); break
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= ny < len(grid) and 0 <= nx < len(grid[ny]) and grid[ny][nx] not in "#Kic" and (nx, ny) not in prev:
                    prev[(nx, ny)] = (x, y); q.append((nx, ny))
        if end is None: g.pump(0.1); continue          # a cart in the way: wait
        step = end
        while prev[step] != me: step = prev[step]
        key = {(1, 0): b"d", (-1, 0): b"a", (0, 1): b"s", (0, -1): b"w"}[(step[0] - me[0], step[1] - me[1])]
        g.send(key); g.pump(0.03)
    return False

def strategy(kind):
    techs = ["fancy", "plain", "typo", "backspace", "huh", "ctrlw", "shout", "word", "sentence", "timeout"]
    def fn(i, sc, v):
        marks = [c["mark"] for c in v["choices"]]
        tm = v["timeout"][1] if v["mode"] == "text" else None
        if kind == "mixed": want, tech = ("best" if i % 2 == 0 else "worst"), techs[i % len(techs)]
        else: want, tech = kind.split("-")
        if tech == "timeout": return None, "timeout"
        if want == "best":
            t = marks.index(max(marks))
            if tm is not None and tm > max(marks): return None, "timeout"
        else:
            t = marks.index(min(marks))
            if tm is not None and tm < min(marks): return None, "timeout"
        return t, tech
    fn.lose_games = kind.startswith("worst")
    return fn

# ------------------------------------------------------------------ files on disk

def data_file(home, name): return os.path.join(home, ".local/share/typed_stories", name)

def load_heat(home):
    h, p = {}, data_file(home, "style.txt")
    if os.path.exists(p):
        for line in open(p):
            a, b = line.rstrip("\n").split(" ", 1); h[b] = float(a)
    return h

def check_files(home, world):
    probs = []
    disk = load_heat(home)
    mirror = {k: round(v, 4) for k, v in world.heat.items() if v >= 0.01}
    bad = {k: (disk.get(k), mirror.get(k)) for k in set(disk) | set(mirror) if abs(disk.get(k, 0) - mirror.get(k, 0)) > 1e-3}
    if bad: probs.append(f"style.txt differs from the model: {bad}")
    recs = {"best": {}, "day": {}, "word": set()}
    p = data_file(home, "records.txt")
    if os.path.exists(p):
        for line in open(p):
            f = line.rstrip("\n").split("\t")
            if f[0] == "best": recs["best"][f[1]] = int(f[2])
            elif f[0] == "day": recs["day"][(f[1], f[2])] = int(f[3])
            elif f[0] == "word": recs["word"].add(f[1])
    if recs["best"] != world.best: probs.append(f"records best {recs['best']} != {world.best}")
    if recs["day"] != world.day: probs.append(f"records day {recs['day']} != {world.day}")
    if recs["word"] != world.word_days: probs.append(f"records word days {recs['word']} != {world.word_days}")
    print(f"  files: style.txt {len(disk)} phrases, records {len(recs['best'])} best / {len(recs['day'])} day / {len(recs['word'])} word days: {'OK' if not probs else probs}")
    return probs

# ------------------------------------------------------------------ test modes

def main(mode, home):
    allp = []
    def run(g, world, pf, key, kind, label=None, **kw):
        print(f"===== {label or pf + ' ' + kind}")
        p, info = play_run(g, world, pf, key, strategy(kind), **kw)
        allp.extend(f"[{label or kind}] {x}" for x in p)
        return info
    if mode == "days":
        w = World(); g = Game(home)
        for kind in ("best-fancy", "worst-plain", "mixed"):
            info = run(g, w, "01-ordinary.txt", "1", kind, log=True)
            print(f"  rating {info['rating']}, score {info['score']}, ended {info['ended']}")
            if kind == "best-fancy" and info["ended"] is not None: allp.append("a perfect day ended early")
        g.quit(); allp += check_files(home, w)
    elif mode == "decay":
        w = World(); g = Game(home); avgs = []
        # Today mode: the same scenes every day, so only the wear on the phrases changes
        for day in (1, 2): avgs.append(run(g, w, "02-knight.txt", "2", "best-fancy", f"knight day {day}", today=True)["avg"])
        g.quit()
        w.heat = {k: round(v, 4) for k, v in load_heat(home).items()}
        g = Game(home); avgs.append(run(g, w, "02-knight.txt", "2", "best-fancy", "knight day 3 (after restart)", today=True)["avg"]); g.quit()
        print(f"  style avg per day: {avgs}")
        if not avgs[0] > avgs[1] > avgs[2]: allp.append(f"style did not decay: {avgs}")
        allp += check_files(home, w)
    elif mode == "narrow":
        w = World(); g = Game(home, cols=40)
        run(g, w, "01-ordinary.txt", "1", "mixed", "ordinary mixed, 40 columns"); g.quit()
    elif mode == "consequences":
        w = World(); g = Game(home); unlocked, conds, endings = set(), set(), []
        pk = parse_pack(f"{PACKS}/01-ordinary.txt", parse_lex(f"{PACKS}/lexicon.lex"))
        for n, kind in enumerate(["worst-plain", "best-fancy", "worst-fancy", "mixed"] * 3):
            info = run(g, w, "01-ordinary.txt", "1", kind, f"day {n + 1}: {kind}")
            titles = {t for t, _ in info["seen"]}
            unlocked |= {s["title"] for s in pk["scen"] if s["needs"] and s["title"] in titles}
            conds |= {d for t, d in info["seen"] for s in pk["scen"] if s["title"] == t for v in s["variants"] if v["cond"] and v["detail"] == d}
            if info["ended"] is not None: endings.append(pk["meters"][info["ended"]][0])
            if kind.startswith("best") and info["ended"] is not None: allp.append(f"day {n + 1}: a best day ended early")
            print(f"  rating {info['rating']}, score {info['score']}, ended: {pk['meters'][info['ended']][0] if info['ended'] is not None else 'no'}")
        g.quit()
        allu = {s["title"] for s in pk["scen"] if s["needs"] and "wordsmith" not in json.dumps(s["needs"])}
        allc = {v["detail"] for s in pk["scen"] for v in s["variants"] if v["cond"]}
        print(f"  unlocked scenes seen {len(unlocked & allu)}/{len(allu)}, conditional variants {len(conds & allc)}/{len(allc)}, early endings: {endings or 'none'}")
        allp += check_files(home, w)
    elif mode == "today":
        days, plays = [], []
        for n in range(2):
            h = os.path.join(home, f"player{n}"); os.makedirs(h)
            w = World(); g = Game(h)
            info = run(g, w, "01-ordinary.txt", "1", "best-fancy", f"player {n + 1}, today", today=True)
            days.append(info["seen"]); plays.append(info["games"])
            g.quit()
        # the same day for both, as long as they play the same: a minigame one of
        # them lost changes what comes after it, as it should
        cut = min([a[0] + 1 for a, b in zip(*plays) if a != b] + [len(days[0]), len(days[1])])
        if days[0][:cut] != days[1][:cut]: allp.append(f"two players got different days:\n{days[0]}\n{days[1]}")
        h = os.path.join(home, "other_date"); os.makedirs(h)
        g = Game(h, date="2026-12-01"); w = World()
        other = run(g, w, "01-ordinary.txt", "1", "best-fancy", "another date, today", today=True)["seen"]; g.quit()
        print(f"  same date, same day: {days[0][:cut] == days[1][:cut]} (compared {cut} scenes, up to the first minigame they played differently); another date differs: {other != days[0]}")
        if other == days[0]: allp.append("another date gave the very same day")
        free = []
        for n in range(2):
            h = os.path.join(home, f"free{n}"); os.makedirs(h)
            g = Game(h); w = World()
            free.append(run(g, w, "01-ordinary.txt", "1", "best-fancy", f"free play {n + 1}")["seen"]); g.quit()
        print(f"  two free days differ: {free[0] != free[1]}")
        if free[0] == free[1]: allp.append("two free-play days were identical")
    elif mode == "secret":
        os.makedirs(os.path.dirname(data_file(home, "x")), exist_ok=True)
        w = World()
        with open(data_file(home, "records.txt"), "w") as f:
            for d in ("2026-11-01", "2026-11-02", "2026-11-03", "2026-11-04"):
                f.write(f"word\t{d}\n"); w.word_days.add(d)
        g = Game(home)
        info = run(g, w, "01-ordinary.txt", "1", "best-word", "4 word days: no secret yet")
        secret = "A letter with no stamp lies on your doormat."
        if secret in {t for t, _ in info["seen"]}: allp.append("the secret scene came after only 4 word days")
        if DATE not in w.word_days: allp.append("using the word did not count today")
        info = run(g, w, "01-ordinary.txt", "1", "best-fancy", "5 word days: the secret scene")
        print(f"  secret scene shown: {secret in {t for t, _ in info['seen']}}")
        if secret not in {t for t, _ in info["seen"]}: allp.append("the secret scene did not appear after 5 word days")
        g.quit(); allp += check_files(home, w)
    elif mode == "stories":
        # every story, many days: finales by rating and by flags, early endings
        for pf, key in (("01-ordinary.txt", "1"), ("02-knight.txt", "2"), ("03-unit.txt", "3")):
            pk = parse_pack(f"{PACKS}/{pf}", parse_lex(f"{PACKS}/lexicon.lex"))
            h = os.path.join(home, key); os.makedirs(h)      # its own style and records
            w = World(); g = Game(h); finales, endings = [], []
            for n, kind in enumerate(["best-fancy", "worst-plain", "mixed", "best-plain", "mixed", "worst-fancy", "mixed", "best-fancy"]):
                info = run(g, w, pf, key, kind, f"{pk['name']} day {n + 1}: {kind}")
                if info["finale"]: finales.append(info["finale"])
                if info["ended"] is not None: endings.append(pk["meters"][info["ended"]][0])
                if kind.startswith("best") and info["ended"] is not None: allp.append(f"{pf} day {n + 1}: a best day ended early")
            g.quit()
            print(f"  {pk['name']}: finales {finales}, early endings {endings or 'none'}")
            if not finales: allp.append(f"{pf}: no finale in 8 days")
    print("PROBLEMS:", allp if allp else "none")
    return allp

if __name__ == "__main__":
    modes = sys.argv[1:] or ["days", "decay", "narrow", "consequences", "today", "secret", "stories"]
    failed = []
    for mode in modes:
        home = tempfile.mkdtemp(prefix="typed_stories_play_")
        print(f"########## {mode}")
        failed += [f"{mode}: {p}" for p in main(mode, home)]
        shutil.rmtree(home, ignore_errors=True)
    print("ALL PROBLEMS:", failed if failed else "none")
    sys.exit(1 if failed else 0)
