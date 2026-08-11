#pragma once

#include "CoreMinimal.h"

#include "InputCoreTypes.h"

// --------------------------------------------------------------------------------------------------------------------
// The physical keyboard, as data. One row per keycap: which FKey lights it, the cap legend, and the cap's rectangle
// in KEY UNITS (1u = one letter-key width — the mechanical-keyboard convention, so a full-size board is ~23u x 6.5u
// and every cap width below reads against real layouts). The widget scales units to pixels at paint time, which is
// what keeps the board crisp at any DPI without a single image.
//
// A def may carry EKeys::Invalid: a cap that exists on the physical board but has no engine key to light
// (PrintScreen, the menu key). It renders and never lights — drawing the board with holes in it would read as a
// bug, not as honesty about the engine's key space.
// --------------------------------------------------------------------------------------------------------------------

struct FCkDebug_DeviceKeyDef
{
    FKey Key;
    FString Label;

    float X = 0.0f;
    float Y = 0.0f;
    float W = 1.0f;
    float H = 1.0f;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::debug_devices
{
    // ANSI full-size: main block + F-row, nav cluster, arrows, numpad. Built once, on first call.
    CKDEBUGGERCOMMON_API auto Get_KeyboardLayout_AnsiFull() -> const TArray<FCkDebug_DeviceKeyDef>&;

    // The layout's bounding box in key units — the widget's aspect and desired size derive from it.
    CKDEBUGGERCOMMON_API auto Get_KeyboardLayout_ExtentUnits() -> FVector2D;
}

// --------------------------------------------------------------------------------------------------------------------
