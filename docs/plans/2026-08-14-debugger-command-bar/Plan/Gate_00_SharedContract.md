# Gate 00 - Shared command-bar contract

## Exit criteria

1. `CkDebuggerCommon` exposes an ordered semantic command-group descriptor with stable ID, accessible label, content widget, and explicit lane/placement metadata only where required.
2. Invalid descriptors and duplicate IDs reject atomically with no partial render plan.
3. `SCkDebug_CommandBar` renders group separators only between non-empty valid groups, keeps both lanes on one horizontally scrollable line each, and owns the single flexible alignment boundary.
4. `SCkDebug_WindowChrome` can render the new command bar while preserving an explicit compatibility path during migration.
5. Focused automation covers ordering, rejection, separator count, and left/trailing partition behavior.

## Verification

- Editor build through UnrealToolbox.
- Focused `CommandBar` automation.
- `git diff --check` within `Plugins/CkGameplayDebugger`.

## Risks

- Horizontal overflow affordance and density are ultimately visual and remain `[EDITOR-VERIFY]`.
- Common must never capture feature `this`; feature-built widgets retain their existing delegates and lifetime guards.
