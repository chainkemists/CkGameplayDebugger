# Runtime graph parity — gate index

| Gate | State | Observable result |
|---|---|---|
| 0 — Contract and shared canvas | Implemented, pending BM | A runtime graph surface renders stable child cards and directed edges with the required viewport interactions in both targets. |
| 1 — ECS and Scheduler | Implemented, pending BM | Their graph panes share the runtime renderer in Editor and packaged builds. |
| 2 — GOAP | Implemented, pending BM | The plan graph, cards, edge states, layout, and selection synchronization share one implementation. |
| 3 — State Machine graph core | Implemented, pending BM | State, transition, entry, compound, task, and wire presentation share one implementation. |
| 4 — State Machine surrounding behavior | Implemented, pending BM | Details, history/scrub/timeline, breakpoints, keyboard commands, and graph coupling are available in both targets; editor-only preview authoring remains excluded. |
| 5 — Package acceptance and cleanup | Pending | Editor and BM packaged builds pass the same side-by-side parity checklist; legacy GraphEditor code is removed or explicitly tombstoned. |

Gate contracts live in [Plan](Plan). Update this table and the active gate status in the same commit that lands a gate.
