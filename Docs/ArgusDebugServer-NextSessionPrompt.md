# ArgusDebugServer — Next Session Prompt

> **Copy-paste the text between the `---` markers below as the initial prompt for the next Claude session.**
> **Prerequisite:** Open the session from the `musing-liskov` worktree of the `CkGameplayDebugger` repo.

---

## Prompt

You are helping me compile a new Unreal Engine module called `ArgusDebugServer` that was written in a prior session. All source files are already on disk. Your job is to **compile the module and fix all errors** until it builds cleanly. No runtime testing this session.

### Context

Read these documents first to understand the full context:

1. **Session plan:** `Docs/ArgusDebugServer-CompileSession.md` — contains file inventory, known risk areas, acceptance criteria, architecture context, and protocol reference
2. **Claude plan file:** `.claude/plans/dynamic-mapping-dewdrop.md` — higher-level plan with risk areas

### Project layout

- **Venus project:** `D:\Repositories\Venus\Venus.uproject`
- **Plugin location:** `D:\Repositories\Venus\Plugins\CkGameplayDebugger\` (this repo, `musing-liskov` worktree)
- **CkFoundation plugin:** `D:\Repositories\Venus\Plugins\CkFoundation\`
- **Argus client (reference):** `D:\Repositories\Argus\`

### What was built

A new `ArgusDebugServer` UE module inside the `CkGameplayDebugger` plugin. It implements a TCP debug server (localhost:47522) that the Argus external debugger connects to for ECS inspection. The module consists of:

- 12 new source files under `Source/ArgusDebugServer/`
- 1 modified file: `CkDebugger.uplugin` (added module entry)
- 1 modified file in CkFoundation: `CkRegistry.h` (added `Get_InternalRegistryRawPtr()` public method at line 226)

### Build environment

- **UE 5.5** (GUID-based engine association, not a version number in .uproject)
- **C++20** — `CkModuleRules` base class enforces `CppStandard = CppStandardVersion.Cpp20`
- **PCH:** `UseExplicitOrSharedPCHs`
- **EnTT 3.16.0** — vendored at `CkFoundation/Source/CkThirdParty/Public/CkThirdParty/entt-3.16.0/`
- **EnTT include path** exposed via `CkThirdParty.Build.cs` → `PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "entt-3.16.0", "src"))`
- **No other module in Venus uses `FTcpListener`** — this is a first. `CogImgui` uses `Sockets` module but only raw sockets.

### Build command

```
UnrealBuildTool.exe VenusEditor Win64 Development -Project="D:\Repositories\Venus\Venus.uproject"
```

Or trigger a build from the UE Editor (Build > Build Solution).

### Known risk areas (ordered by probability)

1. **FTcpListener API** (HIGH) — `ArgusDebugTcpServer.cpp` uses `FTcpListener` constructor + `Init()` + `OnConnectionAccepted()`. The API surface may differ in UE 5.5. If `Init()` doesn't exist, the listener may auto-start on construction. If the delegate binding fails, check that the signature matches `bool(FSocket*, const FIPv4Endpoint&)`.

2. **DECLARE_DELEGATE inside namespace** (MEDIUM) — `ArgusDebugTcpServer.h` lines 22-25 declares `FOnBuildHandshakeResponse` via `DECLARE_DELEGATE_RetVal_OneParam` inside `namespace Argus`. UE delegate macros may fail inside namespaces. Fix: move the declaration outside the namespace, prefixing types with `Argus::`.

3. **EnTT include paths** (MEDIUM) — `ArgusFragmentTypeMap.h` uses `<entt/core/type_info.hpp>` and `<entt/entity/fwd.hpp>`. CkThirdParty exposes includes via `PublicIncludePaths` so angle brackets should work. If not, try quoted includes or add `CkThirdParty` as a dependency in `ArgusDebugServer.Build.cs`.

4. **FPlatformProcess::GetSynchEventFromPool** (MEDIUM) — `ArgusDebugTcpServer.cpp` line 483. May be deprecated in UE 5.5. Replace with `FPlatformProcess::CreateSynchEvent(true)` and change `ReturnSynchEventToPool(DoneEvent)` to `delete DoneEvent`.

5. **Fragment header includes** (LOW) — `ArgusFragmentRegistration.cpp` includes `CkEcsExt/Transform/CkTransform_Fragment_Data.h` and `CkEcs/Net/CkNet_Fragment_Data.h`. Include paths may differ; check actual filesystem paths in CkFoundation.

6. **StringCast usage** (LOW) — `ArgusMsgPack.cpp` uses `StringCast<UTF8CHAR>(*Value)` and `StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(...), Len)`. Standard UE API but verify compilation.

7. **FIPv4Endpoint in header** (LOW) — `ArgusDebugTcpServer.h` forward declares `class FTcpListener` but uses `FIPv4Endpoint` in `HandleConnectionAccepted` signature without including the header. The .cpp includes it, but the .h may need `#include "Interfaces/IPv4/IPv4Endpoint.h"` or a forward declare of `struct FIPv4Endpoint`.

### Task

1. Read `Docs/ArgusDebugServer-CompileSession.md` for full context
2. Trigger the UE build
3. Fix each compiler/linker error iteratively
4. Rebuild until zero errors
5. Verify the module loads by checking the Output Log for `"ArgusDebugServer: Listening on port 47522"`

### Acceptance criteria

- [ ] Zero compiler errors — `ArgusDebugServer` module compiles (Win64 Development Editor)
- [ ] Zero linker errors — all symbols resolve
- [ ] CkFoundation compiles — `Get_InternalRegistryRawPtr()` addition doesn't break CkFoundation build
- [ ] Module loads — UE Editor starts without crash
- [ ] Startup log — Output Log contains `"ArgusDebugServer: Listening on port 47522"`

### Important notes

- **Do NOT modify the protocol wire format** — it must match the Argus client exactly (see `D:\Repositories\Argus\src\protocol\messages.h`)
- **Do NOT change the MessagePack encoding** — field order and types must match Argus's `MSGPACK_DEFINE` declarations
- **Preserve the architecture** — TCP server, handshake builder, fragment type map, and MsgPack codec are separate concerns by design
- **CkFoundation changes** should be minimal — ideally only the `Get_InternalRegistryRawPtr()` addition already on disk

---
