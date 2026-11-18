# Mighty Arcade

A firmware for the M-VAVE FM-1: eighteen casual games that make music as you play. It's built on fm1-base, the same
stack as Mighty Arp (`base/` is fm1-base, as a git submodule; the FM engine in `app/dsp/` is a copy of Mighty Arp's,
with `eng_note_on_with` added so several sounds can play at once).

```
MIGHTY ARCADE: a box of casual games that make music. CHESS: two engines, or you against one,
or two players; the board is the score. A playhead sweeps the files a to h,
 an eighth note each, a bar a sweep; each piece it passes may sound (DENSITY), its pitch from its rank, its voice from
 what it is: pawns plucked, knights a marimba, bishops a celesta, rooks a low electric piano, queens a glass pad, the
 king a drone. Black is an octave down. A move every bar or so (MOVE): the piece's from and to sung on vibraphone; a
 capture swells, a check pings, castling and promotion have their figures, mate rings the bells. As pieces come off
 the music thins out: the game's arc is the piece's.

   PLAY   pause / play on                 HOME   a new game
 SEL    YOU VS CPU: you play white. Keys: the pad at the bottom end (F3 left, A3 right, F#3 up, G3 down), select on F5, at the top (a piece,
        then where to; its moves are dotted; A on it again, or OCT-, lets go). The music goes on as you think
   K1 TEMPO   K2 DENSITY   K3 MOVE (every 1, 2 or 4 bars)   K4 BLUNDER (how often a player plays any old move)
   PRESET the scale   ALGO how deep they think (1..4)   keys: play along in the scale
```

## The games

The idea: you make music by playing, rather than listening to game music. In every game but chess, **K4 is EFFECTS**:
how loud the game's sound effects are. Turn it to 0 and only the music plays, in time, so the games play like an
instrument.

Under the games, playing well grows a soft haze of held chord tones (no fixed run up and down the
chord); big moments get a sting.

