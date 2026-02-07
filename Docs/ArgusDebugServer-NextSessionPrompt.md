# ArgusDebugServer — Next Session Prompt (DS-1.3: Compile)

> **Copy-paste the text between the `---` markers below as the initial prompt for the next Claude session.**
> **Last Updated:** 2026-02-06

---

## Prompt

You are helping me compile the `ArgusDebugServer` UE module and fix all errors until it builds cleanly. All source files are already on disk. **No runtime testing this session** — just get it to compile.

### Context — Read First

1. **Status:** `Docs/STATUS.md` — full session history, architecture, decisions log
2. **Compile guide:** `Docs/ArgusDebugServer-CompileSession.md` — file inventory, known risk areas, architecture context
3. **Spec:** `D:\Repositories\Argus\dev\phase1-foundation\argus-debug-server-spec.md` — protocol reference

### What Happened in Prior Sessions

**DS-1.1** wrote the entire ArgusDebugServer module (12 files). **DS-1.2** refactored the fragment registration architecture:

- Moved the fragment reflection registry from this plugin into CkFoundation (`CkEcs/Reflection/`)
- Created `FCk_FragmentReflectionRegistry` (Meyers singleton) and `CK_REGISTER_ECS_FRAGMENT_REFLECTED` macro
- ArgusDebugServer now **reads from** the CkFoundation registry instead of owning one
- Deleted 3 old files (`ArgusFragmentTypeMap.h/cpp`, `ArgusFragmentRegistration.cpp`)
- Added temporary bridge `ArgusFragmentRegistration_Temp.cpp` (registers 4 USTRUCTs with new macro)
- Added `Get_InternalRegistryRawPtr()` to `FCk_Registry` in CkRegistry.h

**The module has NEVER been compiled.** This session is the first compile attempt.

### Project Layout

- **Venus project:** `D:\Repositories\Venus\Venus.uproject`
- **This plugin:** `D:\Repositories\Venus\Plugins\CkGameplayDebugger\` (branch: `feature/argus-debugger`)
- **CkFoundation:** `D:\Repositories\Venus\Plugins\CkFoundation\` (branch: `feature/upgrade-to-5_7`)
  - **Note:** CkFoundation changes from DS-1.2 are on `feature/fragment-reflection-registry` worktree at `C:\Users\Adam\.claude-worktrees\Source\eloquent-chaum\`. These need to be on the same branch CkFoundation is building from, or merged. **Verify the Reflection/ files and Get_InternalRegistryRawPtr() exist on the active CkFoundation branch before building.**
- **Argus client (reference):** `D:\Repositories\Argus\`

### Current File Inventory (ArgusDebugServer — 11 files)

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
    └── ArgusFragmentRegistration_Temp.cpp
```

### CkFoundation Dependencies (must be on build branch)

```
Source/CkEcs/Public/CkEcs/
├── Reflection/                               ← NEW (from DS-1.2)
│   ├── CkFragmentTypeInfo.h
│   ├── CkFragmentReflectionRegistry.h
│   ├── CkFragmentReflectionRegistry.cpp
│   └── CkFragmentReflection_Macros.h
└── Registry/
    └── CkRegistry.h                          ← MODIFIED (Get_InternalRegistryRawPtr added)
```

### Build Environment

- **UE 5.5** (GUID-based engine association)
- **C++20** — `CkModuleRules` base class enforces `CppStandard = CppStandardVersion.Cpp20`
- **PCH:** `UseExplicitOrSharedPCHs`
- **EnTT 3.16.0** — vendored at `CkFoundation/Source/CkThirdParty/Public/CkThirdParty/entt-3.16.0/`
- **EnTT include path** exposed via `CkThirdParty.Build.cs` → `PublicIncludePaths`
- **No other module in Venus uses `FTcpListener`** — this is a first

### Build Command

```
UnrealBuildTool.exe VenusEditor Win64 Development -Project="D:\Repositories\Venus\Venus.uproject"
```

Or trigger a build from the UE Editor (Build > Build Solution).

