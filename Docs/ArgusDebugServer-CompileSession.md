# ArgusDebugServer — Compile & Fix Session

> **Goal:** Compile the ArgusDebugServer module in UE and fix all errors until it builds cleanly.
> **Scope:** Compile + fix only. No runtime testing this session.
> **Prereq:** All 12 source files + 2 modified files exist on disk from prior session.
> **Last Updated:** 2026-02-06 (updated after DS-1.2 refactor)

---

## 1. What Was Built (Prior Session)

A new `ArgusDebugServer` UE module inside the `CkGameplayDebugger` plugin. It implements a TCP debug server that the Argus external debugger connects to for ECS inspection.

### Current Files (after DS-1.2 refactor)

```
Source/ArgusDebugServer/
├── ArgusDebugServer.Build.cs          ← Module build config (CkModuleRules)
├── Public/
│   ├── ArgusDebugServerModule.h       ← IModuleInterface (StartupModule/ShutdownModule)
│   ├── ArgusDebugTcpServer.h          ← TCP server: FTcpListener + FRunnable worker
│   ├── ArgusHandshakeBuilder.h        ← Builds HandshakeResponse from UE world state
│   ├── ArgusProtocolTypes.h           ← Frame constants + message POD structs
│   └── ArgusMsgPack.h                 ← Minimal MessagePack encoder/decoder
└── Private/
    ├── ArgusDebugServerModule.cpp     ← Module lifecycle + TCP server init
    ├── ArgusDebugTcpServer.cpp        ← Full TCP server + frame encode/decode + serialization
    ├── ArgusHandshakeBuilder.cpp      ← Registry enumeration + UE reflection (reads FCk_FragmentReflectionRegistry)
    ├── ArgusMsgPack.cpp               ← MessagePack codec (~280 lines)
    └── ArgusFragmentRegistration_Temp.cpp  ← TEMPORARY: registers 4 USTRUCTs with CK_REGISTER_ECS_FRAGMENT_REFLECTED
```

**Deleted in DS-1.2** (no longer exist):
- ~~`Public/ArgusFragmentTypeMap.h`~~ → replaced by `CkFoundation/CkEcs/Reflection/CkFragmentReflectionRegistry.h`
- ~~`Private/ArgusFragmentTypeMap.cpp`~~ → replaced
- ~~`Private/ArgusFragmentRegistration.cpp`~~ → replaced by `ArgusFragmentRegistration_Temp.cpp`

### CkFoundation Dependencies (added in DS-1.1 + DS-1.2)

| File | Change |
|------|--------|
| `CkDebugger.uplugin` | Added ArgusDebugServer module entry (UncookedOnly, Win64) |
| `CkEcs/Registry/CkRegistry.h` | Added `Get_InternalRegistryRawPtr()` public method |
| `CkEcs/Reflection/CkFragmentTypeInfo.h` | **NEW** — FCk_FragmentTypeInfo struct |
| `CkEcs/Reflection/CkFragmentReflectionRegistry.h/cpp` | **NEW** — Registry singleton |
| `CkEcs/Reflection/CkFragmentReflection_Macros.h` | **NEW** — Registration macros |

---

## 2. Task: Compile the Module

### Step-by-step

1. **Open UE project** that uses CkGameplayDebugger plugin (Venus project)
2. **Build** — trigger Editor build (Win64, Development)
3. **Fix errors** — iterate through compiler/linker errors, fix each one
4. **Rebuild** — repeat until zero errors
5. **Verify module loads** — check Output Log for the startup message

### Build command (if using command line)
```
UnrealBuildTool.exe VenusEditor Win64 Development -Project="D:\Repositories\Venus\Venus.uproject"
```

---

## 3. Known Risk Areas (Likely Compilation Issues)

These are the areas most likely to produce compiler errors. Address them in order of probability:

### 3.1 FTcpListener API (HIGH probability)

**File:** `ArgusDebugTcpServer.cpp`

