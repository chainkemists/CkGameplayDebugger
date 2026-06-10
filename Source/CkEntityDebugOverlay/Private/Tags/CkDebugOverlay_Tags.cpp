#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger,          "Ck.OnScreenDebugger");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout,   "Ck.OnScreenDebugger.Layout");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider, "Ck.OnScreenDebugger.Provider");

// Layout leaf tags
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Layout_All,       "Ck.OnScreenDebugger.Layout.All");
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

    auto Get_ProviderAbbrev(const FString& InProviderLeaf) -> FString
    {
        static const TMap<FString, FString> Abbrevs = {
            { TEXT("StateMachine"),      TEXT("SM")   },
            { TEXT("Goap"),              TEXT("GOAP") },
            { TEXT("Physics"),           TEXT("PHY")  },
            { TEXT("FloatAttributes"),   TEXT("fATT") },
            { TEXT("IntegerAttributes"), TEXT("iATT") },
            { TEXT("Transform"),         TEXT("XFM")  },
            { TEXT("EntityInfo"),        TEXT("INFO") },
            // Pre-mapped for providers still to be ported:
            { TEXT("Inventory"),         TEXT("INV")  },
            { TEXT("Aggro"),             TEXT("AGRO") },
            { TEXT("AStar"),             TEXT("A*")   },
            { TEXT("Objective"),         TEXT("OBJ")  },
            { TEXT("InteractTarget"),    TEXT("INTR") },
            { TEXT("AnimPlans"),         TEXT("ANIM") },
            { TEXT("MontagePlayer"),     TEXT("MTGE") },
            { TEXT("OverlapBody"),       TEXT("OVLP") },
            { TEXT("Shapes"),            TEXT("SHP")  },
            { TEXT("SceneNode"),         TEXT("NODE") },
        };

        if (const auto* Found = Abbrevs.Find(InProviderLeaf))
        { return *Found; }

        return InProviderLeaf.Left(4).ToUpper();
    }

    auto Get_MarkerDepthTint(int32 InDepth) -> FLinearColor
    {
        static const FLinearColor Tints[] = {
            { 0.30f, 0.62f, 1.00f, 1.0f }, // 0 — blue (ECS picker default)
            { 0.10f, 1.00f, 0.15f, 1.0f }, // 1 — green
            { 1.00f, 0.90f, 0.00f, 1.0f }, // 2 — yellow
            { 1.00f, 0.45f, 0.00f, 1.0f }, // 3 — orange
            { 1.00f, 0.05f, 0.60f, 1.0f }, // 4+ — magenta
        };
        constexpr auto TintCount = static_cast<int32>(UE_ARRAY_COUNT(Tints));
        return Tints[FMath::Clamp(InDepth, 0, TintCount - 1)];
    }
}

// --------------------------------------------------------------------------------------------------------------------
