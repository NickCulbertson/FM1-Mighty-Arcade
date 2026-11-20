#!/usr/bin/env python3
"""Mighty Arcade's browser page, in its front page's look (QG_STYLE: black, cyan and pink, the hint to use the arrows
and Space, under the panel). The arrow keys move the cursor (they press the pad at the keyboard's bottom end: F3 left, A3 right,
F#3 up, G3 down) and Enter or Space selects (F5, at its top end), ahead of the page's own use of them. Run after ./build.sh --web (build.sh does)."""
import sys
p = sys.argv[1]
s = open(p).read()
anchor = 'Object.assign(KEYMAP, { KeyP: { btn: BTN.PLAY }'
if "QG_ARROWS" in s:
    sys.exit(0)
i = s.index(anchor)
j = s.index("\n", i) + 1
add = """// QG_ARROWS (Mighty Arcade): the arrows move the cursor, Enter or Space selects; caught first, so a focused button or
// the SELECT knob never gets them
const QG_ARROWS = { ArrowLeft: 0, ArrowRight: 4, ArrowUp: 1, ArrowDown: 2, Enter: 24, Space: 24 };
addEventListener("keydown", (e) => {
  const k = QG_ARROWS[e.code];
  if (k === undefined || !node || !shortcuts.checked || e.metaKey || e.ctrlKey || e.altKey) return;
  e.preventDefault();
  e.stopImmediatePropagation();
  if (!e.repeat) holdDown("q" + e.code, "key", k);
}, true);
addEventListener("keyup", (e) => {
  if (QG_ARROWS[e.code] === undefined) return;
  e.stopImmediatePropagation();
  letGo("q" + e.code);
}, true);
"""
s = s[:j] + add + s[j:]

# QG_STYLE: the page in the front page's colours (app/site/index.html), dark whatever the system's theme
STYLE = """<style id="QG_STYLE">
html:root:root { --ground: #000; --ink: #f1ecff; --muted: #a99bc9; --rule: #32204f; --accent: #ff2e97; --teal: #6ee1ff;
                 --card: #100a1c; --bad: #ff6b8b; --focus: #ffe200; color-scheme: dark; }
body { font-family: "Helvetica Neue", Helvetica, Arial, sans-serif; }
header { align-items: center; padding-bottom: .8rem; border-bottom: 1px solid var(--rule); margin-bottom: 1.2rem; }
h1 { font: 700 2.4rem/1 "Helvetica Neue", Helvetica, Arial, sans-serif; letter-spacing: .04em; text-transform: uppercase; }
h1 a { text-decoration: none; }
h1 .m { color: #6ee1ff; }
h1 .a { color: #ff2e97; }
p.qg-hint { margin: .9rem 0 0; color: var(--ink); }
p.qg-hint kbd { color: #6ee1ff; border-color: var(--rule); background: var(--card); }
.start button { background: #ff2e97; color: #000; font-family: "Helvetica Neue", Helvetica, Arial, sans-serif;
                text-transform: uppercase; letter-spacing: .05em; font-weight: 700; }
.start button:hover, .start button:focus-visible { background: #ffe200; }
.held .btnface, .held .keyface { stroke: #ff2e97; }
.latched .btnface, .latched .keyface { stroke: #6ee1ff; }
.bar button, .speak button { border-color: var(--rule); }
.bar button:hover, .speak button:hover { border-color: #ffe200; }
.speak summary h2 { color: #6ee1ff; text-transform: uppercase; letter-spacing: .06em; font-family: "Helvetica Neue", Helvetica, Arial, sans-serif; font-size: 1rem; }
</style>
"""
s = s.replace("</head>", STYLE + "</head>", 1)
s = s.replace("<h1>MightyArcade</h1>", "<h1>Mighty Arcade</h1>")   # (build.sh puts the space in after this)
s = s.replace("<h1>Mighty Arcade</h1>", '<h1><a href="../"><span class="m">Mighty</span> <span class="a">Arcade</span></a></h1>', 1)
s = s.replace('<div class="bar">', '<p class="qg-hint">Use the <kbd>arrows</kbd> to move and <kbd>Space</kbd> to select. '
              "If the keys don't respond, click the page first.</p>\n" + '<div class="bar">', 1)
open(p, "w").write(s)
print("arrows added")
