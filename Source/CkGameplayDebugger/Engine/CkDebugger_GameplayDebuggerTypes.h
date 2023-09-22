#pragma once

#include <CoreMinimal.h>
#include <Engine/Canvas.h>

#if WITH_GAMEPLAY_DEBUGGER

#include <GameplayDebuggerTypes.h>

#else

// --------------------------------------------------------------------------------------------------------------------

// NOTES: This is a stub to make Unreal happy in shipping builds (debugger is not compiled in Shipping builds anyway)
class CKGAMEPLAYDEBUGGER_API FGameplayDebuggerCanvasContext { };

// --------------------------------------------------------------------------------------------------------------------

#endif