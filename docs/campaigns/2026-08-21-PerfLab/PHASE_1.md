# Phase 1 — Timing surface in CkProfile (CkFoundation)

> **Status:** ⏳ Pending
> **Depends on:** Phase 0 ✅ (stat-unit expressions extracted)
> **Estimate:** 1 session
> **Change class:** 2 (additive API in an existing CkFoundation module)

## Goal

After this phase: any C++/BP caller can read the same four numbers `stat unit` shows —
Frame/GameThread/RenderThread ms and GPU ms-or-unavailability — through one new struct on
`UCk_Utils_Stats_UE`, spec-tested, in all build environments.

## Entry criteria

- [ ] Load `Plugins/CkFoundation/CLAUDE.md` + `Source/CLAUDE.md` (style doctrine) and
      `ck-macros-and-codegen`.
- [ ] Read `Source/CkProfile/Public/CkProfile/Stats/CkStats_Utils.h` in full + `CkProfile/Claude.md`.
- [ ] Phase 0 addendum's stat-unit expressions at hand — they are the implementation.
- [ ] Branch `perflab/phase-1` in CkFoundation (from `perflab/phase-0` or dev — record base SHA).
- [ ] Baseline: `--test --test-pattern Ck.Profile` counts recorded (plus note the full-suite baseline
      from Phase 0 still stands).

## Work items

### 1.1 Types (`CkProfile/Stats/CkStats_Utils.h` — mimic the file's existing shapes)

Pre-designed surface (executor fills bodies, not designs; adjust ONLY if Phase 0 corrected the
underlying expressions, and date-note any change here):

```cpp
UENUM(BlueprintType)
enum class ECk_Stats_MetricAvailability : uint8
{
    Available,
    Unavailable_NullRhi,
    Unavailable_NoGpuTimestamps,
    Unavailable_NotYetSampled
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Stats_MetricAvailability);

USTRUCT(BlueprintType)
struct CKPROFILE_API FCk_Stats_ThreadTimings
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Stats_ThreadTimings);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
    float _FrameTimeMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
    float _GameThreadTimeMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
    float _RenderThreadTimeMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
    float _RhiThreadTimeMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
    float _GpuTimeMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
    ECk_Stats_MetricAvailability _GpuAvailability = ECk_Stats_MetricAvailability::Unavailable_NotYetSampled;

public:
    CK_PROPERTY_GET(_FrameTimeMs);
    CK_PROPERTY_GET(_GameThreadTimeMs);
    CK_PROPERTY_GET(_RenderThreadTimeMs);
    CK_PROPERTY_GET(_RhiThreadTimeMs);
    CK_PROPERTY_GET(_GpuTimeMs);
    CK_PROPERTY_GET(_GpuAvailability);

    CK_DEFINE_CONSTRUCTORS(FCk_Stats_ThreadTimings, _FrameTimeMs, _GameThreadTimeMs,
        _RenderThreadTimeMs, _RhiThreadTimeMs, _GpuTimeMs, _GpuAvailability);
};
```

(Exact `UPROPERTY` specifier set: copy whatever the newest struct in `CkStats_Utils.h`/sibling files
uses — the file you are editing wins over this sketch. `_GpuTimeMs` is meaningful ONLY when
`_GpuAvailability == Available`; document that on the getter line per house comment rules — a
contract line, not a what-comment.)

### 1.2 Reader (`UCk_Utils_Stats_UE`)

- `static FCk_Stats_ThreadTimings Get_ThreadTimings();` — UFUNCTION(BlueprintPure, same category
  string as siblings). Implementation: the Phase-0-extracted expressions —
  `FPlatformTime::ToMilliseconds(GGameThreadTime)` etc., GPU via `RHIGetGPUFrameCycles()` with
  0-cycles → `Unavailable_NoGpuTimestamps` and nullrhi detection → `Unavailable_NullRhi`
  (Phase 0 addendum names the check). **Fence: never reference `GGPUFrameTime` (deprecated 5.6).**
- Build.cs: `CkProfile.Build.cs` currently declares only `Core`, `CoreUObject`, `Engine`, `CkCore`,
  `CkLog` (verified Phase 0). **Both `RenderCore` (the `G*Time` globals) and `RHI`
  (`RHIGetGPUFrameCycles`) must be added**, each with a justification comment in the house style
  shown by `CkOptimizationDebugger.Build.cs`.
- **Fence — do NOT add `CkEcs` here.** The "every CK module must link CkEcs" comment lives in a
  *debugger-tier* Build.cs; `CkProfile` is tier T1 and `CkEcs` is T2, and the tier rule forbids a
  dependency pointing to a higher band. CkProfile links neither today, and must not start.

### 1.3 Specs

`Source/CkTests/Private/UnitTests/CkProfile/` (or CkProfile's own spec location if one exists —
mimic where `CkAutoTest_Profile_ScopedStat.as`'s C++ siblings live; check first):

- Availability enum formatter round-trips (house formatter spec pattern).
- `Get_ThreadTimings()` under the test environment returns: all four thread values ≥ 0; GPU
  availability is a legal enum value; and **when availability != Available, callers can detect it**
  (this pins the never-0-as-data contract at the source).
- Struct is constructible via `CK_DEFINE_CONSTRUCTORS` with designated values (compile-time shape pin).

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| `--build --target=Editor` | Clean compile incl. UHT | UHT rejects trailing return on the UFUNCTION | Concrete return type on its own line (rule #2) — already specified above |
| `--test --test-pattern Ck.Profile` | Baseline + new specs, all green | New spec red on GPU availability under `-nullrhi` harness | That IS the expected env — assert on the enum being `Unavailable_*`, not on a value; fix the spec, not the reader |
| Grep `GGPUFrameTime` in the diff | 0 hits | any | Replace with `RHIGetGPUFrameCycles()` |

## Exit criteria — same commit

- [ ] Editor target compiles; `Ck.Profile` gate: baseline → baseline+new, names recorded.
- [ ] `CkProfile/Claude.md` gains a §Thread timings block (the availability contract + the
      "mirrors stat unit by construction" claim with the engine file:line it mirrors).
- [ ] BP surface visible: `[EDITOR-VERIFY]` row added to VALIDATION.md ("place `[Ck] Get Thread
      Timings` node, confirm five floats + availability pin").
- [ ] AS: generated `utils_stats::Get_ThreadTimings()` appears at next editor boot —
      `[EDITOR-VERIFY]` row (agents cannot boot the editor; do not claim it).
- [ ] PLAN.md row + Status header + PROGRESS.md entry (baseline→after counts).

## Fences

- Do not add sampling/aggregation here — Phase 2 owns statistics. This phase is the raw read only.
- Do not touch `CkMemory`'s tracking path (vestigial — RESEARCH_Codebase.md).
- No new stat groups (`CkProfile/Claude.md` anti-pattern #1).