The `FTcpListener` constructor and `Init()` method may differ across UE versions. In UE 5.5:
- Constructor: `FTcpListener(const FIPv4Endpoint&, const FTimespan&, bool)`
- The `Init()` method starts the listener thread
- `OnConnectionAccepted()` returns an `FOnTcpListenerConnectionAccepted` delegate

**If `Init()` doesn't exist:** The listener may auto-start on construction. Remove the `Init()` call and check if the listener is valid after construction.

**If delegate binding fails:** The delegate signature is `bool(FSocket*, const FIPv4Endpoint&)`. Verify the method signature matches exactly.

### 3.2 DECLARE_DELEGATE Inside Namespace (MEDIUM probability)

**File:** `ArgusDebugTcpServer.h`, line 22-25

```cpp
namespace Argus
{
    DECLARE_DELEGATE_RetVal_OneParam(
        FHandshakeResponse,
        FOnBuildHandshakeResponse,
        const FHandshakeRequest&);
```

UE delegate macros may not work inside a namespace. **Fix:** Move the `DECLARE_DELEGATE_RetVal_OneParam` call **outside** the namespace, prefixing the types with `Argus::`.

### 3.3 EnTT Include Paths (MEDIUM probability)

**File:** `ArgusFragmentTypeMap.h`

Uses `<entt/core/type_info.hpp>` and `<entt/entity/fwd.hpp>`. These need to be on the include path. CkEcs module should expose them, but verify.

**Fix if not found:** Change to quoted includes matching CkFoundation's pattern: `"entt/core/type_info.hpp"`. Or add CkThirdParty include path in Build.cs.

### 3.4 FPlatformProcess::GetSynchEventFromPool (MEDIUM probability)

**File:** `ArgusDebugTcpServer.cpp`, line 483

```cpp
FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true);
```

May be deprecated in UE 5.5. **Fix:** Replace with `FPlatformProcess::CreateSynchEvent(true)` and change `ReturnSynchEventToPool` to `delete DoneEvent`.

### 3.5 Fragment Header Includes (LOW probability)

**File:** `ArgusFragmentRegistration.cpp`

Includes `CkEcsExt/Transform/CkTransform_Fragment_Data.h` and `CkEcs/Net/CkNet_Fragment_Data.h`. These should resolve since `CkEcsExt` and `CkEcs` are dependencies, but the exact include paths may differ.

### 3.6 StringCast Usage (LOW probability)

**File:** `ArgusMsgPack.cpp`

Uses `StringCast<UTF8CHAR>(*Value)` and `StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(...), Len)`. This is standard UE API but verify it compiles with the TCHAR width of the project.

### 3.7 Missing FIPv4Endpoint Forward Declaration (LOW probability)

**File:** `ArgusDebugTcpServer.h`

Forward declares `class FTcpListener` but uses `FIPv4Endpoint` in `HandleConnectionAccepted` signature without including the header. **Fix:** Add `#include "Interfaces/IPv4/IPv4Endpoint.h"` or forward declare `struct FIPv4Endpoint`.

---

## 4. Acceptance Criteria

All of these must be true for this session to be complete:

- [ ] **AC-1: Zero compiler errors** — `ArgusDebugServer` module compiles with zero errors (Win64 Development Editor)
- [ ] **AC-2: Zero linker errors** — all symbols resolve (no unresolved externals)
- [ ] **AC-3: CkFoundation compiles** — `Get_InternalRegistryRawPtr()` addition doesn't break CkFoundation build
- [ ] **AC-4: Module loads** — UE Editor starts without crash; ArgusDebugServer appears in loaded modules
- [ ] **AC-5: Startup log** — Output Log contains `"ArgusDebugServer: Listening on port 47522"` on editor startup

### Out of Scope (deferred to future sessions)
- Runtime handshake testing with Argus or Python script
- PIE/play testing
- DrawGizmo command implementation
- Performance profiling

---

