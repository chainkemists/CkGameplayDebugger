# STATUS.md — ArgusDebugServer Implementation Progress

> **Last Updated:** 2026-02-06
> **Current Phase:** Phase 1 — Foundation (compile & validate handshake)
> **Parent Spec:** `D:\Repositories\Argus\dev\phase1-foundation\argus-debug-server-spec.md`
> **Partner Project:** `D:\Repositories\Argus` (Argus debugger client)

---

## Quick Status

| Phase | Status | Progress |
|-------|--------|----------|
| Phase 1: Foundation & Handshake | 🟡 In Progress | 2/4 sessions |
| Phase 2: Command Dispatch | ⚪ Not Started | 0/? sessions |

**Legend:** ⚪ Not Started | 🟡 In Progress | 🟢 Complete | 🔴 Blocked

---

## Architecture Summary

**ArgusDebugServer** is a UE module inside the `CkGameplayDebugger` plugin. It serves as the UE-side endpoint for the Argus external ECS debugger.

```
┌──────────────────────────────────────────────────────────┐
│ CkFoundation (CkEcs module)                              │
│                                                          │
│  FCk_FragmentReflectionRegistry (singleton)              │
│    └── TMap<entt::id_type, FCk_FragmentTypeInfo>         │
│                                                          │
│  FCk_Registry                                            │
│    └── Get_InternalRegistryRawPtr() → entt::registry*    │
│                                                          │
│  CK_REGISTER_ECS_FRAGMENT[_REFLECTED] macros             │
└────────────────────────┬─────────────────────────────────┘
                         │ reads from
                         ▼
┌──────────────────────────────────────────────────────────┐
│ ArgusDebugServer (CkGameplayDebugger plugin)             │
│                                                          │
│  FDebugTcpServer  → TCP on localhost:47522               │
│  FHandshakeBuilder → world enumeration + UE reflection   │
│  FMsgPackWriter/Reader → MessagePack codec               │
│                                                          │
│  Handshake sends:                                        │
│    • process_id (for ReadProcessMemory)                  │
│    • registry_address[] (raw entt::registry* pointers)   │
│    • component_types[] (UE UPROPERTY reflection data)    │
└────────────────────────┬─────────────────────────────────┘
                         │ TCP (metadata only)
                         ▼
┌──────────────────────────────────────────────────────────┐
│ Argus (external debugger app)                            │
│                                                          │
│  ReadProcessMemory for entity/component data             │
│  No entity data flows over TCP — direct memory access    │
└──────────────────────────────────────────────────────────┘
```

**Key Design Decisions:**
- UE native reflection (UHT/UPROPERTY) — no RTTR
- `FCk_FragmentReflectionRegistry` lives in CkFoundation (correct dependency direction)
- ArgusDebugServer is a read-only consumer of the registry
- TCP carries metadata + control commands only; bulk data via `ReadProcessMemory`
- Single client connection, localhost only

---

## Repositories & Branches

| Repo | Branch | Role |
|------|--------|------|
| CkFoundation (worktree) | `feature/fragment-reflection-registry` | Owns reflection registry + macros |
| CkGameplayDebugger | `feature/argus-debugger` | Hosts ArgusDebugServer module |
| Argus | main | External debugger (C++20, SDL3, ImGui) |

**Sync Point:** CkFoundation branch must be merged (or rebased onto) before ArgusDebugServer can compile, since it depends on the new `CkEcs/Reflection/` files.

---

## Phase 1: Foundation & Handshake

**Goal:** Build, compile, and validate the ArgusDebugServer module end-to-end with the Argus client.

| Session | Task | Status | Notes |
|---------|------|--------|-------|
| DS-1.1 | Write ArgusDebugServer module (TCP server, protocol, handshake) | 🟢 | 12 files, complete implementation |
| DS-1.2 | Fragment reflection registry + dependency refactor | 🟢 | Registry moved to CkFoundation, macros created, old Argus-owned registry deleted |
| DS-1.3 | Compile module in UE 5.5 + fix errors | ⚪ | **NEXT SESSION** — 7 known risk areas documented |
| DS-1.4 | **[SYNC POINT]** End-to-end handshake with Argus client | ⚪ | Validate connection, registry data, reflection metadata |

