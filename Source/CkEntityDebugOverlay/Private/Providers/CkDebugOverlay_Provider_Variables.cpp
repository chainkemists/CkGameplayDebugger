#include "CkDebugOverlay_Provider_Variables.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"

// Variables fragments — mirroring CkInspector_Variables.cpp includes
#include "CkVariables/CkUnrealVariables_Fragment.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Variables,
    "Ck.OnScreenDebugger.Provider.Variables")

// A single "catch-all" field tag for the collection.
// All variable-count rows share this tag; the type name is embedded in the value string.
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Variables_Value,
    "Ck.OnScreenDebugger.Provider.Variables.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_Variables; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_Variables_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Variables::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Variables::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_Variables::CanInspect:
//   Entity.Has_Any<all ck::FFragment_Variable_* types>()
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Variables::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }

    return Entity.Has_Any<
        ck::FFragment_Variable_Bool,
        ck::FFragment_Variable_Byte,
        ck::FFragment_Variable_Int32,
        ck::FFragment_Variable_Int64,
        ck::FFragment_Variable_Float,
        ck::FFragment_Variable_Name,
        ck::FFragment_Variable_String,
        ck::FFragment_Variable_Text,
        ck::FFragment_Variable_Vector,
        ck::FFragment_Variable_Vector2D,
        ck::FFragment_Variable_Rotator,
        ck::FFragment_Variable_Transform,
        ck::FFragment_Variable_GameplayTag,
        ck::FFragment_Variable_GameplayTagContainer,
        ck::FFragment_Variable_LinearColor,
        ck::FFragment_Variable_Entity>();
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// FieldTag approach: all rows share FieldTag_Value().
// Each row reports one variable type that is present: "Bool: 3", "Float: 1", etc.
// This avoids synthesizing dynamic per-variable gameplay tags at runtime.
//
// BATCH-VERIFY:
//   Entity.Has<ck::FFragment_Variable_X>()              — confirmed from CkInspector_Variables.cpp
//   Entity.Get<ck::FFragment_Variable_X>().Get_Variables() — confirmed from CkInspector_Variables.cpp
//   TFragment_Variables::Get_Variables() returns a map; Num() gives entry count
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Variables::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    // Helper lambda: emit one summary row per variable type that has entries.
    auto AddTypeRow = [&](const FString& TypeName, int32 Count)
    {
        if (Count <= 0) { return; }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value    = FText::FromString(FString::Printf(TEXT("%s: %d"), *TypeName, Count));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    };

    if (Entity.Has<ck::FFragment_Variable_Bool>())
        AddTypeRow(TEXT("Bool"),             Entity.Get<ck::FFragment_Variable_Bool>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Byte>())
        AddTypeRow(TEXT("Byte"),             Entity.Get<ck::FFragment_Variable_Byte>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Int32>())
        AddTypeRow(TEXT("Int32"),            Entity.Get<ck::FFragment_Variable_Int32>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Int64>())
        AddTypeRow(TEXT("Int64"),            Entity.Get<ck::FFragment_Variable_Int64>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Float>())
        AddTypeRow(TEXT("Float"),            Entity.Get<ck::FFragment_Variable_Float>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Name>())
        AddTypeRow(TEXT("Name"),             Entity.Get<ck::FFragment_Variable_Name>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_String>())
        AddTypeRow(TEXT("String"),           Entity.Get<ck::FFragment_Variable_String>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Text>())
        AddTypeRow(TEXT("Text"),             Entity.Get<ck::FFragment_Variable_Text>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Vector>())
        AddTypeRow(TEXT("Vector"),           Entity.Get<ck::FFragment_Variable_Vector>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Vector2D>())
        AddTypeRow(TEXT("Vector2D"),         Entity.Get<ck::FFragment_Variable_Vector2D>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Rotator>())
        AddTypeRow(TEXT("Rotator"),          Entity.Get<ck::FFragment_Variable_Rotator>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Transform>())
        AddTypeRow(TEXT("Transform"),        Entity.Get<ck::FFragment_Variable_Transform>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_GameplayTag>())
        AddTypeRow(TEXT("GameplayTag"),      Entity.Get<ck::FFragment_Variable_GameplayTag>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_GameplayTagContainer>())
        AddTypeRow(TEXT("TagContainer"),     Entity.Get<ck::FFragment_Variable_GameplayTagContainer>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_LinearColor>())
        AddTypeRow(TEXT("LinearColor"),      Entity.Get<ck::FFragment_Variable_LinearColor>().Get_Variables().Num());
    if (Entity.Has<ck::FFragment_Variable_Entity>())
        AddTypeRow(TEXT("Entity"),           Entity.Get<ck::FFragment_Variable_Entity>().Get_Variables().Num());
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Variables::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    // Sum total variable count across all present fragment types.
    int32 Total = 0;

    if (Entity.Has<ck::FFragment_Variable_Bool>())
        Total += Entity.Get<ck::FFragment_Variable_Bool>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Byte>())
        Total += Entity.Get<ck::FFragment_Variable_Byte>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Int32>())
        Total += Entity.Get<ck::FFragment_Variable_Int32>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Int64>())
        Total += Entity.Get<ck::FFragment_Variable_Int64>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Float>())
        Total += Entity.Get<ck::FFragment_Variable_Float>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Name>())
        Total += Entity.Get<ck::FFragment_Variable_Name>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_String>())
        Total += Entity.Get<ck::FFragment_Variable_String>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Text>())
        Total += Entity.Get<ck::FFragment_Variable_Text>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Vector>())
        Total += Entity.Get<ck::FFragment_Variable_Vector>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Vector2D>())
        Total += Entity.Get<ck::FFragment_Variable_Vector2D>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Rotator>())
        Total += Entity.Get<ck::FFragment_Variable_Rotator>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Transform>())
        Total += Entity.Get<ck::FFragment_Variable_Transform>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_GameplayTag>())
        Total += Entity.Get<ck::FFragment_Variable_GameplayTag>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_GameplayTagContainer>())
        Total += Entity.Get<ck::FFragment_Variable_GameplayTagContainer>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_LinearColor>())
        Total += Entity.Get<ck::FFragment_Variable_LinearColor>().Get_Variables().Num();
    if (Entity.Has<ck::FFragment_Variable_Entity>())
        Total += Entity.Get<ck::FFragment_Variable_Entity>().Get_Variables().Num();

    if (Total == 0) { return {}; }
    return FString::Printf(TEXT("Var:%d"), Total);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Variables)

// --------------------------------------------------------------------------------------------------------------------
