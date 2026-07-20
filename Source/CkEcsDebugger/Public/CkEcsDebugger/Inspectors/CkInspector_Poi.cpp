#include "CkInspector_Poi.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkPoi/CkPoi_Utils.h"

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
        FText::FromString(TEXT("Category:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(UCk_Utils_Poi_UE::Get_Category(CapturedPoi).ToString());
        },
        CkStyle::Value_Tag());

    Builder.AddRow(
        FText::FromString(TEXT("Display Name:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            const auto DisplayName = UCk_Utils_Poi_UE::Get_DisplayName(CapturedPoi);
            return DisplayName.IsEmpty() ? FText::FromString(TEXT("(none)")) : DisplayName;
        });

    Builder.AddConditionalRow(
        FText::FromString(TEXT("State:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Poi_UE::Get_EnableDisable(CapturedPoi)));
        },
        [CapturedPoi](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return CkStyle::None(); }
            return UCk_Utils_Poi_UE::Get_EnableDisable(CapturedPoi) == ECk_EnableDisable::Enable
                ? CkStyle::Status_Active()
                : CkStyle::Value_Bool_False();
        });

    Builder.AddRow(
        FText::FromString(TEXT("Priority:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Poi_UE::Get_Priority(CapturedPoi)));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Offscreen:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Poi_UE::Get_OffscreenPolicy(CapturedPoi)));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("Max Range:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            const auto MaxVisibleRange = UCk_Utils_Poi_UE::Get_MaxVisibleRange(CapturedPoi);
            return FText::FromString(MaxVisibleRange <= 0.0f
                ? TEXT("0 (unlimited)")
                : ck::Format_UE(TEXT("{:.0f}"), MaxVisibleRange));
        },
        CkStyle::Value_Numeric());

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

    Builder.AddRow(
        FText::FromString(TEXT("State Tags:")),
        [CapturedPoi](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            const auto StateTags = UCk_Utils_Poi_UE::Get_StateTags(CapturedPoi);
            return FText::FromString(StateTags.IsEmpty() ? TEXT("(none)") : StateTags.ToStringSimple());
        },
        CkStyle::Value_Tag());

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
