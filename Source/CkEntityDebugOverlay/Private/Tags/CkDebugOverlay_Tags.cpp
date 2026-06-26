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
            { TEXT("ByteAttributes"),    TEXT("bATT") },
            { TEXT("Transform"),         TEXT("T")    },
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
            { TEXT("Team"),              TEXT("TEAM") },
            { TEXT("TagSet"),            TEXT("TAGS") },
            { TEXT("EntityCollection"),  TEXT("COL")  },
            { TEXT("Timer"),             TEXT("TIMR") },
            { TEXT("Label"),             TEXT("LABL") },
            { TEXT("Variables"),         TEXT("VAR")  },
        };

        if (const auto* Found = Abbrevs.Find(InProviderLeaf))
        { return *Found; }

        return InProviderLeaf.Left(4).ToUpper();
    }
}

// --------------------------------------------------------------------------------------------------------------------
