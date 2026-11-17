#!/usr/bin/env python3
"""Mighty Arcade's browser page: the arrow keys move the cursor (they press C4 left, E4 right, C#4 up, D4 down) and
Enter or Space selects (A4), ahead of the page's own use of them. Run after ./build.sh --web (build.sh does)."""
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
const QG_ARROWS = { ArrowLeft: 7, ArrowRight: 11, ArrowUp: 8, ArrowDown: 9, Enter: 16, Space: 16 };
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
// the keys only reach a focused page: say so when it is not, and a click anywhere takes them back
const qgFocus = document.createElement("div");
qgFocus.textContent = "Click here to play with the keyboard (arrows move, Enter selects)";
qgFocus.style.cssText = "position:fixed;left:50%;top:12px;transform:translateX(-50%);z-index:99;padding:10px 18px;" +
  "border-radius:10px;background:#FFD23F;color:#0B0814;font:600 15px system-ui,sans-serif;cursor:pointer;" +
  "box-shadow:0 4px 16px rgba(0,0,0,.4);display:none";
document.body.appendChild(qgFocus);
const qgShow = () => { qgFocus.style.display = document.hasFocus() ? "none" : "block"; };
addEventListener("blur", qgShow);
addEventListener("focus", qgShow);
qgFocus.addEventListener("pointerdown", () => { window.focus(); qgFocus.style.display = "none"; });
setInterval(qgShow, 500);
"""
s = s[:j] + add + s[j:]
open(p, "w").write(s)
print("arrows added")
