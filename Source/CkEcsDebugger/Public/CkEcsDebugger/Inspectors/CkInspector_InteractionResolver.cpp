#include "CkInspector_InteractionResolver.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Utils.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_InteractionResolver)

static auto PopulateBadgeBox(
    SWrapBox& InBox,
    const TArray<FCk_Handle>& InHandles,
    TWeakPtr<FCkDebuggerModel_EntitySelection> InWeakModel) -> void
{
    InBox.ClearChildren();

    for (const auto& Handle : InHandles)
    {
        if (ck::Is_NOT_Valid(Handle)) { continue; }

        const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Handle).ToString();
        const auto CapturedHandle = Handle;

        InBox.AddSlot()
            .Padding(FMargin(0.0f, 0.0f, 2.0f, 2.0f))
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(4.0f, 1.0f))
                    .OnClicked_Lambda([InWeakModel, CapturedHandle]()
                    {
                        if (const auto Model = InWeakModel.Pin(); Model.IsValid())
                        {
                            Model->Set_SelectedEntities({ CapturedHandle });
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(DebugName))
                            .ColorAndOpacity(FCkDebuggerStyle::Color_Selection)
                    ]
            ];
    }
}

static auto MakeBadgeBox(
    const TArray<FCk_Handle>& InHandles,
    TWeakPtr<FCkDebuggerModel_EntitySelection> InWeakModel) -> TSharedRef<SWrapBox>
{
    auto Box = SNew(SWrapBox).UseAllottedSize(true);
    PopulateBadgeBox(*Box, InHandles, InWeakModel);
    return Box;
}

// =====================================================================================================================

auto FCkInspector_InteractionResolver::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Interaction Resolver"));
}

auto FCkInspector_InteractionResolver::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_InteractionResolver_UE::Has(Entity);
}

auto FCkInspector_InteractionResolver::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildResolverGrid(Entity);
}

// =====================================================================================================================

