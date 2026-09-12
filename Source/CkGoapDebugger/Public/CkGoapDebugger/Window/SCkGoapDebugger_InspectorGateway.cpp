#include "CkGoapDebugger/Window/SCkGoapDebugger_InspectorGateway.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/SNullWidget.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// ====================================================================================================================

namespace ck_goap_debugger_gateway_internal
{
    static auto AuthoredTokens() -> FCkUiView::FTokens
    {
        return {
            {TEXT("--space-xs"), FString::SanitizeFloat(CkStyle::SpaceXS)},
            {TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
            {TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM)},
            {TEXT("--goap-gateway-surface"), TEXT("#") + CkStyle::Bg3().ToFColorSRGB().ToHex()},
            {TEXT("--goap-gateway-heading-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeH3()))},
            {TEXT("--goap-gateway-body-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeBody()))},
            {TEXT("--goap-gateway-small-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeSmall()))},
            {TEXT("--goap-gateway-micro-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeMicro()))},
        };
    }

    static auto TextField(const FString& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    static auto ColorField(const FLinearColor& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Color, .Color = InValue}; }

    static auto BoolField(const bool InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Bool, .Bool = InValue}; }

    struct FAuthoredProjection
    {
        TArray<FCkUiRecordData> PlannerRecords;
        TArray<FCkUiRecordData> ChainRecords;
        TArray<FCkUiRecordData> PreviewRecords;
        FString PlannerCount;
        FString ChainHeading;
        FString ChainTags;
        FString LeafHeading;
        FString LeafStatus;
        FString LeafCost;
        FString LeafLength;
        FString PreviewHeading;
        FString PreviewOverflow;
        FLinearColor LeafStatusForeground = FLinearColor::White;
        FLinearColor LeafStatusBackground = FLinearColor::Transparent;
        bool HasContent = false;
        bool HasPlanner = false;
        bool HasLeaf = false;
        bool PreviewOverflowVisible = false;
    };

    // Mirrors the standalone window's status-color lookup so the gateway dots
    // read the same way at-a-glance.
    static auto
    ColorForPlanStatus(
        ECk_GoapPlanStatus InStatus,
        ECk_EnableDisable  InEnable) -> FLinearColor
    {
        if (InEnable == ECk_EnableDisable::Disable)
        { return CkStyle::OverlayOf(CkStyle::TextMute(), 0.75f); }

        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:               return CkStyle::Ok();
        case ECk_GoapPlanStatus::Planning:                return CkStyle::Accent();
        case ECk_GoapPlanStatus::PlanFailed:              return CkStyle::Err();
        case ECk_GoapPlanStatus::CostThresholdReached:    return CkStyle::Warn();
        case ECk_GoapPlanStatus::Idle:
        default:                                          return CkStyle::TextMute();
        }
    }

    static auto
    BackgroundForPlanStatus(
        ECk_GoapPlanStatus InStatus,
        ECk_EnableDisable InEnable) -> FLinearColor
    {
        if (InEnable == ECk_EnableDisable::Disable) { return CkStyle::Bg2(); }

        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:               return CkStyle::GetToneDimColor(ECk_Tone::Ok);
        case ECk_GoapPlanStatus::Planning:                return CkStyle::GetToneDimColor(ECk_Tone::Info);
        case ECk_GoapPlanStatus::PlanFailed:              return CkStyle::GetToneDimColor(ECk_Tone::Err);
        case ECk_GoapPlanStatus::CostThresholdReached:    return CkStyle::GetToneDimColor(ECk_Tone::Warn);
        case ECk_GoapPlanStatus::Idle:
        default:                                          return CkStyle::Bg2();
        }
    }

    static auto
    LabelForPlanStatus(
        ECk_GoapPlanStatus InStatus) -> FString
    {
        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:               return TEXT("PlanFound");
        case ECk_GoapPlanStatus::Planning:                return TEXT("Planning");
        case ECk_GoapPlanStatus::PlanFailed:              return TEXT("PlanFailed");
        case ECk_GoapPlanStatus::CostThresholdReached:    return TEXT("CostThreshold");
        case ECk_GoapPlanStatus::Idle:                    return TEXT("Idle");
        default:                                          return TEXT("(unknown)");
        }
    }

    // Compute the "headline" plan status for a Planner — used by the dot in
    // the Planner list. Picks the deepest active chain entry's status when
    // available; otherwise falls back to the root's status.
    static auto
    HeadlineStatus(
        const FCkGoapDebugger_ActionSetInfo& InAs) -> ECk_GoapPlanStatus
    {
        if (InAs.ActiveChainHandles.Num() > 0)
        {
            const auto& Leaf = InAs.ActiveChainHandles.Last();
            const auto* Info = InAs.Catalog.FindByPredicate(
                [&Leaf](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == Leaf; });
            if (Info != nullptr) { return Info->PlanStatus; }
        }

        if (ck::IsValid(InAs.RootActionHandle))
        {
            const auto* Root = InAs.Catalog.FindByPredicate(
                [&InAs](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == InAs.RootActionHandle; });
            if (Root != nullptr) { return Root->PlanStatus; }
        }

        return ECk_GoapPlanStatus::Idle;
    }

    static auto
    LeafActionInfo(
        const FCkGoapDebugger_ActionSetInfo& InAs) -> const FCkGoapDebugger_ActionInfo*
    {
        if (InAs.ActiveChainHandles.Num() == 0) { return nullptr; }
        const auto& LeafHandle = InAs.ActiveChainHandles.Last();
        return InAs.Catalog.FindByPredicate(
            [&LeafHandle](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == LeafHandle; });
    }

    // Small colored dot used in the Planner list rows + the leaf-status indicator.
    static auto
    MakeStatusDot(
        const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride_Lambda([]() -> FOptionalSize
            { return FOptionalSize{ck_goap_debugger_axes::Get_DotSize()}; })
            .HeightOverride_Lambda([]() -> FOptionalSize
            { return FOptionalSize{ck_goap_debugger_axes::Get_DotSize()}; })
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(InColor)
                    .Padding(FMargin(0.0f))
                    [ SNew(SSpacer) ]
            ];
    }

    // Hash the bits of the snapshot the gateway actually renders. Keeps Tick
    // from re-emitting Slate every frame when nothing visible changed.
    static auto
    HashGatewayState(
        const FCkGoapDebugger_EntitySnapshot* InSnapshot) -> uint32
    {
        if (InSnapshot == nullptr) { return 0; }

        auto Hash = uint32{0};
        Hash = HashCombine(Hash, ::GetTypeHash(InSnapshot->EntityHandle));
        Hash = HashCombine(Hash, ::GetTypeHash(InSnapshot->ActionSets.Num()));

        for (const auto& As : InSnapshot->ActionSets)
        {
            Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(As.Handle)));
            Hash = HashCombine(Hash, GetTypeHash(As.DebugName));
            Hash = HashCombine(Hash, GetTypeHash(As.ActionSetTag));
            Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(As.EnableToggle)));
            Hash = HashCombine(Hash, ::GetTypeHash(As.ActiveChainHandles.Num()));

            for (const auto& ActiveHandle : As.ActiveChainHandles)
            { Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(ActiveHandle))); }

            for (const auto& Action : As.Catalog)
            {
                Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(Action.Handle)));
                Hash = HashCombine(Hash, GetTypeHash(Action.ClassName));
                Hash = HashCombine(Hash, GetTypeHash(Action.ActionTag));
                Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(Action.PlanStatus)));
                Hash = HashCombine(Hash, ::GetTypeHash(Action.PlanCost));
                Hash = HashCombine(Hash, ::GetTypeHash(Action.PlanClassNames.Num()));
                for (const auto& PlanClassName : Action.PlanClassNames)
                { Hash = HashCombine(Hash, GetTypeHash(PlanClassName)); }
            }
        }

        return Hash;
    }
} // namespace ck_goap_debugger_gateway_internal