The sequencer games (SOLITAIRE, CUBE DROP, 2048, SAND WORM, 4 IN A ROW, MINES, BRICK BREAK, STAR SWARM, ROCK DRIFT, POOL): what you build on
the board is the sequence. A playhead plays it on the beat, a column an eighth note, and the further you get, the
more of the drums come in (hi-hats, kick, snare, the full kit, double time). Their knobs, each game its own: K1
TEMPO, K2 CHANCE (how many of the playhead's notes sound), K3 SOUND, K4 EFFECTS, PRESET the scale, ALGORITHM the key
(in 4 IN A ROW, ALGORITHM is the difficulty).

The menu, two pages of nine (right off the grid's edge for the second), grouped (17 Nov 2026):

    page 1, the favourites          page 2
    BOUNCE  SWARM       DRIFT          GOLF    PINBALL  DODGE
    CHESS   DICE SYNTH  SOLITAIRE      DARK    DROP     POOL
    WORM    CUBES       DEFUSE         ROWS    2048     CHECKERS

Short names (17 Nov 2026): DROP was NOTE DROP, BOUNCE was BRICK BREAK, SWARM was STAR SWARM, DRIFT was ROCK DRIFT,
WORM was SAND WORM, CUBES was CUBE DROP, PINBALL was BUMPIN BALL, GOLF was MINI GOLF, ROWS was 4 IN A ROW, DARK was LIGHTS.

The cursor starts on DICE SYNTH, in the middle. In the menu, PRESET picks the scale and ALGORITHM the key for every game (its name is in the
bottom corner; the new chord sounds as you turn). The tests find games by name (`expect menu_to_CUBE_DROP 1`,
`expect mode_is_CUBE_DROP 1`), so the order can change without them.

A menu of nine a page (HOME comes back to it), and a PLAYERS switch above it: 1 (against the computer, or alone) or 2 (taking
turns). The same keys drive them all, held like a game controller: the pad under the left hand at the keyboard's bottom end (F3 left, F#3 up, G3 down, A3 right) and select under the right hand on F5, at its top end (the TOP KEY in the games' hints) (in the browser:
the arrow keys and Enter or Space). A held direction repeats. Each game makes its own music from what you do.

- **CHESS**: you (white) against the computer, or two players. ALGORITHM sets the difficulty: EASY, MEDIUM, HARD,
  EXPERT. SEL lets the computer play itself; PLAY pauses. The board is the score.
- **CHECKERS**: American rules (taking is a must, multi-jumps, kings). A on a piece, then A where it lands (a
  multi-jump: its last square). Every move plays out: a step slides, a jump hops over the piece it takes, which
  bursts as it's cleared, each landing on its note. A playhead sweeps the board as in chess.
- **4 IN A ROW**: left and right pick a column, A drops. A column is a note, a row is higher up the scale. The
  playhead walks the seven columns and a rest, each playing its top disc (yours and theirs an octave apart).
- **SOLITAIRE** (one player): Klondike, one card a draw. PLAY deals again. It is a sequencer, almost ambient: a
  playhead sweeps the columns, then the waste, an eighth note each, and by chance each top card plays its rank on long,
  reverby keys (A low, K high, always a note of the pad's chord). The more cards you get home, the more (soft) drums
  come in: hi-hats, kick, snare, the full kit, then double time. Its knobs: K1 TEMPO, K2 CHANCE, K3 SOUND, K4 EFFECTS,
  PRESET the scale, ALGORITHM the key.
- **CUBE DROP** (reworked 17 Nov 2026 into a colour puzzle, nothing like the falling-block game): pairs and threes
  of coloured cubes fall; up turns, down is faster, A drops. Four or more touching cubes of one colour POP, the
  cubes above fall, and if that makes another group it pops too: a CHAIN, each link higher up the chord. Each level
  begins with NOTE GEMS set in the well; pop a gem in a group of its colour and it plays its note. Clear every gem
  to finish the level (more gems each level, the fall quicker). A chain of two or more earns a BOMB, which blasts a
  hole five wide and three deep, gems and all. A playhead walks the columns, each playing its top cube (a gem, its
  note).
- **2048**: slide; equal tiles merge and climb the scale, several merges at once a chord. The playhead sweeps the
  four columns twice a bar, each column's tiles a chord (the bigger the tile, the higher).
- **SAND WORM** (the snake game): a banded worm burrowing through dunes at dusk. It moves on the groove's eighth notes,
  and on each its body plays the notes it has eaten, tail to head: the longer it grows, the longer its melody. Each
  ring is banded with its note's colour; its toothed maw opens on the beat.
- **DICE SYNTH** (one player; from the Dice Synth prototype): roll a die, get a new FM sound with a name, play it on
  every key. PLAY rolls (the only roll: a one on the die curses it, anything goes); EDIT, ENV, LFO, FX lock the TIMBRE, SHAPE, MOTION, SPACE through the
  rolls; PRESET the die (ANY, KEYS, BELLS, PLUCKS, LEADS, PADS, BASS, PERC, WEIRD); SELECT back through the last
  rolls; K1..K4 BRIGHT, LENGTH, SPACE, WOBBLE; ARP plays a few notes of it (a roll itself is silent); SAVE keeps it (saved), SEQ steps through the kept
  ones; OCT- OCT+ the octave. No groove under it: it's an instrument. (It replaced ECHO, 16 Nov 2026.)
- **DEFUSE** (was MINES): 12 x 9, in fields: the first has 10 mines, each field you clear the next has three more (up to 31);
  a boom replays the same field. A opens (the first is always safe), A held flags. The playhead walks the twelve
  columns, each playing its highest number opened (1 low, 8 high).

SUDOKU and SEA BATTLE were cut, 17 Nov 2026.

- **DARK** (was LIGHTS; one player): A flips a light and its neighbours; turn them all off. Its music is ambient: a slow
  chord every two bars in a big room, and the lights still on sound now and then, soft and long, so solving it
  quiets the music down to the chord alone.
- **BRICK BREAK**, **STAR SWARM**, **ROCK DRIFT**: held keys (left, right, up, down), A to launch or
  fire. Original names and designs. All three are sequencers, like SOLITAIRE. In ROCK DRIFT a rock never touches the
  ship: its field turns rocks away, bouncing them off with a soft note (two players: a 60-second round each). In ROCK DRIFT a radar sweep goes
  round the field once a bar, a sixteenth a slice, and plays every rock it crosses: big ones low, small ones high,
  further from the middle higher still, all chord tones; as the rocks drift, the tune changes. Your shots play the
  chord tone of the way you aim, a break lands on the next sixteenth, quick breaks build a combo, and thrust pulses
  the bass on the beat. (DOT MUNCHER was cut, 16 Nov 2026: a maze of dots with chasers is too close to Pac-Man,
  cf. Atari v. North American Philips, 1982.) In BRICK BREAK a playhead walks the wall's ten columns,
  an eighth note each, playing each column's lowest brick (its row the note). In SWARM (a space shooter of its own: no rows marching, no formations diving) a swarm of droid ships holds
  the upper sky, never in step, each kind flying and firing its own way, its shots on the beat: scouts weave and fire
  straight down, gunships lumber and fire a spread, interceptors sweep and fire at you, orbiters circle and drop
  wandering mines, sentinels step on the beat and fire bursts. You fly a rocket in the lower sky (left, right, a
  little forward and back). Every droid you hit is caught in the jar, in a random one of eight slots across the top that loop as a bar
  of eighths, each playing its note, so the tune is what you shot. Three hits on you and it's the end. Clear the wall, or thin the swarm, and the drums come
  in. Their knobs, as SOLITAIRE's (each game keeps its own): K1 TEMPO, K2 CHANCE, K3 SOUND, K4 EFFECTS, PRESET the
  scale, ALGORITHM the key.

- **PINBALL** (one player): pinball down in a dungeon, two screens tall. Three shots up from the
  big flippers, named on the floor: STAIRS (the left lane, into the upper chamber), the PORTAL (the ball flies up,
  seen, to the door at the top) and the ORBIT (the right lane, round through the plunger's lane, lighting a top
  light). Shot after shot within five seconds is a combo, each worth more. A line under the header says what's
  next: the shots, the jars (MULTIBALL), the chest (JACKPOT), the top lights and the targets (the multiplier).
- **GOLF** (one player): three courses of nine, chosen from a quick list when it opens: THE GARDEN, THE CANYON
  (a desert at dusk) and THE CASTLE (its halls by torchlight). The greens curve, rounded at the corners and bowed along the sides; the
  walls inside are arcs; sand and water lie in soft blobs. No hole has a straight line from the tee to the cup,
  and it's the course that sees to it, not a bumper dropped in front of the tee: the green turns a corner, a wall or a
  ledge stands across it, water lies between, or the mill or the sliding wall is in the way (Nick, 18 Nov 2026).
  Only the bumper fields (BUMPER ALLEY, TUMBLEWEEDS) put their bumpers across the line, all over the green. The windmill has two blades; the sliding wall is four
  blocks wide.
- **POOL** (one player): nine balls racked in a diamond. Left and right aim (held: faster), up and down finely; hold the top key, the
  power pulses, let go to shoot. A dotted line and a ghost ball show what the cue ball meets first. No fouls: clear
  the table in as few shots as you can. Every knock is a note on the next sixteenth (ball on ball a chord tone, a
  cushion a wood knock). The six pockets are six notes of the chord, ringed in their colours, low at your end and
  high at the far one: each ball sunk drops into a random slot of the RACK (emptied when the table's cleared), eight slots under the header that loop as a bar of
  eighths, so the tune is where you sank them. The emptier the table, the more of the drums; two or more in one shot
  is a combo; the cue ball down is a scratch (back on its spot). The table cleared: the next rack, a little quicker,
  the loop playing on.

The music toys (one player): games you play as much as hear, from Nick's CYD MIDI Controller. Every
collision is a note, always in tune, and lands on the next sixteenth of the groove, so the physics is free and the
music always in time. The more you light, the more of the drums come in. Knobs: NOTE DROP: K1..K4 tilt its
platforms, SELECT the board; DODGE: K1 TEMPO, K2 SPEED, K3 SOUND, K4 EFFECTS. Both: PRESET the scale, ALGORITHM the key.

- **NOTE DROP**: every key drops a ball from above where it sits on the keyboard (all the keys light). It tumbles
  down the board's platforms, each a note (the higher, the higher), onto the floor's eight bars, each a note too. A
  ball landing on a bar lights it: light all eight to clear the board, so aim with the keys. Three hits light a
  platform full, and lit platforms bring in the drums. Four hits on the way down is a chain. Twelve boards progress from two platforms to eight, with
  increasingly dense and varied arrangements; the header shows your level. After board twelve the cycle repeats. Each
  platform has a knob (K1 .. K4, marked under it; on a board of more than four, the fifth shares K1, and so on):
  turn it and the platform tilts, up to 60 degrees either way, to send the balls where you want them.
- **DODGE** (was BOUNCE BOX): you're a rolling ball (it keeps rolling, slows by itself, and takes a moment to
  turn), dodge-ball in a box whose walls are 24 note tiles; every bounce off
  a wall is a note (one an eighth). The arrows move you. Every four bars you last, another ball comes in on the bar
  line (up to six), so the music builds the longer you survive, a drum part more for each ball. A hit: back to one
  ball, and the music drops. A near miss is an accent and points; stars are points and light some tiles. Light all
  24: a full circle. When the third ball arrives, a cyan exit ring opens in the opposite corner. Roll into it to
  earn 500 points and advance with your score intact. Each level starts with one ball; levels two onward add
  solid musical bumpers that deflect both you and the enemy balls. The first five levels build from an empty
  arena to five bumpers, then alternate the two densest layouts. A hit keeps your level and any earned exit open.

Two players: the board games take turns on one board; the score games are a round each (player 1, then player 2,
then the scores side by side).

## Build

```
./build.sh            # app/build/*.fwsc, the firmware (Docker + JieLi toolchain, as Mighty Arp)
./build.sh --host     # the simulator: app/build/host/*_host app/tests/basic.txt app/build/host
./build.sh --web      # the browser emulator: app/build/emu/ (serve it over http)
```


## Licence

MPL-2.0, all of it: see `LICENSE` and `LICENSING.md`. The games' art is their own, drawn in code.
