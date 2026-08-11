# Runtime graph parity — gate index

| Gate | State | Observable result |
|---|---|---|
| 0 — Contract and shared canvas | Build verified | A runtime graph surface renders stable child cards and directed edges with the required viewport interactions in both targets. |
| 1 — ECS and Scheduler | Build verified | Their graph panes share the runtime renderer in Editor and packaged builds. |
| 2 — GOAP | Build verified | The plan graph, cards, edge states, layout, controls, and selection synchronization share one implementation. |
| 3 — State Machine graph core | Build verified | State, transition, entry, compound, task, and wire presentation share one implementation. |
| 4 — State Machine surrounding behavior | Build verified | Details, history/scrub/timeline, breakpoints, runtime preview/test controls, keyboard commands, and graph coupling are available in both targets. |
| 5 — Package acceptance and cleanup | In progress | Editor and packaged Development builds compile; QA still needs to complete the matched side-by-side visual and interaction checklist. Legacy GraphEditor sources are retained as tombstoned editor-only reference code. |

Gate contracts live in [Plan](Plan). Update this table and the active gate status in the same commit that lands a gate.