### Session DS-1.1 — Write ArgusDebugServer Module
**Date:** 2026-02-05 | **Status:** 🟢 Complete

**Output:**
- 12 new source files under `Source/ArgusDebugServer/`
- Complete TCP server (`FDebugTcpServer`) on port 47522, single-client, background thread
- MessagePack codec (`FMsgPackWriter`/`FMsgPackReader`)
- Handshake builder (`FHandshakeBuilder`) using UE reflection (`TFieldIterator<FProperty>`)
- Protocol types matching Argus client wire format exactly
- `Argus::FFragmentTypeMap` with manual registration of 4 USTRUCTs
- Module lifecycle wiring (`StartupModule` → TCP listen)
- `.uplugin` entry (UncookedOnly, Win64)
- `Get_InternalRegistryRawPtr()` added to `FCk_Registry`

**Note:** Module written but never compiled. Compilation deferred to DS-1.3.

### Session DS-1.2 — Fragment Reflection Registry + Dependency Refactor
**Date:** 2026-02-06 | **Status:** 🟢 Complete

**Output:**
- **CkFoundation (4 new files):**
  - `CkEcs/Reflection/CkFragmentTypeInfo.h` — value type (hash, name, size, optional UScriptStruct*)
  - `CkEcs/Reflection/CkFragmentReflectionRegistry.h/cpp` — Meyers singleton, `Register<T>()` templates
  - `CkEcs/Reflection/CkFragmentReflection_Macros.h` — `CK_REGISTER_ECS_FRAGMENT[_REFLECTED]` macros
  - `CkEcs/Registry/CkRegistry.h` — added `Get_InternalRegistryRawPtr()` (was on another branch, cherry-picked)
- **ArgusDebugServer (refactored):**
  - Deleted `ArgusFragmentTypeMap.h/cpp` and `ArgusFragmentRegistration.cpp` (3 files)
  - Created `ArgusFragmentRegistration_Temp.cpp` — 4 USTRUCTs registered with new macro (temporary bridge)
  - Updated `ArgusHandshakeBuilder.cpp` to read from `ck::FCk_FragmentReflectionRegistry`
  - Updated `ArgusDebugServerModule.cpp` to remove manual registration call
- **Net change:** -100 lines from debugger plugin, +224 lines in CkFoundation

