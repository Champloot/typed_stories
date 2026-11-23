#!/usr/bin/env python3
"""Play test: plays whole days through a terminal emulator and checks every
screen against a Python model of the game (matching, style, flags, rating).

    python3 tests/play_test.py              all checks below (a few minutes)
    python3 tests/play_test.py days         three ordinary days: best, worst, mixed
    python3 tests/play_test.py decay        knight three times: repeated words pay less
    python3 tests/play_test.py narrow       a mixed day in a 40-column terminal
    python3 tests/play_test.py consequences eight days, unlocked scenes and conditional variants

Needs pyte (pip install pyte, or python3Packages.pyte in Nix).
"""
import os, pty, select, time, re, sys, fcntl, termios, struct, json, signal, tempfile, shutil
try:
    import pyte
except ImportError:
    sys.exit("play_test needs pyte: pip install pyte (or add python3Packages.pyte to the dev shell)")

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
norm_ws = lambda t: re.sub(r"\s+", " ", t).strip()

def normalize(s):
    out = []; space = False
    for ch in s:
        if ch == "'": continue
        if ch.isascii() and ch.isalnum():
            if space and out: out.append(" ")
            space = False; out.append(ch.lower())
        else: space = True
    return "".join(out)[:60]

def push(lst, raw, text, tier):
    for p in lst:
        if p[0] == text:
            p[1] = max(p[1], tier); return
    lst.append([text, tier, raw])

def parse_lex(path):
    lex = {}; cur = None
    if not os.path.exists(path): return lex
    for raw in open(path):
        line = raw.strip()
        if not line or line.startswith("#"): continue
        k, v = [x.strip() for x in line.split(":", 1)]
        if k == "intent":
            if cur: push(cur["ph"], cur["label"], normalize(cur["label"]), 1)
            cur = lex[v] = dict(label=None, ph=[])
        elif k == "hint": cur["label"] = v
        else:
            for part in v.split("|"):
                part = part.strip()
                push(cur["ph"], part, normalize(part), int(k))
    if cur: push(cur["ph"], cur["label"], normalize(cur["label"]), 1)
    return lex

def parse_pack(path, lex):
    pk = dict(name=None, verdicts=[], phases=[], scen=[]); cur = None
    def newvar(detail=None):
        cur["variants"].append(dict(detail=detail, choices=[], timeout=None, mode="text", tsets=[], cond=None, last=None))
        return cur["variants"][-1]
    def var():
        if not cur["variants"]: newvar()
        return cur["variants"][-1]
    def cond(v):
        out = []
        for clause in v.split(","):
            lits = []
            for lit in clause.split(" or "):
                lit = lit.strip(); neg = lit.startswith("!")
                lits.append((lit.lstrip("!").strip(), neg))
            out.append(lits)
        return out
    for raw in open(path):
        line = raw.strip()
        if not line or line.startswith("#"): continue
        k, v = [x.strip() for x in line.split(":", 1)]
        if k == "pack": pk["name"] = v
        elif k == "verdicts": pk["verdicts"] = [norm_ws(x) for x in v.split("|")]
        elif k == "phases":
            for part in v.split("|"):
                bits = part.strip().rsplit(" ", 1)
                pk["phases"].append((bits[0].strip(), int(bits[1])) if len(bits) == 2 and bits[1].isdigit() else (part.strip(), 0))
        elif k == "scenario": cur = dict(title=norm_ws(v), time=0, phase=None, slot="any", variants=[], needs=None); pk["scen"].append(cur)
        elif k == "needs": cur["needs"] = cond(v)
        elif k == "if": var()["cond"] = cond(v)
        elif k == "sets":
            vv = var(); names = [x.strip() for x in v.split(",")]
            (vv["tsets"] if vv["last"] == "timeout" else vv["choices"][-1]["sets"]).extend(names)
        elif k == "time": cur["time"] = float(v)
        elif k == "phase": cur["phase"] = v
        elif k == "slot": cur["slot"] = v
        elif k == "variant": newvar(norm_ws(v) or None)
        elif k == "on_timeout": var()["mode"] = v
        elif k == "weight": var()
        elif k == "timeout":
            p = [x.strip() for x in v.split("|")]; var()["timeout"] = (norm_ws(p[0]), int(p[1]) if len(p) > 1 else -1); var()["last"] = "timeout"
        elif k == "choice":
            p = [x.strip() for x in v.split("|")]
            c = dict(label=None, ph=[], result=norm_ws(p[1]), mark=int(p[2]) if len(p) > 2 else 0, sets=[])
            if len(p) > 3: c["label"] = p[3]; push(c["ph"], p[3], normalize(p[3]), 1)
            for tok in [t.strip() for t in p[0].split("+")]:
                if tok in lex:
                    for text, tier, rawp in lex[tok]["ph"]: push(c["ph"], rawp, text, tier)
                    c["label"] = c["label"] or lex[tok]["label"]
                else:
                    push(c["ph"], tok, normalize(tok), 1); c["label"] = c["label"] or tok
            var()["choices"].append(c); var()["last"] = "choice"
    return pk