// ====================================================================================================================
// CONSTRUCT
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Entity = InArgs._Entity;

    ChildSlot
    [
        SAssignNew(_RootHost, SBox)
    ];

    Build_AuthoredView();
    Rebuild();
}

SCkGoapDebugger_InspectorGateway::~SCkGoapDebugger_InspectorGateway()
{
    _Entity = {};
    Clear_AuthoredProjection();
    _AuthoredPlannerRecords.Reset();
    _AuthoredChainRecords.Reset();
    _AuthoredPreviewRecords.Reset();
    _AuthoredView.Reset();
    if (_ContentBox.IsValid()) { _ContentBox->ClearChildren(); }
    if (_RootHost.IsValid()) { _RootHost->SetContent(SNullWidget::NullWidget); }
}

// ====================================================================================================================
// SELECTION
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Set_Entity(
        const FCk_Handle& InEntity)
    -> void
{
    if (_Entity == InEntity) { return; }
    _Entity        = InEntity;
    _HasBuilt      = false;
    _LastBuiltHash = 0;
    Rebuild();
}

// ====================================================================================================================
// TICK — re-pull snapshots and rebuild if the gateway-visible state changed.
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    Poll_AuthoredFiles();

    using namespace ck_goap_debugger_gateway_internal;

    // Honour the per-window refresh gate. The gateway lives inside the
    // EcsDebugger entity inspector, so it consults that window's gate id —
    // matching SCkDebuggerWindow_Main::WindowId. Without this the collect below
    // runs every editor tick even when the user has set "OnlyWhenVisible" or a
    // Hz cap on the EcsDebugger.
    static const auto EcsDebuggerWindowId = FName(TEXT("EcsDebugger"));
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(EcsDebuggerWindowId))
    { return; }

    if (ck::Is_NOT_Valid(_Entity)) { return; }

    auto* World = Resolve_World();
    if (World == nullptr) { return; }

    // Deep tier for THIS entity only. This used to pull the all-agents batch
    // and discard everything but its own slice — the dominant cost of having
    // the gateway open in a populated world.
    const auto MySnapshotOpt = FCkGoapDebugger_DataCollector::CollectFull(World, _Entity);
    const auto* MySnapshot = MySnapshotOpt.GetPtrOrNull();

    const auto NewHash = HashGatewayState(MySnapshot);
    if (_HasBuilt && NewHash == _LastBuiltHash) { return; }

    _LastBuiltHash = NewHash;
    _HasBuilt      = true;
    Present_Snapshot(MySnapshot);
}

