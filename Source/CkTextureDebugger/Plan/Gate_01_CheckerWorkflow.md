# Gate 1 — Checker artist workflow

> **Status:** ⏳ Pending
> **Depends on:** Gate 0 ✅

## Goal

After this gate, an artist can open the tool, choose any supplied checker, target selected/focused/nearby/loaded-world mesh components, pick a component, choose slots, apply the checker directly in the active world, and restore originals without dirtying a level.

## Work items

1. Build the shared-chrome window and underline page tabs with Checker as the landing page.
2. Build checker cards/thumbnails from real assets; persist per-user checker choice and display preferences.
3. Implement explicit scope models: Editor selection, runtime focused picker target, nearby radius, loaded world.
4. Wire component picker, component/slot inspector, foliage affected-instance confirmation, apply, restore, and status.
5. Add tests for scope collection, user-state persistence, UI construction, level-package dirtiness, save-mid-session restore/reapply, and lifecycle restoration.

## Expected observations

| Run | Expected | If instead | Response |
|---|---|---|---|
| Apply/restore in clean Editor level | Visible checker appears; package dirty state stays unchanged; original material identity returns | Level dirties or material differs | Stop; do not clear dirty flags to hide the defect. |
| Make unrelated level edit and Save All while checker is active | Pre-save removes checker; disk contains original/external materials; post-save reapply is visible without dirtying the saved package | Checker serializes or save callback misses | Stop; direct editor-world workflow is unsafe. |
| PIE picker on no-collision mesh | Bounds fallback selects component; slot remains explicit | No target or guessed slot | Fix picker; never silently pick slot 0. |
| Foliage component selection | UI states N affected instances and requires confirmation | Appears per-instance | Block misleading workflow. |

## Exit criteria

- [ ] Focused tests green versus Gate 0.
- [ ] `[EDITOR-VERIFY]` and `[PIE-VERIFY]` steps recorded with results.
- [ ] Gate docs and permanent authoring docs updated.