def holds(c, flags):
    return all(any((name in flags) != neg for name, neg in clause) for clause in c)

def open_variants(sc, flags):
    conds = [v for v in sc["variants"] if v["cond"] and holds(v["cond"], flags)]
    return conds if conds else [v for v in sc["variants"] if not v["cond"]]

def scen_open(sc, flags):
    if sc["needs"] and not holds(sc["needs"], flags): return False
    return any(not v["cond"] or holds(v["cond"], flags) for v in sc["variants"])

def lev(a, b):
    prev = list(range(len(b) + 1))
    for i in range(1, len(a) + 1):
        cur = [i] + [0] * len(b)
        for j in range(1, len(b) + 1):
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1]))
        prev = cur
    return prev[len(b)]

def match(v, inp):
    for ci, c in enumerate(v["choices"]):
        for text, tier, _ in c["ph"]:
            if text == inp: return (ci, tier, 0, text)
    L = len(inp)
    if L < 4: return None
    cap = 2 if L >= 8 else 1
    best, who, tie, tier, ph = cap + 1, -1, False, 0, None
    for ci, c in enumerate(v["choices"]):
        for text, t, _ in c["ph"]:
            d = lev(inp, text) if abs(len(inp) - len(text)) <= cap else cap + 1
            d = min(d, cap + 1) if d > cap else d
            if d < best: best, who, tie, tier, ph = d, ci, False, t, text
            elif d == best and d <= cap:
                if ci != who: tie = True
                elif t > tier: tier, ph = t, text
    if who >= 0 and best <= cap and not tie: return (who, tier, 1, ph)
    return None

BASE = {1: 10, 2: 30, 3: 60}
def pts(tier, typo, heat):
    p = BASE[tier] / (1.0 + heat)
    if typo: p *= 0.5
    return int(p + 0.5)
rank = lambda p: "S" if p >= 50 else "A" if p >= 35 else "B" if p >= 20 else "C" if p >= 10 else "D"

class Game:
    def __init__(self, home, cols=80, rows=40):
        self.screen = pyte.Screen(cols, rows); self.stream = pyte.ByteStream(self.screen)
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
            os.chdir(PROJ); os.environ["TERM"] = "xterm"; os.environ["HOME"] = home
            os.environ.pop("XDG_DATA_HOME", None); os.environ.pop("LINES", None); os.environ.pop("COLUMNS", None)
            os.execv(f"{PROJ}/game", ["./game"])
        self.pump(0.5)
    def pump(self, w):
        end = time.time() + w
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.03)
            if r:
                try: self.stream.feed(os.read(self.fd, 65536))
                except OSError: return
    def lines(self): return [l.rstrip() for l in self.screen.display]
    def send(self, b, gap=0.02):
        for c in b: os.write(self.fd, bytes([c])); time.sleep(gap)
    def quit(self):
        self.send(b"q"); t0 = time.time()
        while time.time() - t0 < 3:
            pid, st = os.waitpid(self.pid, os.WNOHANG)
            if pid: return True
            self.pump(0.1)
        os.kill(self.pid, signal.SIGKILL); os.waitpid(self.pid, 0); return False

def block(L, i):
    out = []
    while i < len(L) and L[i].strip(): out.append(L[i].strip()); i += 1
    return norm_ws(" ".join(out)), i

