#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// THE canonical widget for rendering a shortened EntityScript class name or
// gameplay-tag path. Every debugger that shows either should use this instead
// of formatting names into text blocks by hand.
//
//   - Get_ShortName() is the one shortener: strips the "Default__" CDO prefix
//     and the "_C" class suffix, splits on '_' and '.', and keeps the last
//     NameDepth segments joined with "." (depth 0 = full joined name).
//   - The short text is selectable/copyable; the tooltip always shows the
//     full name.
//   - When the name actually got shortened, a tiny »/« button appears to
//     expand/contract it in place (no rebuild).
//
// NameDepth is attribute-bound so the owning debugger's name-depth tuner
// drives every label live from one source of truth.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_NameLabel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_NameLabel)
        : _NameDepth(1)
        , _ColorAndOpacity(FSlateColor::UseForeground())
    {}
        // The full class name or gameplay-tag string.
        SLATE_ARGUMENT(FString, FullName)
        // Optional glyphs rendered before the name (role markers like "◆● ").
        SLATE_ARGUMENT(FString, Prefix)
        SLATE_ATTRIBUTE(int32, NameDepth)
        SLATE_ATTRIBUTE(FSlateFontInfo, Font)
        SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Class-name/tag → display-name. Examples (depth 1 / 2):
    //   "Ck_GoapGym_Patrol_GoToWaypoint_C"    → "GoToWaypoint" / "Patrol.GoToWaypoint"
    //   "Default__Ck_GoapFEARGym_AttackEnemy" → "AttackEnemy" / "GoapFEARGym.AttackEnemy"
    //   "Gym.GoapFEAR.WS.Combatant"           → "Combatant" / "WS.Combatant"
    static auto Get_ShortName(const FString& InFullName, int32 InDepth) -> FString;

    // Segment count under the same strip/split rules — feeds a depth cycler's
    // MaxDepth (largest meaningful depth for a content set). Returns >= 1.
    static auto Get_SegmentCount(const FString& InFullName) -> int32;

private:
    FString _FullName;
    FString _Prefix;
    TAttribute<int32> _NameDepth;
    bool _Expanded = false;
};

// ====================================================================================================================