// ====================================================================================================================
// REBUILD
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Build_AuthoredView()
    -> void
{
    auto PlannerRecords = TSharedPtr<FCkUiCollection>{};
    auto ChainRecords = TSharedPtr<FCkUiCollection>{};
    auto PreviewRecords = TSharedPtr<FCkUiCollection>{};

    const auto PlannerResult = FCkUiCollection::TryCreate({
        {TEXT("name"), ECkUiFieldKind::Text},
        {TEXT("tag"), ECkUiFieldKind::Text},
        {TEXT("name-foreground"), ECkUiFieldKind::Color},
        {TEXT("status"), ECkUiFieldKind::Text},
        {TEXT("status-foreground"), ECkUiFieldKind::Color},
        {TEXT("status-background"), ECkUiFieldKind::Color},
    }, PlannerRecords);
    const auto ChainResult = FCkUiCollection::TryCreate({
        {TEXT("separator-visible"), ECkUiFieldKind::Bool},
        {TEXT("name"), ECkUiFieldKind::Text},
        {TEXT("foreground"), ECkUiFieldKind::Color},
    }, ChainRecords);
    const auto PreviewResult = FCkUiCollection::TryCreate({
        {TEXT("ordinal"), ECkUiFieldKind::Text},
        {TEXT("name"), ECkUiFieldKind::Text},
        {TEXT("hint"), ECkUiFieldKind::Text},
    }, PreviewRecords);

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const auto RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT PlannerResult.Succeeded || NOT ChainResult.Succeeded || NOT PreviewResult.Succeeded
        || NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = TArray<FString>{};
        Errors.Append(PlannerResult.Errors);
        Errors.Append(ChainResult.Errors);
        Errors.Append(PreviewResult.Errors);
        Errors.Append(RegistryResult.Errors);
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        Activate_NativeFallback(FString::Join(Errors, TEXT("\n")));
        return;
    }

    _AuthoredPlannerRecords = PlannerRecords;
    _AuthoredChainRecords = ChainRecords;
    _AuthoredPreviewRecords = PreviewRecords;

    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkGoapDebugger_InspectorGateway> WeakGateway{SharedThis(this)};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakGateway]() { return WeakGateway.IsValid(); });

    auto BindText = [&Data, WeakGateway](
        const FString& InName,
        FString SCkGoapDebugger_InspectorGateway::* InMember) -> void
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakGateway, InMember]()
        {
            const auto Gateway = WeakGateway.Pin();
            return Gateway.IsValid()
                ? FText::FromString(Gateway.Get()->*InMember)
                : FText::GetEmpty();
        }));
    };
    BindText(TEXT("goap-gateway-planner-count"), &SCkGoapDebugger_InspectorGateway::_AuthoredPlannerCount);
    BindText(TEXT("goap-gateway-chain-heading"), &SCkGoapDebugger_InspectorGateway::_AuthoredChainHeading);
    BindText(TEXT("goap-gateway-chain-tags"), &SCkGoapDebugger_InspectorGateway::_AuthoredChainTags);
    BindText(TEXT("goap-gateway-leaf-heading"), &SCkGoapDebugger_InspectorGateway::_AuthoredLeafHeading);
    BindText(TEXT("goap-gateway-leaf-status"), &SCkGoapDebugger_InspectorGateway::_AuthoredLeafStatus);
    BindText(TEXT("goap-gateway-leaf-cost"), &SCkGoapDebugger_InspectorGateway::_AuthoredLeafCost);
    BindText(TEXT("goap-gateway-leaf-length"), &SCkGoapDebugger_InspectorGateway::_AuthoredLeafLength);
    BindText(TEXT("goap-gateway-preview-heading"), &SCkGoapDebugger_InspectorGateway::_AuthoredPreviewHeading);
    BindText(TEXT("goap-gateway-preview-overflow"), &SCkGoapDebugger_InspectorGateway::_AuthoredPreviewOverflow);

    Data.Color.Add(TEXT("goap-gateway-leaf-status-foreground"),
        TAttribute<FLinearColor>::CreateLambda([WeakGateway]()
        {
            const auto Gateway = WeakGateway.Pin();
            return Gateway.IsValid() ? Gateway->_AuthoredLeafStatusForeground : FLinearColor::White;
        }));
    Data.Color.Add(TEXT("goap-gateway-leaf-status-background"),
        TAttribute<FLinearColor>::CreateLambda([WeakGateway]()
        {
            const auto Gateway = WeakGateway.Pin();
            return Gateway.IsValid() ? Gateway->_AuthoredLeafStatusBackground : FLinearColor::Transparent;
        }));
    Data.Visibility.Add(TEXT("goap-gateway-has-content"), TAttribute<bool>::CreateLambda([WeakGateway]()
    {
        const auto Gateway = WeakGateway.Pin();
        return Gateway.IsValid() && Gateway->_AuthoredHasContent;
    }));
    Data.Visibility.Add(TEXT("goap-gateway-empty-visible"), TAttribute<bool>::CreateLambda([WeakGateway]()
    {
        const auto Gateway = WeakGateway.Pin();
        return NOT Gateway.IsValid() || NOT Gateway->_AuthoredHasContent;
    }));
    Data.Visibility.Add(TEXT("goap-gateway-has-planner"), TAttribute<bool>::CreateLambda([WeakGateway]()
    {
        const auto Gateway = WeakGateway.Pin();
        return Gateway.IsValid() && Gateway->_AuthoredHasPlanner;
    }));
    Data.Visibility.Add(TEXT("goap-gateway-has-leaf"), TAttribute<bool>::CreateLambda([WeakGateway]()
    {
        const auto Gateway = WeakGateway.Pin();
        return Gateway.IsValid() && Gateway->_AuthoredHasLeaf;
    }));
    Data.Visibility.Add(TEXT("goap-gateway-preview-overflow-visible"),
        TAttribute<bool>::CreateLambda([WeakGateway]()
        {
            const auto Gateway = WeakGateway.Pin();
            return Gateway.IsValid() && Gateway->_AuthoredPreviewOverflowVisible;
        }));
    Data.Collections.Add(TEXT("goap-gateway-planners"), _AuthoredPlannerRecords);
    Data.Collections.Add(TEXT("goap-gateway-chain"), _AuthoredChainRecords);
    Data.Collections.Add(TEXT("goap-gateway-preview"), _AuthoredPreviewRecords);

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("goap-gateway-open"), FSimpleDelegate::CreateLambda([WeakGateway]()
    {
        if (const auto Gateway = WeakGateway.Pin(); Gateway.IsValid())
        { Gateway->OnClicked_OpenInGoapDebugger(); }
    }));

    const auto Candidate = FCkUiView::Create(
        {}, MoveTemp(Actions), ck_goap_debugger_gateway_internal::AuthoredTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const auto Main = Candidate->GetRegion(TEXT("main"));
    const auto ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("GoapInspectorGateway.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("GoapInspectorGateway.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        Activate_NativeFallback(FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")));
        return;
    }

    _AuthoredView = Candidate;
    _AuthoredMounted = true;
    _AuthoredLoadError.Reset();
    _RootHost->SetContent(Main);
}

auto
    SCkGoapDebugger_InspectorGateway::
    Poll_AuthoredFiles()
    -> void
{
    if (NOT _AuthoredView.IsValid()) { return; }
    _AuthoredView->PollFiles(ck_goap_debugger_gateway_internal::AuthoredTokens());
    if (NOT _AuthoredView->GetLastResult().Succeeded)
    { _AuthoredLoadError = FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n")); }
    else { _AuthoredLoadError.Reset(); }
}

auto
    SCkGoapDebugger_InspectorGateway::
    Activate_NativeFallback(
        const FString& InError)
    -> void
{
    _AuthoredMounted = false;
    _AuthoredLoadError = InError;
    _AuthoredView.Reset();
    _AuthoredPlannerRecords.Reset();
    _AuthoredChainRecords.Reset();
    _AuthoredPreviewRecords.Reset();
    SAssignNew(_ContentBox, SVerticalBox);
    _RootHost->SetContent(_ContentBox.ToSharedRef());
}

auto
    SCkGoapDebugger_InspectorGateway::
    Present_Snapshot(
        const FCkGoapDebugger_EntitySnapshot* InSnapshot)
    -> void
{
    if (_AuthoredMounted)
    {
        Publish_AuthoredSnapshot(InSnapshot);
        return;
    }
    Rebuild_Native(InSnapshot);
}

auto
    SCkGoapDebugger_InspectorGateway::
    Publish_AuthoredSnapshot(
        const FCkGoapDebugger_EntitySnapshot* InSnapshot)
    -> bool
{
    using namespace ck_goap_debugger_gateway_internal;

    if (InSnapshot == nullptr)
    {
        Clear_AuthoredProjection();
        return true;
    }

    auto Projection = FAuthoredProjection{};
    Projection.HasContent = true;
    Projection.PlannerCount = FString::Printf(TEXT("PLANNERS - %d"), InSnapshot->ActionSets.Num());

    for (auto Index = 0; Index < InSnapshot->ActionSets.Num(); ++Index)
    {
        const auto& Planner = InSnapshot->ActionSets[Index];
        const auto Status = HeadlineStatus(Planner);
        const auto IsDisabled = Planner.EnableToggle == ECk_EnableDisable::Disable;

        auto Record = FCkUiRecordData{};
        Record.Key = FString::Printf(TEXT("planner:%d"), Index);
        Record.Fields.Add(TEXT("name"), TextField(Planner.DebugName));
        Record.Fields.Add(TEXT("tag"), TextField(Planner.ActionSetTag.IsValid()
            ? Planner.ActionSetTag.ToString()
            : FString{}));
        Record.Fields.Add(TEXT("name-foreground"), ColorField(IsDisabled ? CkStyle::TextMute() : CkStyle::Text()));
        Record.Fields.Add(TEXT("status"), TextField(IsDisabled
            ? TEXT("Disabled")
            : FString::Printf(TEXT("%s - %d active"), *LabelForPlanStatus(Status), Planner.ActiveChainHandles.Num())));
        Record.Fields.Add(TEXT("status-foreground"), ColorField(ColorForPlanStatus(Status, Planner.EnableToggle)));
        Record.Fields.Add(TEXT("status-background"), ColorField(BackgroundForPlanStatus(Status, Planner.EnableToggle)));
        Projection.PlannerRecords.Add(MoveTemp(Record));
    }

    const auto* DisplayPlanner = Pick_DisplayActionSet(*InSnapshot);
    if (DisplayPlanner != nullptr)
    {
        Projection.HasPlanner = true;
        Projection.ChainHeading = FString::Printf(TEXT("ACTIVE CHAIN - %s"), *DisplayPlanner->DebugName);

        for (auto Index = 0; Index < DisplayPlanner->ActiveChainHandles.Num(); ++Index)
        {
            const auto& Handle = DisplayPlanner->ActiveChainHandles[Index];
            const auto* Action = DisplayPlanner->Catalog.FindByPredicate(
                [&Handle](const FCkGoapDebugger_ActionInfo& InAction) { return InAction.Handle == Handle; });
            const auto IsLeaf = Index == DisplayPlanner->ActiveChainHandles.Num() - 1;

            auto Record = FCkUiRecordData{};
            Record.Key = FString::Printf(TEXT("chain:%d"), Index);
            Record.Fields.Add(TEXT("separator-visible"), BoolField(Index > 0));
            Record.Fields.Add(TEXT("name"), TextField(Action != nullptr ? Action->ClassName : TEXT("(?)")));
            Record.Fields.Add(TEXT("foreground"), ColorField(IsLeaf ? CkStyle::Warn() : CkStyle::TextDim()));
            Projection.ChainRecords.Add(MoveTemp(Record));

            if (Action != nullptr && Action->ActionTag.IsValid())
            {
                if (NOT Projection.ChainTags.IsEmpty()) { Projection.ChainTags.Append(TEXT(" > ")); }
                Projection.ChainTags.Append(Action->ActionTag.ToString());
            }
        }

        if (DisplayPlanner->ActiveChainHandles.IsEmpty())
        {
            auto Record = FCkUiRecordData{};
            Record.Key = TEXT("chain:empty");
            Record.Fields.Add(TEXT("separator-visible"), BoolField(false));
            Record.Fields.Add(TEXT("name"), TextField(TEXT("(no active chain)")));
            Record.Fields.Add(TEXT("foreground"), ColorField(CkStyle::TextMute()));
            Projection.ChainRecords.Add(MoveTemp(Record));
        }
        Projection.ChainTags = FString::Printf(TEXT("Action tag chain: %s"),
            Projection.ChainTags.IsEmpty() ? TEXT("(none)") : *Projection.ChainTags);

        if (const auto* Leaf = LeafActionInfo(*DisplayPlanner))
        {
            Projection.HasLeaf = true;
            Projection.LeafHeading = FString::Printf(TEXT("LEAF ACTION - %s"), *Leaf->ClassName);
            Projection.LeafStatus = LabelForPlanStatus(Leaf->PlanStatus);
            Projection.LeafCost = FString::Printf(TEXT("%.0f"), Leaf->PlanCost);
            Projection.LeafLength = FString::Printf(TEXT("%d action%s"), Leaf->PlanClassNames.Num(),
                Leaf->PlanClassNames.Num() == 1 ? TEXT("") : TEXT("s"));
            Projection.LeafStatusForeground = ColorForPlanStatus(Leaf->PlanStatus, ECk_EnableDisable::Enable);
            Projection.LeafStatusBackground = BackgroundForPlanStatus(Leaf->PlanStatus, ECk_EnableDisable::Enable);

            const auto Total = Leaf->PlanClassNames.Num();
            const auto PreviewCount = FMath::Min(3, Total);
            Projection.PreviewHeading = Total <= PreviewCount
                ? FString::Printf(TEXT("PLAN (%d)"), Total)
                : FString::Printf(TEXT("PLAN (first %d of %d)"), PreviewCount, Total);
            for (auto Index = 0; Index < PreviewCount; ++Index)
            {
                auto Record = FCkUiRecordData{};
                Record.Key = FString::Printf(TEXT("preview:%d"), Index);
                Record.Fields.Add(TEXT("ordinal"), TextField(FString::Printf(TEXT("%d."), Index + 1)));
                Record.Fields.Add(TEXT("name"), TextField(Leaf->PlanClassNames[Index]));
                Record.Fields.Add(TEXT("hint"), TextField(TEXT("(leaf)")));
                Projection.PreviewRecords.Add(MoveTemp(Record));
            }
            if (Total == 0)
            {
                auto Record = FCkUiRecordData{};
                Record.Key = TEXT("preview:empty");
                Record.Fields.Add(TEXT("ordinal"), TextField(FString{}));
                Record.Fields.Add(TEXT("name"), TextField(TEXT("(no plan)")));
                Record.Fields.Add(TEXT("hint"), TextField(FString{}));
                Projection.PreviewRecords.Add(MoveTemp(Record));
            }
            Projection.PreviewOverflowVisible = Total > PreviewCount;
            if (Projection.PreviewOverflowVisible)
            {
                Projection.PreviewOverflow = FString::Printf(TEXT("+ %d more - open the Window to see all"),
                    Total - PreviewCount);
            }
        }
    }

    auto Updates = TArray<FCkUiCollectionUpdate>{};
    Updates.Reserve(3);
    Updates.Add({.Collection = _AuthoredPlannerRecords, .Records = MoveTemp(Projection.PlannerRecords)});
    Updates.Add({.Collection = _AuthoredChainRecords, .Records = MoveTemp(Projection.ChainRecords)});
    Updates.Add({.Collection = _AuthoredPreviewRecords, .Records = MoveTemp(Projection.PreviewRecords)});
    const auto PublishResult = FCkUiCollection::TrySetRecordsBatch(MoveTemp(Updates));
    if (NOT PublishResult.Succeeded)
    {
        Clear_AuthoredProjection();
        _AuthoredLoadError = FString::Join(PublishResult.Errors, TEXT("\n"));
        return false;
    }

    _AuthoredPlannerCount = MoveTemp(Projection.PlannerCount);
    _AuthoredChainHeading = MoveTemp(Projection.ChainHeading);
    _AuthoredChainTags = MoveTemp(Projection.ChainTags);
    _AuthoredLeafHeading = MoveTemp(Projection.LeafHeading);
    _AuthoredLeafStatus = MoveTemp(Projection.LeafStatus);
    _AuthoredLeafCost = MoveTemp(Projection.LeafCost);
    _AuthoredLeafLength = MoveTemp(Projection.LeafLength);
    _AuthoredPreviewHeading = MoveTemp(Projection.PreviewHeading);
    _AuthoredPreviewOverflow = MoveTemp(Projection.PreviewOverflow);
    _AuthoredLeafStatusForeground = Projection.LeafStatusForeground;
    _AuthoredLeafStatusBackground = Projection.LeafStatusBackground;
    _AuthoredHasContent = Projection.HasContent;
    _AuthoredHasPlanner = Projection.HasPlanner;
    _AuthoredHasLeaf = Projection.HasLeaf;
    _AuthoredPreviewOverflowVisible = Projection.PreviewOverflowVisible;
    _AuthoredLoadError.Reset();
    return true;
}

auto
    SCkGoapDebugger_InspectorGateway::
    Clear_AuthoredProjection()
    -> void
{
    if (_AuthoredPlannerRecords.IsValid() && _AuthoredChainRecords.IsValid() && _AuthoredPreviewRecords.IsValid())
    {
        auto Updates = TArray<FCkUiCollectionUpdate>{};
        Updates.Reserve(3);
        Updates.Add({.Collection = _AuthoredPlannerRecords});
        Updates.Add({.Collection = _AuthoredChainRecords});
        Updates.Add({.Collection = _AuthoredPreviewRecords});
        FCkUiCollection::TrySetRecordsBatch(MoveTemp(Updates));
    }

    _AuthoredPlannerCount.Reset();
    _AuthoredChainHeading.Reset();
    _AuthoredChainTags.Reset();
    _AuthoredLeafHeading.Reset();
    _AuthoredLeafStatus.Reset();
    _AuthoredLeafCost.Reset();
    _AuthoredLeafLength.Reset();
    _AuthoredPreviewHeading.Reset();
    _AuthoredPreviewOverflow.Reset();
    _AuthoredLeafStatusForeground = FLinearColor::White;
    _AuthoredLeafStatusBackground = FLinearColor::Transparent;
    _AuthoredHasContent = false;
    _AuthoredHasPlanner = false;
    _AuthoredHasLeaf = false;
    _AuthoredPreviewOverflowVisible = false;
}

auto
    SCkGoapDebugger_InspectorGateway::
    Rebuild_Native(
        const FCkGoapDebugger_EntitySnapshot* InSnapshot)
    -> void
{
    if (NOT _ContentBox.IsValid()) { return; }
    _ContentBox->ClearChildren();
    if (InSnapshot == nullptr)
    {
        _ContentBox->AddSlot().AutoHeight()[Build_EmptyStub()];
        return;
    }

    _ContentBox->AddSlot().AutoHeight()[Build_Header(*InSnapshot)];
    _ContentBox->AddSlot().AutoHeight()[Build_ActionSetList(*InSnapshot)];
    if (const auto* DisplayPlanner = Pick_DisplayActionSet(*InSnapshot))
    {
        _ContentBox->AddSlot().AutoHeight()[Build_ActiveChain(*DisplayPlanner)];
        if (const auto* Leaf = ck_goap_debugger_gateway_internal::LeafActionInfo(*DisplayPlanner))
        {
            _ContentBox->AddSlot().AutoHeight()[Build_LeafAction(*Leaf)];
            _ContentBox->AddSlot().AutoHeight()[Build_PlanPreview(*Leaf)];
        }
    }
}

auto
    SCkGoapDebugger_InspectorGateway::
    Rebuild()
    -> void
{
    if (ck::Is_NOT_Valid(_Entity))
    {
        Present_Snapshot(nullptr);
        return;
    }

    auto* World = Resolve_World();
    if (World == nullptr)
    {
        Present_Snapshot(nullptr);
        return;
    }

    const auto Snapshot = FCkGoapDebugger_DataCollector::CollectFull(World, _Entity);
    Present_Snapshot(Snapshot.GetPtrOrNull());
}

// ====================================================================================================================
// SECTIONS
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Build_Header(
        const FCkGoapDebugger_EntitySnapshot& InSnapshot)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_gateway_internal;

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(CkStyle::Bg2())
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("GOAP")))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH3()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        ck_goap_debugger_axes::Make_Chip(
                            FText::FromString(TEXT("root")), ECk_Tone::Ok)
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Open in Goap Debugger >")))
                            .ToolTipText(FText::FromString(TEXT("Open the standalone Goap Debugger window focused on this entity")))
                            .OnClicked(this, &SCkGoapDebugger_InspectorGateway::OnClicked_OpenInGoapDebugger)
                    ]
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_ActionSetList(
        const FCkGoapDebugger_EntitySnapshot& InSnapshot)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_gateway_internal;

    auto Box = SNew(SVerticalBox);

    // Section header
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(FString::Printf(TEXT("PLANNERS - %d"), InSnapshot.ActionSets.Num())))
                .Font_Lambda([]() -> FSlateFontInfo
                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
        ];

    for (const auto& As : InSnapshot.ActionSets)
    {
        const auto Status   = HeadlineStatus(As);
        const auto DotColor = ColorForPlanStatus(Status, As.EnableToggle);

        auto TagStr = As.ActionSetTag.IsValid()
            ? As.ActionSetTag.ToString()
            : FString{};

        auto StatusStr = As.EnableToggle == ECk_EnableDisable::Disable
            ? FString(TEXT("Disabled"))
            : FString::Printf(TEXT("%s - %d active"),
                  *LabelForPlanStatus(Status),
                  As.ActiveChainHandles.Num());

        const auto NameColor = As.EnableToggle == ECk_EnableDisable::Disable
            ? CkStyle::TextMute()
            : CkStyle::Text();

        Box->AddSlot()
            .AutoHeight()
            .Padding(ck_goap_debugger_axes::Live_RowDensity(
                FMargin{0.0f, FCkGoapDebuggerStyle::Padding_XSmall}))
            [
                SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                        [
                            MakeStatusDot(DotColor)
                        ]

                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                                    [
                                        SNew(SCkDebug_SelectableLabel)
                                            .Text(FText::FromString(As.DebugName))
                                            .Font_Lambda([]() -> FSlateFontInfo
                                            { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                                            .ColorAndOpacity(FSlateColor(NameColor))
                                    ]
                                + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SCkDebug_SelectableLabel)
                                            .Text(FText::FromString(TagStr))
                                            .Font_Lambda([]() -> FSlateFontInfo
                                            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                                            .ColorAndOpacity(FSlateColor(CkStyle::OverlayOf(CkStyle::TextMute(), 0.75f)))
                                    ]
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(StatusStr))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                        ]
            ];
    }

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(CkStyle::Bg3())
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            Box
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_ActiveChain(
        const FCkGoapDebugger_ActionSetInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    auto HeaderText = FString::Printf(TEXT("ACTIVE CHAIN - %s"), *InPlanner.DebugName);

    // Compose the breadcrumb. Leaf gets highlighted in amber per the mockup.
    auto BreadcrumbBox = SNew(SHorizontalBox);
    auto TagChain      = FString{};

    const auto NumEntries = InPlanner.ActiveChainHandles.Num();
    for (auto i = 0; i < NumEntries; ++i)
    {
        const auto& Handle = InPlanner.ActiveChainHandles[i];
        const auto* Info = InPlanner.Catalog.FindByPredicate(
            [&Handle](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == Handle; });

        auto ClassName = (Info != nullptr) ? Info->ClassName : FString(TEXT("(?)"));
        const bool IsLeaf = (i == NumEntries - 1);

        if (i > 0)
        {
            BreadcrumbBox->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT(">")))
                        .Font_Lambda([]() -> FSlateFontInfo
                        { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeH3()); })
                        .ColorAndOpacity(FSlateColor(CkStyle::OverlayOf(CkStyle::TextMute(), 0.55f)))
                ];
        }

        const auto Color = IsLeaf
            ? CkStyle::Warn()
            : CkStyle::TextDim();

        BreadcrumbBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(ClassName))
                    .Font_Lambda([IsLeaf]() -> FSlateFontInfo
                    {
                        return ck::debug_axes::ScaledFont(
                            IsLeaf ? "Bold" : "Regular", CkStyle::FontSizeH3());
                    })
                    .ColorAndOpacity(FSlateColor(Color))
            ];

        if (Info != nullptr && Info->ActionTag.IsValid())
        {
            if (NOT TagChain.IsEmpty()) { TagChain.Append(TEXT(" > ")); }
            TagChain.Append(Info->ActionTag.ToString());
        }
    }

    if (NumEntries == 0)
    {
        BreadcrumbBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(TEXT("(no active chain)")))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeBody()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::OverlayOf(CkStyle::TextMute(), 0.75f)))
            ];
    }

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(CkStyle::Bg3())
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(HeaderText))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(CkStyle::Bg1())
                            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
                            [
                                BreadcrumbBox
                            ]
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(FString::Printf(TEXT("Action tag chain: %s"),
                                TagChain.IsEmpty() ? TEXT("(none)") : *TagChain)))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::OverlayOf(CkStyle::TextMute(), 0.75f)))
                    ]
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_LeafAction(
        const FCkGoapDebugger_ActionInfo& InLeaf)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_gateway_internal;

    auto MakeRow = [](const FString& InLabel, const FString& InValue, const FLinearColor& InValueColor) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(InLabel))
                        .Font_Lambda([]() -> FSlateFontInfo
                        { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                        .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(InValue))
                        .Font_Lambda([]() -> FSlateFontInfo
                        { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); })
                        .ColorAndOpacity(FSlateColor(InValueColor))
                ];
    };

    const auto StatusColor = ColorForPlanStatus(InLeaf.PlanStatus, ECk_EnableDisable::Enable);
    const auto CostStr     = FString::Printf(TEXT("%.0f"), InLeaf.PlanCost);
    const auto LenStr      = FString::Printf(TEXT("%d action%s"),
        InLeaf.PlanClassNames.Num(),
        InLeaf.PlanClassNames.Num() == 1 ? TEXT("") : TEXT("s"));

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(CkStyle::Bg3())
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(FString::Printf(TEXT("LEAF ACTION - %s"), *InLeaf.ClassName)))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(ck_goap_debugger_axes::Live_RowDensity(
                        FMargin{0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f}))
                    [
                        MakeRow(TEXT("Status"), LabelForPlanStatus(InLeaf.PlanStatus), StatusColor)
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(ck_goap_debugger_axes::Live_RowDensity(
                        FMargin{0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f}))
                    [
                        MakeRow(TEXT("Cost"), CostStr, CkStyle::Warn())
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(ck_goap_debugger_axes::Live_RowDensity(
                        FMargin{0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f}))
                    [
                        MakeRow(TEXT("Plan length"), LenStr, CkStyle::Text())
                    ]
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_PlanPreview(
        const FCkGoapDebugger_ActionInfo& InLeaf)
    -> TSharedRef<SWidget>
{
    const auto Total   = InLeaf.PlanClassNames.Num();
    const auto Preview = FMath::Min(3, Total);

    auto HeaderText = (Total <= Preview)
        ? FString::Printf(TEXT("PLAN (%d)"), Total)
        : FString::Printf(TEXT("PLAN (first %d of %d)"), Preview, Total);

    auto List = SNew(SVerticalBox);

    for (auto i = 0; i < Preview; ++i)
    {
        const auto& Name = InLeaf.PlanClassNames[i];

        List->AddSlot()
            .AutoHeight()
            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(FString::Printf(TEXT("%d."), i + 1)))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::Info()))
                        ]
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(Name))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeBody()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                        ]
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            // We don't have per-plan-entry cost in PlanClassNames; surface a
                            // single "(leaf)" hint since plan entries are always leaves of
                            // the active chain.
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(TEXT("(leaf)")))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                        ]
            ];
    }

    if (Total == 0)
    {
        List->AddSlot()
            .AutoHeight()
            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(TEXT("(no plan)")))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeBody()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::OverlayOf(CkStyle::TextMute(), 0.75f)))
            ];
    }

    if (Total > Preview)
    {
        List->AddSlot()
            .AutoHeight()
            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(FString::Printf(TEXT("+ %d more - open the Window to see all"),
                        Total - Preview)))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeMicro()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            ];
    }

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(CkStyle::Bg3())
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(HeaderText))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        List
                    ]
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_EmptyStub()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(CkStyle::Bg3())
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT("(no Goap root on selected entity)")))
                .Font_Lambda([]() -> FSlateFontInfo
                { return ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeBody()); })
                .ColorAndOpacity(FSlateColor(CkStyle::OverlayOf(CkStyle::TextMute(), 0.75f)))
        ];
}

