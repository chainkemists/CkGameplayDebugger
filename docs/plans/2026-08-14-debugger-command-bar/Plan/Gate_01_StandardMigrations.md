# Gate 01 - Standard debugger migrations

## Scope

Migrate Aggro, A*, Dialog, ECS, EQS, Input, Intent, Jolt, Map, Object Pooling, Save, Scheduler, Style Lab, and UI.

## Exit criteria

1. Every scoped window supplies explicit semantic command groups through the common contract.
2. Existing controls and callbacks are reused; only layout composition changes.
3. Existing refresh-enabled windows retain refresh in the common trailing utility position; refresh-free windows remain refresh-free.
4. EQS and Scheduler duplicate debugger labels and static Map/Jolt product labels are removed; Map's dynamic world context remains.
5. No feature-local flexible spacer or textual separator remains in the migrated top area.

## Verification

- Static 14-window migration census.
- Focused `CommandBar` test plus module compilation.
- Existing debugger automation included by the broad `Debugger` gate.

## Result

Complete on 2026-08-14. All 14 windows use direct groups; the fresh Editor build succeeded and the baseline-comparable `Debugger` gate passed 111/111 with zero failures, skips, or contamination.