def play_run(g, packfile, key, plan_fn, heat, hints=True, log=True):
    lex = parse_lex(f"{PROJ}/packs/lexicon.lex"); pk = parse_pack(f"{PROJ}/packs/{packfile}", lex)
    index = {}
    for sc in pk["scen"]:
        for v in sc["variants"]:
            index[norm_ws(sc["title"] + (" " + v["detail"] if v["detail"] else ""))] = (sc, v)
    probs = []
    st = dict(marks=0, best=0, worst=0, typed=0, erased=0, timeouts=0, misses=0, style=0, n=0)
    for k in list(heat): heat[k] *= 0.5                     # new run: everything cools down
    flags = set(); phase_names = [n.upper() for n, _ in pk["phases"]]
    cur_phase = [None]; snap = [set()]; seen_in_phase = []; last_header = [None]; played = [0]
    unlocked_seen = []
    def check_phase(name, snapshot, seen):
        ph = next(n for n, _ in pk["phases"] if n.upper() == name); pick = dict(pk["phases"])[ph]
        reg = [s_ for s_ in pk["scen"] if s_["phase"] == ph and not s_["needs"] and scen_open(s_, snapshot)]
        unl = {s_["title"] for s_ in pk["scen"] if s_["phase"] == ph and s_["needs"] and scen_open(s_, snapshot)}
        got_unl = {t for t, needs in seen if needs}; got_reg = [t for t, needs in seen if not needs]
        exp_reg = min(pick, len(reg)) if pick else len(reg)
        if got_unl != unl: probs.append(f"phase {name}: unlocked scenes {sorted(got_unl)} != expected {sorted(unl)}")
        if len(got_reg) != exp_reg: probs.append(f"phase {name}: {len(got_reg)} regular scenes, expected {exp_reg}")
    def skipped_phases(upto_idx):
        start = phase_names.index(cur_phase[0]) + 1 if cur_phase[0] else 0
        for name in phase_names[start:upto_idx]:
            ph = next(n for n, _ in pk["phases"] if n.upper() == name)
            if any(s_["phase"] == ph and scen_open(s_, flags) for s_ in pk["scen"]):
                probs.append(f"phase {name} was skipped but had open scenes")
    g.send(key.encode()); g.pump(0.4)
    i = 0
    while True:
        L = g.lines()
        if any("RUN COMPLETE" in l for l in L): break
        if not any("Press Enter when ready" in l for l in L): probs.append(f"#{i}: no intro: {norm_ws(' '.join(L))[:100]!r}"); break
        text, _ = block(L, 3)
        if text not in index: probs.append(f"#{i}: unknown screen {text!r}"); break
        sc, v = index[text]
        hm = re.match(r"\s*\[(\d+)/(\d+)\]\s*(.*)$", L[1])
        if not hm or int(hm.group(1)) != played[0] + 1: probs.append(f"#{i}: header {L[1]!r}")
        ph_name = hm.group(3).strip() if hm else None; last_header[0] = hm
        if pk["phases"] and ph_name != cur_phase[0]:
            if cur_phase[0]: check_phase(cur_phase[0], snap[0], seen_in_phase)
            skipped_phases(phase_names.index(ph_name))
            cur_phase[0] = ph_name; snap[0] = set(flags); seen_in_phase.clear()
        if sc["phase"] and sc["phase"].upper() != ph_name: probs.append(f"#{i}: {sc['title'][:30]!r} shown under {ph_name}")
        seen_in_phase.append((sc["title"], bool(sc["needs"])))
        if sc["needs"]:
            unlocked_seen.append(sc["title"])
            if not holds(sc["needs"], snap[0]): probs.append(f"#{i}: unlocked scene {sc['title'][:30]!r} shown without its flags {sorted(flags)}")
        ov = open_variants(sc, flags)
        if v not in ov: probs.append(f"#{i}: variant {v['detail']!r} shown, but open variants are {[x['detail'] for x in ov]} (flags {sorted(flags)})")
        played[0] += 1
        if v["cond"]: g.cond_variants = getattr(g, "cond_variants", set()) | {v["detail"]}
        ms = [c["mark"] for c in v["choices"]] + ([v["timeout"][1]] if v["mode"] == "text" else [])
        st["best"] += max(ms); st["worst"] += min(ms); st["n"] += 1
        g.send(b"\n"); g.pump(0.35)
        L = g.lines()
        inp_row = next((j for j, l in enumerate(L) if l.startswith("    > ")), None)
        if inp_row is None: probs.append(f"#{i}: no input line"); break
        sit_end = block(L, 3)[1]
        hint_text = norm_ws(" ".join(l.strip() for l in L[sit_end:inp_row] if l.strip()))
        exp_hint = norm_ws("  /  ".join(c["label"] for c in v["choices"]))
        if hints and hint_text != exp_hint: probs.append(f"#{i}: hint {hint_text!r} != {exp_hint!r}")
        if not hints and hint_text: probs.append(f"#{i}: hints off but shown {hint_text!r}")

        target, technique = plan_fn(i, sc, v)
        tag = f"#{i:2d} {technique:9s} {sc['title'][:26]!r:29s} {(v['detail'] or '-')[:22]!r:25s}"
        if target is None:
            t0 = time.time()
            while time.time() - t0 < sc["time"] + 3:
                g.pump(0.2)
                if any("TIME'S UP" in l for l in g.lines()): break
            L = g.lines(); st["timeouts"] += 1
            a = next((j for j, l in enumerate(L) if "TIME'S UP" in l), None)
            if a is None: probs.append(f"{tag}: no timeout"); break
            b = next(j for j, l in enumerate(L) if "(press any key)" in l)
            txt = norm_ws(" ".join(l.strip() for l in L[a + 1:b] if l.strip()))
            if v["mode"] == "text":
                ok = txt == v["timeout"][0]; mk = v["timeout"][1]
            else:
                rest = txt[len(v["timeout"][0]):].strip()
                hit = [c["mark"] for c in v["choices"] if c["result"] == rest]; ok = bool(hit); mk = hit[0] if hit else 0
            st["marks"] += mk
            flags.update(v["tsets"])
            if v["mode"] != "text" and ok: flags.update(next(c["sets"] for c in v["choices"] if c["result"] == rest))
            if log: print(f"{tag} timeout ({mk:+d}) {'OK' if ok else 'MISMATCH'}  flags+{v['tsets']}")
            if not ok: probs.append(f"{tag}: timeout text {txt!r}")
            g.send(b" "); g.pump(0.35); i += 1; continue

        c = v["choices"][target]
        by_tier = sorted(c["ph"], key=lambda p: -p[1])
        def exact_ok(raw):
            m = match(v, normalize(raw)); return m and m[0] == target and m[2] == 0
        fancy = next(p[2] for p in by_tier if exact_ok(p[2]))
        plain = c["label"]
        typed_text = fancy if technique not in ("plain",) else plain
        keys = []          # (bytes, counts as typed, counts as erased)
        if technique == "typo":
            cand = None
            for text, tier, rawp in by_tier:
                if len(text) >= 6:
                    mid = len(text) // 2
                    t2 = text[:mid] + text[mid + 1:]
                    m = match(v, normalize(t2))
                    if m and m[0] == target and m[2] == 1: cand = t2; break
            typed_text = cand or fancy
        if technique == "shout":
            typed_text = typed_text.upper() + "!!"
        if technique == "huh":
            g.send(b"qzxv"); st["typed"] += 4; g.send(b"\n"); g.pump(0.3)
            Lh = g.lines(); st["misses"] += 1
            if "Huh? Nobody gets 'qzxv'. Try again." not in norm_ws(" ".join(Lh)): probs.append(f"{tag}: no 'Huh' message")
            inp = next((l for l in Lh if l.startswith("    > ")), "")
            if inp.strip() != "> _": probs.append(f"{tag}: input not cleared after miss: {inp!r}")
        if technique == "backspace":
            g.send(typed_text.encode() + b"xq"); st["typed"] += len(typed_text) + 2
            g.send(b"\x7f\x7f"); st["erased"] += 2
        elif technique == "ctrlw":
            g.send(b"blah wrong"); st["typed"] += 10
            g.send(b"\x17"); st["erased"] += 5      # "wrong"
            g.send(b"\x17"); st["erased"] += 5      # "blah " (space then word)
            g.send(typed_text.encode()); st["typed"] += len(typed_text)
        else:
            g.send(typed_text.encode()); st["typed"] += len(typed_text)
        g.pump(0.1)
        inp = next((l for l in g.lines() if l.startswith("    > ")), "")
        if inp.strip() != "> " + typed_text + "_": probs.append(f"{tag}: input line {inp!r}, expected {typed_text!r}")
        g.send(b"\n"); g.pump(0.4)
        L = g.lines()
        m = match(v, normalize(typed_text))
        if not m or m[0] != target: probs.append(f"{tag}: mirror says {typed_text!r} -> {m}"); break
        _, tier, typo, phrase = m
        before = heat.get(phrase, 0.0)
        exp_pts = pts(tier, typo, before)
        heat[phrase] = before + 1.0
        q = next((j for j, l in enumerate(L) if l.strip() == f'"{typed_text}"'), None)
        sl = next((j for j, l in enumerate(L) if re.match(r"\s*style [SABCD] \+\d+", l)), None)
        if q is None or sl is None: probs.append(f"{tag}: result screen broken: {norm_ws(' '.join(L))[:140]!r}"); break
        reaction = norm_ws(" ".join(l.strip() for l in L[q + 1:sl] if l.strip()))
        note = block(L, sl)[0]
        sm = re.match(r"style ([SABCD]) \+(\d+)(\s+\(typo\))?(\s+\(heard that before\))?\s+\d+\.\ds$", note)
        got = (sm.group(1), int(sm.group(2)), bool(sm.group(3)), bool(sm.group(4))) if sm else None
        exp = (rank(exp_pts), exp_pts, bool(typo), before >= 0.5)
        ok = reaction == c["result"] and got == exp
        st["marks"] += c["mark"]; st["style"] += exp_pts; flags.update(c["sets"])
        if log: print(f"{tag} {typed_text!r:28s} -> {c['label']!r} tier {tier}{' typo' if typo else ''} heat {before:.2f}: style {got} {'OK' if ok else 'MISMATCH exp ' + str(exp) + ' ' + repr(reaction)}")
        if not ok: probs.append(f"{tag}: got {got} exp {exp}, reaction {reaction!r}")
        g.send(b" "); g.pump(0.35); i += 1

    if pk["phases"]:
        if cur_phase[0]: check_phase(cur_phase[0], snap[0], seen_in_phase)
        skipped_phases(len(phase_names))
    if last_header[0] and int(last_header[0].group(2)) != played[0]:
        probs.append(f"last header said total {last_header[0].group(2)}, but {played[0]} were played")
    if log or unlocked_seen: print(f"  flags at the end: {sorted(flags)}; unlocked scenes: {[t[:28] for t in unlocked_seen]}")
    g.unlocked = getattr(g, "unlocked", set()) | set(unlocked_seen)
    g.cond_variants = getattr(g, "cond_variants", set())
    final = norm_ws(" ".join(l for l in g.lines() if l.strip()))
    span, got = st["best"] - st["worst"], st["marks"] - st["worst"]
    exp_rating = (got * 20 + span) // (2 * span) if span > 0 else 10
    nv = len(pk["verdicts"]); vi = min(nv - 1, max(0, got * nv // span)) if span > 0 else nv - 1
    avg = (st["style"] + st["n"] // 2) // st["n"] if st["n"] else 0
    acc = 100 * (st["typed"] - st["erased"]) / st["typed"] if st["typed"] else 100
    print(f"  END: {final[:170]}")
    def chk(cond, what):
        if not cond: probs.append(what)
    mr = re.search(r"Rating: (\d+)/10", final); chk(mr and int(mr.group(1)) == exp_rating, f"rating {mr and mr.group(1)} != {exp_rating}")
    chk(pk["verdicts"][vi] in final, f"verdict {pk['verdicts'][vi]!r}")
    ms_ = re.search(r"Style: ([SABCD]) \(avg (\d+) per answer\)", final)
    chk(ms_ and ms_.group(1) == rank(avg) and int(ms_.group(2)) == avg, f"style {ms_ and ms_.groups()} != {(rank(avg), avg)}")
    ma = re.search(r"Typing: (\d+)% accuracy, (\d+) wpm", final); chk(ma and abs(int(ma.group(1)) - acc) <= 1, f"accuracy {ma and ma.group(1)} != {acc:.1f}")
    mt = re.search(r"Timeouts: (\d+), not understood: (\d+)", final)
    chk(mt and (int(mt.group(1)), int(mt.group(2))) == (st["timeouts"], st["misses"]), f"timeouts/misses {mt and mt.groups()} != {(st['timeouts'], st['misses'])}")
    g.send(b" "); g.pump(0.4)
    chk("choose who you are today" in norm_ws(" ".join(g.lines())), "did not return to menu")
    return probs, exp_rating, avg

def strategy(kind):
    techs = ["fancy", "plain", "typo", "backspace", "huh", "ctrlw", "shout", "timeout"]
    def fn(i, sc, v):
        marks = [c["mark"] for c in v["choices"]]
        tm = v["timeout"][1] if v["mode"] == "text" else None
        if kind == "mixed":
            tech = techs[i % len(techs)]
            want = "best" if i % 2 == 0 else "worst"
        else:
            want, tech = kind.split("-")
        if tech == "timeout": return None, "timeout"
        if want == "best":
            t = marks.index(max(marks))
            if tm is not None and tm > max(marks): return None, "timeout"
        else:
            t = marks.index(min(marks))
            if tm is not None and tm < min(marks): return None, "timeout"
        return t, tech
    return fn

def load_heatfile(home):
    p = f"{home}/.local/share/typed_stories/style.txt"; h = {}
    if os.path.exists(p):
        for line in open(p):
            a, b = line.rstrip("\n").split(" ", 1); h[b] = float(a)
    return h

def main(mode, home):
    allp = []
    os.makedirs(home, exist_ok=True)
    if mode == "days":
        heat = {}
        g = Game(home)
        for kind in ("best-fancy", "worst-plain", "mixed"):
            print(f"===== ordinary {kind}")
            p, r, a = play_run(g, "01-ordinary.txt", "1", strategy(kind), heat); allp += [f"[{kind}] {x}" for x in p]
        g.quit()
        disk = load_heatfile(home)
        mirror = {k: round(v, 4) for k, v in heat.items() if v >= 0.01}
        bad = {k: (disk.get(k), mirror.get(k)) for k in set(disk) | set(mirror) if abs(disk.get(k, 0) - mirror.get(k, 0)) > 1e-3}
        print(f"style file: {len(disk)} phrases, mismatches vs mirror: {bad or 'none'}")
        if bad: allp.append(f"style file mismatch {bad}")
    elif mode == "decay":
        heat = {}
        g = Game(home)
        for day in (1, 2):
            print(f"===== knight best-fancy, day {day} (same process)")
            p, r, a = play_run(g, "02-knight.txt", "2", strategy("best-fancy"), heat, log=(day == 2)); allp += p
            print(f"  day {day}: style avg {a}")
        g.quit()
        heat = {k: round(v, 4) for k, v in load_heatfile(home).items()}   # what the next process will read
        g = Game(home)
        print("===== knight best-fancy, day 3 (after restart)")
        p, r, a = play_run(g, "02-knight.txt", "2", strategy("best-fancy"), heat, log=False); allp += p
        print(f"  day 3: style avg {a}")
        g.quit()
    elif mode == "consequences":
        heat = {}; g = Game(home)
        for kind in ("worst-plain", "best-fancy", "worst-fancy", "mixed", "worst-plain", "best-plain", "worst-plain", "mixed"):
            print(f"===== ordinary {kind}")
            p, r, a = play_run(g, "01-ordinary.txt", "1", strategy(kind), heat, log=False); allp += [f"[{kind}] {x}" for x in p]
        g.quit()
        pk = parse_pack(f"{PROJ}/packs/01-ordinary.txt", parse_lex(f"{PROJ}/packs/lexicon.lex"))
        allu = {s_["title"] for s_ in pk["scen"] if s_["needs"]}
        allc = {v["detail"] for s_ in pk["scen"] for v in s_["variants"] if v["cond"]}
        print(f"unlocked scenes seen: {len(g.unlocked & allu)}/{len(allu)}; missing: {[t[:40] for t in allu - g.unlocked]}")
        print(f"conditional variants seen: {len(g.cond_variants & allc)}/{len(allc)}; missing: {[t[:40] for t in allc - g.cond_variants]}")
    elif mode == "narrow":
        heat = load_heatfile(home)
        g = Game(home, cols=40)
        if "hints:" in norm_ws(" ".join(g.lines())): allp.append("menu still shows a hints toggle")
        print("===== ordinary mixed, 40 cols")
        p, r, a = play_run(g, "01-ordinary.txt", "1", strategy("mixed"), heat, log=False); allp += p
        g.quit()
    print("PROBLEMS:", allp if allp else "none")
    return allp

if __name__ == "__main__":
    modes = sys.argv[1:] or ["days", "decay", "narrow", "consequences"]
    failed = []
    for mode in modes:
        home = tempfile.mkdtemp(prefix="typed_stories_play_")
        print(f"########## {mode}")
        failed += [f"{mode}: {p}" for p in main(mode, home)]
        shutil.rmtree(home, ignore_errors=True)
    print("ALL PROBLEMS:", failed if failed else "none")
    sys.exit(1 if failed else 0)
