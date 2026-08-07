# Debugger QoL campaign - gate index

> **Written:** 2026-08-04. Update this table in the same change that closes each gate.

| Gate | Goal | Status |
|---|---|---|
| [00 - Entity truth and navigation](Plan/Gate_00_EntityTruthAndNavigation.md) | ECS regrouping, universal ECS selection adoption, and targeted debugger jumps | Implementation verified; editor acceptance pending |
| [01 - Overlay AI triage](Plan/Gate_01_OverlayAiTriage.md) | Bounded focus-card density plus actionable GOAP/navigation state | Implementation verified; editor acceptance pending |
| [02 - Common window chrome](Plan/Gate_02_CommonWindowChrome.md) | Shared top/content/status frame across all plugin-owned debugger windows | Implementation verified; editor acceptance pending |
| [03 - Insights Analyzer ownership](Plan/Gate_03_InsightsAnalyzerOwnership.md) | Move analyzer Slate ownership into CkGameplayDebugger while retaining the Foundation analysis core | Implementation verified; editor acceptance pending |
| [04 - Complete toggle coverage](Plan/Gate_04_CompleteToggleCoverage.md) | Put useful shared icon actions in every debugger menu bar and remove feature-local raw checkboxes | Implementation verified; renewed editor acceptance pending |

The implementation campaign is complete only when all five gates are automation-verified against their recorded baselines. Live Slate, PIE lifecycle, hierarchy-target, and control-presentation behavior retain a separate editor-acceptance boundary.
