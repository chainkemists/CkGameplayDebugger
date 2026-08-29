# Gate 2 — packaged and performance acceptance

> **Status:** Pending
> **Depends on:** Gate 1 complete

## Goal

After this gate, the visualizer has measured same-machine evidence for disabled, activation, steady-visible, and nav-refresh costs, plus live packaged Development and Test acceptance.

## Entry criteria

- [ ] Gate 1 observations reverified on current HEAD.
- [ ] Baseline stock/legacy draw scenario and map recorded.
- [ ] Build machine resolves the project engine association.

## Work items

1. Instrument tile extraction, bucket publication, proxy replacement, active bucket count, and active ticker state.
2. Capture at least three same-machine packaged runs for disabled baseline and enabled visualizer.
3. Validate Development and Test packages; verify the command is unavailable/inert in Shipping without starting a local Shipping build.
4. Perform visual acceptance over static and dynamically regenerated navmesh.

## Expected observations

| I will run | I expect to observe | If instead I see | Response |
|---|---|---|---|
| Disabled packaged trace | No tile-processing scopes, components, or draw calls from this module. | Any recurring work | Block delivery and remove the owner. |
| Static enabled trace | Capture ends; steady CPU geometry work becomes zero until a nav event. | Polling/rebuild scopes continue | Block delivery and trace the invalidation source. |
| Dynamic rebuild | Only changed-signature buckets replace proxies. | Whole-city replacement | Revisit bucket signature/publish boundaries. |

## Exit criteria

- [ ] N >= 3 A/B results reported with config, machine, avg/max, and spread.
- [ ] Packaged Development and Test visual acceptance complete.
- [ ] Shipping boundary verified without an unauthorized Shipping build.
- [ ] Campaign docs deleted or tombstoned after permanent doctrine is complete.
