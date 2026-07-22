#include "CkInspector_Poi.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkPoi/CkPoi_Utils.h"

#include "CkEntityTag/CkEntityTag_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Poi)

// =====================================================================================================================

auto FCkInspector_Poi::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Poi"));
}

auto FCkInspector_Poi::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Poi_UE::Has(Entity);
}

auto FCkInspector_Poi::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

auto FCkInspector_Poi::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto PoiHandle = UCk_Utils_Poi_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(PoiHandle))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedPoi = PoiHandle;

    Builder.AddRow(
        FText::FromString(TEXT("Category Tags:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            const auto CategoryTags = UCk_Utils_Poi_UE::Get_CategoryTags(CapturedPoi);
            return FText::FromString(CategoryTags.IsEmpty() ? TEXT("(none)") : CategoryTags.ToStringSimple());
        },
        CkStyle::Value_Tag());

    Builder.AddRow(
        FText::FromString(TEXT("Label:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            // CkLabel Get_Label ensures on unlabeled entities — gate with Has first.
            if (NOT UCk_Utils_GameplayLabel_UE::Has(CapturedPoi)) { return FText::FromString(TEXT("(none)")); }
            const auto Label = UCk_Utils_GameplayLabel_UE::Get_Label(CapturedPoi);
            return Label.IsValid() ? FText::FromName(Label.GetTagName()) : FText::FromString(TEXT("(none)"));
        },
        CkStyle::Value_Tag());

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Disabled:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            const auto IsDisabled = UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(CapturedPoi, Tag_Poi_DisabledName);
            return FText::FromString(ck::Format_UE(TEXT("{}"), IsDisabled));
        },
        [CapturedPoi](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return CkStyle::None(); }
            const auto IsDisabled = UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(CapturedPoi, Tag_Poi_DisabledName);
            return IsDisabled ? CkStyle::Value_Bool_False() : CkStyle::Status_Active();
        });

    Builder.AddRow(
        FText::FromString(TEXT("World Pos:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            const auto WorldLocation = UCk_Utils_Poi_UE::Get_WorldLocation(CapturedPoi);
            return FText::FromString(ck::Format_UE(TEXT("X {:.0f}  Y {:.0f}  Z {:.0f}"),
                WorldLocation.X, WorldLocation.Y, WorldLocation.Z));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