## 5. Reference: Key APIs Used

| API | Used In | Purpose |
|-----|---------|---------|
| `FTcpListener` | ArgusDebugTcpServer.cpp | TCP accept loop |
| `FSocket::Recv/Send` | ArgusDebugTcpServer.cpp | Blocking socket I/O |
| `FRunnable/FRunnableThread` | ArgusDebugTcpServer.cpp | Background worker thread |
| `AsyncTask(ENamedThreads::GameThread)` | ArgusDebugTcpServer.cpp | Marshal to game thread |
| `FPlatformProcess::GetSynchEventFromPool` | ArgusDebugTcpServer.cpp | Cross-thread sync |
| `GEngine->GetWorldContexts()` | ArgusHandshakeBuilder.cpp | World enumeration |
| `UWorld::GetSubsystem<>()` | ArgusHandshakeBuilder.cpp | ECS subsystem access |
| `TFieldIterator<FProperty>` | ArgusHandshakeBuilder.cpp | UE property reflection |
| `entt::type_id<T>().hash()` | ArgusFragmentTypeMap.h | EnTT type identification |
| `StringCast<UTF8CHAR>` | ArgusMsgPack.cpp | FString → UTF-8 conversion |

---

## 6. Architecture Context

### How the module fits together

```
StartupModule()
  → RegisterAllFragments()          // ArgusFragmentRegistration.cpp
  → FDebugTcpServer::StartListening()  // Creates FTcpListener on 127.0.0.1:47522
      → OnConnectionAccepted()      // Spawns FClientWorker thread
          → ReadFrame()             // Reads HandshakeRequest
          → HandleHandshake()       // Deserializes request
              → AsyncTask(GameThread) → FHandshakeBuilder::Build()
                  → EnumerateRegistries()   // GEngine→World→Subsystem→Registry
                  → BuildComponentTypes()   // FragmentTypeMap→UScriptStruct→TFieldIterator
              → SerializeHandshakeResponse()
              → SendFrame()         // Sends HandshakeResponse
          → Loop: ReadFrame() for future commands
```

### Protocol wire format (must match Argus client exactly)

```
Frame: [Magic:4B][Version:2B][Length:4B][Payload:NB]  (all big-endian)
Payload: MessagePack array encoding

HandshakeRequest  = array[3]: [client_version, client_name, expected_entt_version]
HandshakeResponse = array[8]: [accepted, server_version, project_name, engine_version,
                               process_id, entt_version, registries[], component_types[]]
RegistryInfo      = array[5]: [world_id, world_name, registry_address, entity_count, net_mode]
ComponentTypeInfo = array[2]: [type_name, properties[]]
PropertyInfo      = array[4]: [name, type_name, offset, size]
```

### Argus client source (for cross-reference)
- Protocol definitions: `D:\Repositories\Argus\src\protocol\messages.h`
- Frame codec: `D:\Repositories\Argus\src\protocol\frame.h` / `frame.cpp`
- Mock server (reference impl): `D:\Repositories\Argus\tests\network\mock_server.cpp`

### CkFoundation key files
- Registry: `D:\Repositories\Venus\Plugins\CkFoundation\Source\CkEcs\Public\CkEcs\Registry\CkRegistry.h`
- World subsystem: `D:\Repositories\Venus\Plugins\CkFoundation\Source\CkEcs\Public\CkEcs\Subsystem\CkEcsWorld_Subsystem.h`
- TPtrWrapper: `D:\Repositories\Venus\Plugins\CkFoundation\Source\CkCore\Public\CkCore\Types\CkPtrWrapper.h`
- CK_PROPERTY_GET macro: `D:\Repositories\Venus\Plugins\CkFoundation\Source\CkCore\Public\CkCore\Macros\CkMacros.h`
- EnTT 3.16.0: `D:\Repositories\Venus\Plugins\CkFoundation\Source\CkThirdParty\Public\CkThirdParty\entt-3.16.0\`