// ====================================================================================================================
// HELPERS
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Resolve_World() const
    -> UWorld*
{
    auto FoundWorld = static_cast<UWorld*>(nullptr);

    if (GEngine == nullptr) { return nullptr; }

    for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
    {
        if (It->WorldType == EWorldType::PIE && ck::IsValid(It->World()) && It->World()->HasBegunPlay())
        {
            FoundWorld = It->World();
            break;
        }
    }

#if WITH_EDITOR
    if (FoundWorld == nullptr && GEditor)
    {
        auto& EditorCtx = GEditor->GetEditorWorldContext();
        if (ck::IsValid(EditorCtx.World()))
        { FoundWorld = EditorCtx.World(); }
    }
#endif

    return FoundWorld;
}

auto
    SCkGoapDebugger_InspectorGateway::
    Pick_DisplayActionSet(
        const FCkGoapDebugger_EntitySnapshot& InSnapshot) const
    -> const FCkGoapDebugger_ActionSetInfo*
{
    using namespace ck_goap_debugger_gateway_internal;

    if (InSnapshot.ActionSets.Num() == 0) { return nullptr; }

    // Priority: enabled + PlanFound > enabled + Planning > enabled + has chain > first.
    const FCkGoapDebugger_ActionSetInfo* Best = nullptr;
    auto BestRank = -1;

    for (const auto& As : InSnapshot.ActionSets)
    {
        if (As.EnableToggle == ECk_EnableDisable::Disable) { continue; }

        const auto Status = HeadlineStatus(As);
        auto Rank = 0;
        if (Status == ECk_GoapPlanStatus::PlanFound) { Rank = 3; }
        else if (Status == ECk_GoapPlanStatus::Planning) { Rank = 2; }
        else if (As.ActiveChainHandles.Num() > 0) { Rank = 1; }

        if (Rank > BestRank)
        {
            Best     = &As;
            BestRank = Rank;
        }
    }

    if (Best != nullptr) { return Best; }
    return &InSnapshot.ActionSets[0];
}

auto
    SCkGoapDebugger_InspectorGateway::
    OnClicked_OpenInGoapDebugger()
    -> FReply
{
    SCkGoapDebuggerWindow::OpenForEntity(_Entity);
    return FReply::Handled();
}

// ====================================================================================================================
