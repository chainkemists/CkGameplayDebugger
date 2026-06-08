#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger,          "Ck.OnScreenDebugger");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout,   "Ck.OnScreenDebugger.Layout");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider, "Ck.OnScreenDebugger.Provider");

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
