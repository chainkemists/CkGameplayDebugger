# Gate 03 - Verification and closeout

## Exit criteria

1. Registry count, `SCkDebug_WindowChrome` count, and migrated command-bar count match at 18.
2. The dock tab is the sole debugger identity surface; no debugger title or identity icon remains in command bars.
3. No textual separator or feature-owned top-row fill spacer remains where the common bar owns grouping/alignment.
4. `git diff --check` passes and changed-file scope contains no unrelated edits.
5. A fresh UnrealToolbox Editor build and `Debugger` gate are compared with the 108/108 pre-change baseline.
6. Fresh logs are scanned for failures, contamination, ensures, script errors, compiler/linker failures, and errors naming touched modules.
7. An independent adversarial review reports no unresolved correctness or lifetime finding.
8. The editor acceptance matrix is handed off honestly; pending rows are not claimed complete.

## Result

- Static census: 18 Chrome windows, 18 direct command-group windows, 0 legacy chrome-slot consumers, 11 refresh opt-ins, 0 feature-local refresh widgets.
- Fresh no-wrap build and focused gate: `Saved/Logs/DebuggerCommandBar-NoWrap-Focused-04.log`, build succeeded; 40/40 `Ck.DebuggerCommon` tests passed, including narrow eight-icon arrangement and command-lane no-wrap regressions.
- Jolt ensure red/green gate: `Saved/Logs/JoltSlotAttributeEnsure-Repro.log` failed 0/1 on the exact `FSlateAttributeMetaData::RegisterContainAttributeImpl` ensure; after moving live padding from unsupported `SScrollBox` slots to each row's registered `SBox` attribute, `Saved/Logs/JoltSlotAttributeEnsure-Fixed.log` passed 1/1 with no ensure or other failure pattern.
- Post-rebase baseline-comparable gate: after the fetched `CkEcsDebugger.Build.cs` dependency change, project generation and the Editor build succeeded; baseline 108/108 -> final 114/114 (five command-bar/icon-toolbar contract tests plus one Jolt window-construction regression), failed 0, skipped 0, contaminated 0 in `Saved/Logs/DebuggerCommandBar-PostRebase.log`.
- Fresh-log scan: no ensures, script errors, reload contamination, compiler/linker errors, assertions, or fatal errors. The three `Saved/Search/FileInfo.db` startup errors are an inherited baseline class and are not a new delta.
- `git diff --check`: passed. Editor recheck remains pending after the live feedback correction.