### Known Risk Areas (ordered by probability)

1. **FTcpListener API** (HIGH) — `ArgusDebugTcpServer.cpp` uses `FTcpListener` constructor + `Init()` + `OnConnectionAccepted()`. The API surface may differ in UE 5.5. If `Init()` doesn't exist, the listener may auto-start on construction. If the delegate binding fails, check that the signature matches `bool(FSocket*, const FIPv4Endpoint&)`.

2. **DECLARE_DELEGATE inside namespace** (MEDIUM) — `ArgusDebugTcpServer.h` declares `FOnBuildHandshakeResponse` via `DECLARE_DELEGATE_RetVal_OneParam` inside `namespace Argus`. UE delegate macros may fail inside namespaces. Fix: move the declaration outside the namespace, prefixing types with `Argus::`.

3. **EnTT include paths** (MEDIUM) — `CkFragmentReflectionRegistry.h` (CkFoundation) and `ArgusFragmentRegistration_Temp.cpp` use `<entt/core/type_info.hpp>`. CkThirdParty exposes includes via `PublicIncludePaths` so angle brackets should work. If not, try quoted includes.

4. **FPlatformProcess::GetSynchEventFromPool** (MEDIUM) — `ArgusDebugTcpServer.cpp`. May be deprecated in UE 5.5. Replace with `FPlatformProcess::CreateSynchEvent(true)` and change `ReturnSynchEventToPool(DoneEvent)` to `delete DoneEvent`.

5. **Static auto-registration timing** (MEDIUM) — `ArgusFragmentRegistration_Temp.cpp` uses the `CK_REGISTER_ECS_FRAGMENT_REFLECTED` macro which calls `StaticStruct()` during static init. UE's UObject system must be initialized before `StaticStruct()` is called. If this crashes, move registration into `StartupModule()` instead.

6. **Fragment header includes** (LOW) — `ArgusFragmentRegistration_Temp.cpp` includes `CkEcsExt/Transform/CkTransform_Fragment_Data.h` and `CkEcs/Net/CkNet_Fragment_Data.h`. Include paths may differ; check actual filesystem paths in CkFoundation.

7. **StringCast usage** (LOW) — `ArgusMsgPack.cpp` uses `StringCast<UTF8CHAR>(*Value)`. Standard UE API but verify compilation.

8. **FIPv4Endpoint in header** (LOW) — `ArgusDebugTcpServer.h` forward declares `class FTcpListener` but uses `FIPv4Endpoint` in `HandleConnectionAccepted` signature without including the header.

### Task

1. **Verify CkFoundation has the Reflection/ files** — check that `CkEcs/Reflection/CkFragmentReflectionRegistry.h` exists on the CkFoundation branch being built. If not, the DS-1.2 changes need to be merged or cherry-picked.
2. Trigger the UE build
3. Fix each compiler/linker error iteratively
4. Rebuild until zero errors
5. Verify the module loads by checking the Output Log

### Acceptance Criteria

- [ ] Zero compiler errors — `ArgusDebugServer` module compiles (Win64 Development Editor)
- [ ] Zero linker errors — all symbols resolve
- [ ] CkFoundation compiles with new `Reflection/` files + `Get_InternalRegistryRawPtr()`
- [ ] Module loads — UE Editor starts without crash
- [ ] Output Log: `"ArgusDebugServer: Listening on port 47522"`
- [ ] Output Log: `"ArgusDebugServer: X fragment types available in reflection registry"` (expect 4)

### Important Notes

- **Do NOT modify the protocol wire format** — it must match the Argus client exactly (see `D:\Repositories\Argus\src\protocol\messages.h`)
- **Do NOT change the MessagePack encoding** — field order and types must match Argus's `MSGPACK_DEFINE` declarations
- **Preserve the architecture** — TCP server, handshake builder, reflection registry, and MsgPack codec are separate concerns by design
- **CkFoundation changes should be minimal** — the Reflection/ files and `Get_InternalRegistryRawPtr()` are already on disk; avoid further modifications

---