auto FCkInspector_InteractionResolver::BuildResolverGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    _BestTargetBoxes.Empty();
    _AvailableTargetsBox.Reset();
    _LastTotalBestCount = 0;
    _LastAvailableCount = 0;

    auto MutableEntity = Entity;
    const auto ResolverHandle = UCk_Utils_InteractionResolver_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(ResolverHandle))
    {
        return Builder.Build(Entity, FString());
    }

    const auto CapturedResolver = ResolverHandle;

    // Read params for intent-channel mappings (static config)
    const auto& Params = ResolverHandle.Get<ck::FFragment_InteractionResolver_Params>();
    const auto& Mappings = Params.Get_IntentChannelMappings();

    for (const auto& Mapping : Mappings)
    {
        // Header per intent
        const auto& Intent = Mapping.Get_Intent();
        const auto IntentName = Intent.IsValid() ? Intent.GetTagName().ToString() : TEXT("Unknown Intent");
        Builder.AddHeader(FText::FromString(IntentName));

        // Channels
        const auto& Channels = Mapping.Get_Channels();
        auto ChannelsStr = FString{};
        for (auto i = 0; i < Channels.Num(); i++)
        {
            if (i > 0) { ChannelsStr += TEXT(", "); }
            ChannelsStr += Channels[i].IsValid() ? Channels[i].GetTagName().ToString() : TEXT("None");
        }
        if (ChannelsStr.IsEmpty()) { ChannelsStr = TEXT("None"); }

        Builder.AddRow(
            FText::FromString(TEXT("Channels:")),
            [ChannelsStr](const FCk_Handle& E) { return FText::FromString(ChannelsStr); },
            FCkDebuggerStyle::Color_Value_Tag);

        // Distance Sorting
        const auto DistanceSorting = Mapping.Get_DistanceSorting();
        Builder.AddRow(
            FText::FromString(TEXT("Distance Sort:")),
            [DistanceSorting](const FCk_Handle& E)
            {
                return FText::FromString(ck::Format_UE(TEXT("{}"), DistanceSorting));
            },
            FCkDebuggerStyle::Color_Value_Enum);

        // Max Concurrent
        const auto MaxConcurrent = Mapping.Get_MaxConcurrentInteractions();
        Builder.AddRow(
            FText::FromString(TEXT("Max Concurrent:")),
            [MaxConcurrent](const FCk_Handle& E)
            {
                return FText::FromString(ck::Format_UE(TEXT("{}"), MaxConcurrent));
            },
            FCkDebuggerStyle::Color_Value_Numeric);

        // Best Targets — live badge box stored for in-place updates
        {
            auto BestHandles = TArray<FCk_Handle>{};
            for (const FCk_Handle_InteractTarget& T : UCk_Utils_InteractionResolver_UE::Get_BestInteractTargets(CapturedResolver, Intent))
            {
                if (ck::IsValid(T)) { BestHandles.Add(T); }
            }
            _LastTotalBestCount += BestHandles.Num();

            auto BestBox = MakeBadgeBox(BestHandles, SelectionModel);
            _BestTargetBoxes.Add(BestBox);
            Builder.AddWidgetRow(FText::FromString(TEXT("Best Targets:")), BestBox);
        }
    }

    // Runtime section
    Builder.AddHeader(FText::FromString(TEXT("Runtime")));

    // Active Intents (live-updating via TAttribute)
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Active Intents:")),
        [CapturedResolver](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedResolver)) { return FText::FromString(TEXT("--")); }
            const auto& Current = CapturedResolver.Get<ck::FFragment_InteractionResolver_Current>();
            const auto& ActiveIntents = Current.Get_ActiveIntents();
            if (ActiveIntents.IsEmpty()) { return FText::FromString(TEXT("None")); }

            auto Result = FString{};
            for (const auto& Intent : ActiveIntents)
            {
                if (NOT Result.IsEmpty()) { Result += TEXT(", "); }
                Result += Intent.IsValid() ? Intent.GetTagName().ToString() : TEXT("?");
            }
            return FText::FromString(Result);
        },
        [CapturedResolver](const FCk_Handle& E) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedResolver)) { return FCkDebuggerStyle::Color_None; }
            const auto& Current = CapturedResolver.Get<ck::FFragment_InteractionResolver_Current>();
            return Current.Get_ActiveIntents().IsEmpty()
                ? FCkDebuggerStyle::Color_Text_Secondary
                : FCkDebuggerStyle::Color_Status_Active;
        });

    // Available Targets — live badge box stored for in-place updates
    {
        auto AvailHandles = TArray<FCk_Handle>{};
        const auto& Current = CapturedResolver.Get<ck::FFragment_InteractionResolver_Current>();
        for (const FCk_Handle_InteractTarget& T : Current.Get_AvailableTargets())
        {
            if (ck::IsValid(T)) { AvailHandles.Add(T); }
        }
        _LastAvailableCount = AvailHandles.Num();

        _AvailableTargetsBox = MakeBadgeBox(AvailHandles, SelectionModel);
        Builder.AddWidgetRow(FText::FromString(TEXT("Available Targets:")), _AvailableTargetsBox.ToSharedRef());
    }

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_InteractionResolver::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_InteractionResolver_UE::Has(Entity))
    { return; }

    auto MutableEntity = Entity;
    const auto Resolver = UCk_Utils_InteractionResolver_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(Resolver))
    { return; }

    const auto& Current = Resolver.Get<ck::FFragment_InteractionResolver_Current>();

    // Available targets
    if (_AvailableTargetsBox.IsValid())
    {
        const auto AvailableCount = static_cast<int32>(Current.Get_AvailableTargets().Num());
        if (AvailableCount != _LastAvailableCount)
        {
            _LastAvailableCount = AvailableCount;

            auto AvailHandles = TArray<FCk_Handle>{};
            for (const FCk_Handle_InteractTarget& T : Current.Get_AvailableTargets())
            {
                if (ck::IsValid(T)) { AvailHandles.Add(T); }
            }
            PopulateBadgeBox(*_AvailableTargetsBox, AvailHandles, SelectionModel);
        }
    }

    // Best targets per intent
    const auto& Params = Resolver.Get<ck::FFragment_InteractionResolver_Params>();
    const auto& Mappings = Params.Get_IntentChannelMappings();
    auto TotalBestCount = 0;
    for (const auto& Mapping : Mappings)
    {
        TotalBestCount += UCk_Utils_InteractionResolver_UE::Get_BestInteractTargets(Resolver, Mapping.Get_Intent()).Num();
    }

    if (TotalBestCount != _LastTotalBestCount)
    {
        _LastTotalBestCount = TotalBestCount;

        for (auto i = 0; i < Mappings.Num() && i < _BestTargetBoxes.Num(); ++i)
        {
            if (NOT _BestTargetBoxes[i].IsValid()) { continue; }

            auto BestHandles = TArray<FCk_Handle>{};
            for (const FCk_Handle_InteractTarget& T : UCk_Utils_InteractionResolver_UE::Get_BestInteractTargets(Resolver, Mappings[i].Get_Intent()))
            {
                if (ck::IsValid(T)) { BestHandles.Add(T); }
            }
            PopulateBadgeBox(*_BestTargetBoxes[i], BestHandles, SelectionModel);
        }
    }
}
