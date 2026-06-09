#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger,          "Ck.OnScreenDebugger");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout,   "Ck.OnScreenDebugger.Layout");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider, "Ck.OnScreenDebugger.Provider");

// Layout leaf tags
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout_AI,        "Ck.OnScreenDebugger.Layout.AI");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout_Animation, "Ck.OnScreenDebugger.Layout.Animation");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout_Movement,  "Ck.OnScreenDebugger.Layout.Movement");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout_Combat,    "Ck.OnScreenDebugger.Layout.Combat");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout_Overview,  "Ck.OnScreenDebugger.Layout.Overview");

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugoverlay
{
    auto Get_LeafName(const FGameplayTag& InTag) -> FString
    {
        if (!InTag.IsValid()) { return {}; }
        FString Full = InTag.GetTagName().ToString();
        int32 Dot;
        return Full.FindLastChar(TEXT('.'), Dot) ? Full.RightChop(Dot + 1) : Full;
    }
}

// --------------------------------------------------------------------------------------------------------------------
