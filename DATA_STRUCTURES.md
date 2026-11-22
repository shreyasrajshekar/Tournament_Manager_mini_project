# Tournament Manager: Major Data Structures

This report covers the five core data structures built from scratch in C. They live in `src/ds/` and are used by the tournament engine in `src/core/`.

## At a glance

| # | Structure | File | Main job in the app | Key cost |
|---|---|---|---|---|
| 1 | Hash map | `src/ds/hashmap.c` | Find clubs and players by name, reject duplicates | O(1) average |
| 2 | Linked list | `src/ds/linkedlist.c` | Keep clubs, squads, history and fans in order | O(1) add and remove |
| 3 | Queue | `src/ds/queue.c` | Build the bracket level by level | O(1) |
| 4 | Stack | `src/ds/stack.c` | Undo the last match result | O(1) |
| 5 | Heap | `src/ds/heap.c` | Seeding, "up next", standings, leaderboards | O(log n) |

---

## 1. Hash map

**What it is:** A table of buckets. Each key (a club name) is turned into a number by the FNV-1a hash function, and that number picks a bucket. Collisions are handled by chaining, so each bucket holds a short list.

**Role:** Fast lookup and duplicate checks. The table grows when it is more than 75% full.

**Where it is used:**

| Use | Key | Why it matters |
|---|---|---|
| Club registry | lowercase club name | "Arsenal" and "arsenal" count as the same club |
| Player lookup | player name | rejects duplicate names inside a club |
| Fan lookup | fan id or name | quick access to a fan's points |
| Prediction check | fan + match | one prediction per fan per match |

**Typical call:** `hashmap_put(&t->teamIndex, key, team)` on registration, and `hashmap_get` on every lookup by name.

---

## 2. Linked list

**What it is:** A doubly linked list. Each node points to the previous and the next node, so you can add or remove a node in the middle without shifting anything.

**Role:** Keeps items in insertion order and lets the engine remove an item in O(1) once it has the node.

**Where it is used:**

| Use | What is stored | Why a list |
|---|---|---|
| Club registry | every club | keeps registration order |
| Squad | players in each club | add and remove players cheaply |
| Match history | results per club, newest first | `push_front` is O(1) |
| Fans | every fan | simple iteration |
| Predictions | every prediction | simple iteration and removal |

**Typical call:** `list_push_back` when adding a club, `list_remove` when removing one.

---

## 3. Queue

**What it is:** A first-in, first-out line. Items enter at the back and leave at the front. This version is circular and grows when full.

**Role:** Processes items in the order they arrive. The engine uses it to build the bracket one level at a time, from the first round to the final.

**Where it is used:**

| Use | What happens |
|---|---|
| Bracket build | Each match is queued, then created in breadth-first order |
| Level-order export | Walks the bracket level by level |

**Typical call:** `queue_push` to add a match, `queue_pop` to take the next one.

---

## 4. Stack

**What it is:** A last-in, first-out pile. You can only add or take from the top.

**Role:** Holds every played result, so the last one can be undone.

**Where it is used:**

| Use | What happens |
|---|---|
| Undo | Every result is pushed. Undo pops the last one, then rebuilds the bracket from the seeds and replays the rest |

This is event sourcing. Ratings, history and fan points are always recalculated from the stack, so they stay consistent after an undo.

**Typical call:** `stack_push` after each result, `stack_pop` on undo.

---

## 5. Heap (priority queue)

**What it is:** A binary tree stored in an array. The item with the highest priority always sits at the top. You supply a comparison function that decides the order.

**Role:** Gets the "best" item quickly without sorting the whole list.

**Where it is used:**

| Use | Priority rule | Heap type |
|---|---|---|
| Seeding | highest starting rating first | max-heap |
| Up next | earliest round, then earliest position | min-heap |
| Club strength table | highest live Elo rating first | max-heap |
| Cup standings | furthest round, then goal difference, wins, then rating | max-heap |
| Fan leaderboard | most points first | max-heap |

**Typical call:** `heap_push` to add an item, `heap_pop` to take the top one.

---

## How the five work together

1. **Hash map** finds a club by name when you register it or add a player.
2. **Linked list** stores the club in the registry, in order.
3. **Heap** sorts the clubs by rating and gives each one a seed.
4. **Queue** builds the bracket tree, one level at a time.
5. **Stack** records each result, so any result can be undone later.

The bracket itself is a binary tree made of nodes with `left`, `right` and `parent` pointers. It is not a separate container in `src/ds/`. The queue builds it, and the parent pointers move each winner up to the next match.

## Notes

- Each structure is written in plain C with no external libraries.
- Each has a test in `tests/test_ds.c`. All tests passed.
- Some comments in the headers still mention court scheduling and an event log. Court scheduling was removed from the app, so those comments are out of date.
