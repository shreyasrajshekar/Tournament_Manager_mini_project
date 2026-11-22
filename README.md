# Tournament Manager

A football cup manager written in **C** on top of hand-built data structures. A small embedded HTTP server exposes the engine as a JSON API, and a framework-free browser UI shows it, including a live **C engine trace** console that prints every data-structure operation behind each click.

> Mini project for Data Structures: every feature is driven by a data structure implemented from scratch (no STL, no external containers).

## Features

- Register clubs and their squads (GK / DEF / MID / FWD, shirt numbers); duplicates are rejected.
- Knockout cup bracket for **2 to 64 clubs**, with byes for the top seeds.
- Play a match with a score, or simulate one match, one round, or the whole cup (strength-weighted).
- Winners advance automatically; club strength (Elo-style) updates after every result.
- **Undo** any result, with ratings, history and fan points rolled back consistently.
- Cup standings, club strength table, per-club match history and an activity feed.
- Fan **predictions** with virtual points only (no real money); underdogs pay more.
- Persistence in **SQLite** (`data/tournament.db`) and an exportable replay log.
- Downloadable **HTML tournament report** (`/api/report`).
- Live visualization of hash buckets, the heap and the undo stack in the UI.

## Data structures

| Structure | Used for | Cost | Files |
|---|---|---|---|
| Hash map (chaining, FNV-1a, rehash at 0.75 load) | Club, player and fan lookup; one prediction per fan per match | O(1) avg | `src/ds/hashmap.c` |
| Doubly linked list | Club registry, squads, match history, fans, predictions, activity feed | O(1) add / unlink | `src/ds/linkedlist.c` |
| Binary tree | The bracket: each match has `left`, `right`, `parent`; winners advance via `parent` | O(1) advance | `src/core/bracket.c` |
| Queue (circular, growable) | Building the tree level by level; level-order export | O(1) | `src/ds/queue.c` |
| Binary heap | Seeding, "up next" order, standings, strength table, fan leaderboard | O(log n) | `src/ds/heap.c` |
| Stack | Undo of match results | O(1) | `src/ds/stack.c` |

Undo is **event sourcing**: the stack holds every result; undo pops the last one, rebuilds the bracket from the stored seeds and replays the rest. Fan points are recomputed from predictions and results, so they are always consistent. See [DATA_STRUCTURES.md](DATA_STRUCTURES.md) for a deeper write-up.

## Getting started

### Requirements
- GCC (MinGW-w64 on Windows via MSYS2 / winlibs, or any GCC/Clang on Linux/macOS)
- SQLite3 development library (`libsqlite3`)
- A web browser. No Node, Python or npm.

### Build and run

**Windows:** double-click `run.bat` (builds with `build.bat` if needed, then opens the browser).

**Linux / macOS:**
```bash
make
./tm_server --open        # or browse to http://localhost:8080
```

Run from the project folder: the server serves `./web`.

### Options
```
tm_server [--port 8080] [--web web] [--save data/tournament.db] [--open] [--seed N]
```

### Tests and CLI demo
```bash
make test          # unit tests for the data structures and the engine
./tm_cli 8 3       # terminal demo of the whole flow, no browser needed
```

## Demo walkthrough (about 3 minutes)

1. **Create the cup**: click *Quick demo* (sample clubs, squads, fans) or *Create your own*.
2. **Squads**: open a club, add players or fill a sample squad.
3. **Generate bracket**: seeds come from a max-heap; open the trace console to see heap pushes and the queue-built tree.
4. **Predict**: add a fan and stake points on a ready match.
5. **Play**: enter a score or *Simulate*; the winner climbs the tree through the parent pointer.
6. **Explore tabs**: Up next, Clubs & squads, Tables, History, Data structures.
7. **Undo** the last result, then **Sim all** to crown a winner and download the report.

## Project structure

```
web/                 Browser UI (HTML / CSS / JS)
src/main.c           Server entry point
src/cli.c            Terminal demo
src/server/          http.c (sockets), json.c (parser), api.c (routes), report.c (HTML report)
src/core/            Domain logic
  types.h              shared structs
  tournament.c         facade: create, add club, generate, play, sim, undo
  bracket.c            seeding, tree build, advancement      (tree + queue)
  upnext.c             next ready match                      (heap)
  team.c / player.c    clubs, squads, strength maths         (hash map + list)
  history.c            activity feed, result records
  ranking.c            standings and strength table          (heap)
  predict.c            fans, predictions, virtual points
  persist.c / db.c     replay log and SQLite storage
src/ds/              hashmap, linkedlist, queue, stack, heap
src/util/            memory helpers, string buffer, RNG, trace log
tests/               test_ds.c, test_core.c
```

## JSON API

| Endpoint | Body | Effect |
|---|---|---|
| `GET /api/state` | | Full state: clubs, bracket, tables, fans, history, DS stats, trace |
| `GET /api/report` | | Download the HTML tournament report |
| `GET /api/export` | | Download the replay log |
| `POST /api/create` | `name` | New cup in setup phase |
| `POST /api/team`, `/api/team/remove` | `name, rating` / `id` | Add / remove a club |
| `POST /api/fill` | `count` | Add sample clubs |
| `POST /api/player`, `/api/player/remove` | `teamId, name, pos, number` / `id` | Add / remove a player |
| `POST /api/squad/fill` | `teamId?` | Sample squad(s) |
| `POST /api/generate` | | Seed, build tree, resolve byes |
| `POST /api/play` | `matchId, scoreA, scoreB` | Record a result |
| `POST /api/sim`, `/simround`, `/simall` | `matchId?` | Simulate |
| `POST /api/undo` | | Undo last result |
| `POST /api/fan`, `/api/fan/fill` | `name` / none | Add fans |
| `POST /api/predict` | `fanId, matchId, pickId, stake` | Stake virtual points |
| `POST /api/demo`, `/api/reset` | `teams` | Quick demo / discard |

## Limits

Single elimination, 2 to 64 clubs, up to 25 players per club, up to 64 fans, one active cup, no draws in knockout matches. The server is single-threaded and binds to localhost only.

## Team

| Member | Area | Structures |
|---|---|---|
| Shreyas | Cup bracket, tournament flow | Tree, Queue |
| Jatin | Clubs and players | Hash map, Linked list |
| Shaun | Tables, undo, predictions, UI | Heap, Stack |

## Future ideas

Double elimination, round-robin groups with a heap-based table, third-place playoff.
