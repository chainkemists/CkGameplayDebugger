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

namespace ck_inspector_poi
{
    // Category tags are the POI's identity — set at construction and effectively static — so the chips row's
    // compose-time snapshot semantics are honest here.
    static auto Make_CategoryChips(
        const FCk_Handle_Poi& InPoi)
        -> TArray<FCkInspector_Chip>
    {
        auto Chips = TArray<FCkInspector_Chip>{};

        if (ck::Is_NOT_Valid(InPoi))
        { return Chips; }

        for (const auto& Tag : UCk_Utils_Poi_UE::Get_CategoryTags(InPoi))
        {
            Chips.Add(FCkInspector_Chip{FText::FromName(Tag.GetTagName()), ECk_Tone::Neutral});
        }

        return Chips;
    }

    static auto Make_AxisComponents(
        const FCk_Handle_Poi& InPoi,
        TFunction<FVector(const FCk_Handle_Poi&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InPoi, InProjector, Axis]()
            {
                if (ck::Is_NOT_Valid(InPoi))
                { return FText::FromString(TEXT("--")); }

                return FText::FromString(ck::Format_UE(TEXT("{:.0f}"), InProjector(InPoi)[Axis]));
            }));
        }

        return Components;
    }
}

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

    if (const auto CategoryChips = ck_inspector_poi::Make_CategoryChips(CapturedPoi); NOT CategoryChips.IsEmpty())
    {
        Builder.AddChipsRow(FText::FromString(TEXT("Category Tags:")), CategoryChips);
    }
    else
    {
        Builder.AddRow(
            FText::FromString(TEXT("Category Tags:")),
            [](const FCk_Handle&) { return FText::FromString(TEXT("(none)")); },
            CkStyle::TextDim());
    }

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

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedPoi]() -> FText
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(
                UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(CapturedPoi, Tag_Poi_DisabledName)
                    ? TEXT("Disabled")
                    : TEXT("Enabled"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedPoi]() -> ECk_Tone
        {
            if (ck::Is_NOT_Valid(CapturedPoi)) { return ECk_Tone::Neutral; }
            return UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(CapturedPoi, Tag_Poi_DisabledName)
                ? ECk_Tone::Err
                : ECk_Tone::Ok;
        }));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("World Pos:")),
        ck_inspector_poi::Make_AxisComponents(CapturedPoi,
            [](const FCk_Handle_Poi& InPoi) { return UCk_Utils_Poi_UE::Get_WorldLocation(InPoi); }));

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