**Key Decision:** Registration macro uses anonymous-namespace auto-registrar pattern (`.cpp`-only, same as UE's `FAutoConsoleObject`). Two variants: `CK_REGISTER_ECS_FRAGMENT` (non-USTRUCT) and `CK_REGISTER_ECS_FRAGMENT_REFLECTED` (USTRUCT with `StaticStruct()`).

### Session DS-1.3 — Compile Module in UE 5.5 (NEXT)
**Date:** TBD | **Status:** ⚪ Not Started

**Goal:** Zero compiler/linker errors. Module loads in UE Editor.

**Known Risk Areas (ordered by probability):**
1. FTcpListener API differences in UE 5.5 (HIGH)
2. DECLARE_DELEGATE inside namespace (MEDIUM)
3. EnTT include paths (MEDIUM)
4. FPlatformProcess::GetSynchEventFromPool deprecation (MEDIUM)
5. Fragment header include paths (LOW)
6. StringCast usage (LOW)
7. Missing FIPv4Endpoint forward declaration (LOW)

**Acceptance Criteria:**
- [ ] Zero compiler errors (Win64 Development Editor)
- [ ] Zero linker errors
- [ ] CkFoundation compiles with new Reflection/ files
- [ ] Module loads — UE Editor starts without crash
- [ ] Output Log: `"ArgusDebugServer: Listening on port 47522"`
- [ ] Output Log: `"ArgusDebugServer: 4 fragment types available in reflection registry"`

### Session DS-1.4 — End-to-End Handshake Validation (SYNC POINT)
**Date:** TBD | **Status:** ⚪ Not Started

**Goal:** Argus client connects to live ArgusDebugServer and receives valid handshake.

**Acceptance Criteria:**
- [ ] Argus connects to localhost:47522
- [ ] HandshakeResponse received with `accepted=true`
- [ ] `process_id` is correct (matches UE Editor PID)
- [ ] `registries[]` contains at least 1 entry (PIE world)
- [ ] `registry_address` is non-zero and valid
- [ ] `component_types[]` contains 4 types with correct property metadata
- [ ] `entt_version` matches between client and server
- [ ] Argus can `OpenProcess` with `PROCESS_VM_READ` using the received PID
- [ ] Connection survives for >30 seconds without disconnect

---

## Phase 2: Command Dispatch (Future)

**Goal:** Implement post-handshake commands (DrawGizmo, entity queries, etc.)

Currently the server loops reading frames after handshake but logs and ignores unknown commands. Phase 2 adds command handlers.

| Session | Task | Status | Notes |
|---------|------|--------|-------|
| DS-2.1 | Command dispatch framework | ⚪ | Message type routing, handler registration |
| DS-2.2 | DrawGizmoRequest handler | ⚪ | Forward to UE debug draw |
| DS-2.3+ | TBD based on Argus Phase 3-4 needs | ⚪ | |

---

## Current File Inventory

### ArgusDebugServer Module (11 files)

```
Source/ArgusDebugServer/
├── ArgusDebugServer.Build.cs
├── Public/
│   ├── ArgusDebugServerModule.h
│   ├── ArgusDebugTcpServer.h
│   ├── ArgusHandshakeBuilder.h
│   ├── ArgusProtocolTypes.h
│   └── ArgusMsgPack.h
└── Private/
    ├── ArgusDebugServerModule.cpp
    ├── ArgusDebugTcpServer.cpp
    ├── ArgusHandshakeBuilder.cpp
    ├── ArgusMsgPack.cpp
    └── ArgusFragmentRegistration_Temp.cpp    ← temporary, delete when fragments self-register
```

### CkFoundation Additions (5 files, on feature/fragment-reflection-registry)

```
Source/CkEcs/Public/CkEcs/
├── Reflection/
│   ├── CkFragmentTypeInfo.h
│   ├── CkFragmentReflectionRegistry.h
│   ├── CkFragmentReflectionRegistry.cpp
│   └── CkFragmentReflection_Macros.h
└── Registry/
    └── CkRegistry.h                         ← Get_InternalRegistryRawPtr() added
```

---

## Decisions Log

| Date | Decision | Rationale | Session |
|------|----------|-----------|---------|
| 2026-02-05 | UE native reflection, no RTTR | USTRUCTs have UHT reflection; FFragmentTypeMap for EnTT→UScriptStruct* | DS-1.1 |
| 2026-02-05 | TCP metadata only, ReadProcessMemory for data | Maximum speed, minimal IPC overhead | DS-1.1 |
| 2026-02-05 | Custom MsgPack codec (no library) | Lightweight, no extra dependencies, only need subset of spec | DS-1.1 |
| 2026-02-05 | Single client, localhost only | Debugger runs on same machine; simplifies threading | DS-1.1 |
| 2026-02-06 | Move reflection registry to CkFoundation | CkFoundation cannot depend on CkGameplayDebugger; correct dependency direction | DS-1.2 |
| 2026-02-06 | Static auto-registration macros (.cpp only) | Same pattern as UE's FAutoConsoleObject; no centralized registration file | DS-1.2 |
| 2026-02-06 | Two-phase registration (non-USTRUCT → USTRUCT) | Fragments aren't USTRUCT yet; registry supports nullable ScriptStruct* | DS-1.2 |
| 2026-02-06 | Temporary registration bridge in ArgusDebugServer | 4 USTRUCTs registered via temp file; delete when fragments self-register | DS-1.2 |
