# CK AI Overview — gate index

> **Current:** Gate 4 automated pane-owner remediation is complete through the dedicated Common host; live painted/editor acceptance remains. Gate 3 remains a build-machine handoff. Update this table and each gate header together.

| Gate | Name | Status | Contract |
|---|---|---|---|
| 0 | Common chrome and control primitives | Complete | [Gate_00_CommonChrome.md](Plan/Gate_00_CommonChrome.md) |
| 1 | Authoritative runtime controls and BB policy | Complete; direct multi-PIE proof passed | [Gate_01_RuntimeControls.md](Plan/Gate_01_RuntimeControls.md) |
| 2 | Unified AI Overview debugger | Remediation implemented; live Slate visual acceptance pending | [Gate_02_AiOverview.md](Plan/Gate_02_AiOverview.md) |
| 3 | Packaged picker root cause and final acceptance | Instrumented; build-machine reproduction pending | [Gate_03_PackagedAcceptance.md](Plan/Gate_03_PackagedAcceptance.md) |
| 4 | Suite-wide Style Lab pane migration | Dedicated host automated complete; live editor acceptance pending | [Gate_04_SuiteStyleMigration.md](Plan/Gate_04_SuiteStyleMigration.md) |

## Permanent survivors

At campaign death, keep the implemented code/tests and update the relevant module `CLAUDE.md` files. Delete or tombstone this plan directory after the implementation has shipped.
