#pragma once

#include <CoreMinimal.h>

#if WITH_GAMEPLAY_DEBUGGER

#include <GameplayDebuggerCategoryReplicator.h>

#else

// --------------------------------------------------------------------------------------------------------------------

// NOTES: This is a stub to make Unreal happy in shipping builds (debugger is not compiled in Shipping builds anyway)
class CKGAMEPLAYDEBUGGER_API AGameplayDebuggerCategoryReplicator { };

// --------------------------------------------------------------------------------------------------------------------

#endif