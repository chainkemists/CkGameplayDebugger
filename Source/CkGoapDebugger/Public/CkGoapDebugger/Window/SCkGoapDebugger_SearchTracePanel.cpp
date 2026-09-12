#include "CkGoapDebugger/Window/SCkGoapDebugger_SearchTracePanel.h"

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_goap_debugger_search_trace
{
    auto TextField(const FString& InValue) -> FCkUiFieldValue
    {
        return {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)};
    }

    auto ColorField(const FLinearColor& InValue) -> FCkUiFieldValue
    {
        return {.Kind = ECkUiFieldKind::Color, .Color = InValue};
    }

    auto Schema() -> TArray<FCkUiFieldSchema>
    {
        return {{TEXT("trace-index"), ECkUiFieldKind::Text},
                {TEXT("trace-conditions"), ECkUiFieldKind::Text},
                {TEXT("trace-via"), ECkUiFieldKind::Text},
                {TEXT("trace-heuristic"), ECkUiFieldKind::Text},
                {TEXT("trace-row-color"), ECkUiFieldKind::Color},
                {TEXT("trace-satisfaction"), ECkUiFieldKind::Text},
                {TEXT("trace-satisfaction-foreground"), ECkUiFieldKind::Color},
                {TEXT("trace-satisfaction-background"), ECkUiFieldKind::Color}};
    }

    auto Tokens() -> FCkUiView::FTokens
    {
        return {{TEXT("--space-xs"), FString::SanitizeFloat(CkStyle::SpaceXS)},
                {TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
                {TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM)},
                {TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL)},
                {TEXT("--goap-search-trace-body-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeBody()))},
                {TEXT("--goap-search-trace-small-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeSmall()))},
                {TEXT("--goap-search-trace-micro-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeMicro()))},
                {TEXT("--goap-search-trace-text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex()},
                {TEXT("--goap-search-trace-text-dim"), TEXT("#") + CkStyle::TextDim().ToFColorSRGB().ToHex()},
                {TEXT("--goap-search-trace-text-mute"), TEXT("#") + CkStyle::TextMute().ToFColorSRGB().ToHex()}};
    }

    auto LeafOfTag(const FGameplayTag& InTag) -> FString
    {
        auto Full = InTag.ToString();
        if (auto Idx = int32{INDEX_NONE}; Full.FindLastChar(TEXT('.'), Idx))
        { return Full.RightChop(Idx + 1); }
        return Full;
    }

    auto LeafOfClass(const TSubclassOf<UCk_GoapAction_EntityScript>& InClass) -> FString
    {
        if (InClass == nullptr) { return FString(TEXT("(goal seed)")); }
        return InClass->GetName();
    }
}

// ====================================================================================================================
// CONSTRUCT / REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_SearchTracePanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    using namespace ck_goap_debugger_search_trace;

    _ViewModel = InArgs._ViewModel;

    _NativeContent =
        SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
            .Padding(FMargin(CkStyle::SpaceL))
    [
        SNew(SScrollBox)
            .Orientation(Orient_Vertical)

            + SScrollBox::Slot()
            [
                SAssignNew(_Body, SVerticalBox)
            ]
    ];
    ChildSlot[SAssignNew(_ContentHost, SBox)[_NativeContent.ToSharedRef()]];

    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(Schema(), _AuthoredCollection);
    _AuthoredProjectionReady = CollectionResult.Succeeded;
    if (!CollectionResult.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(CollectionResult.Errors, TEXT("\n"));
    }
    TryActivateAuthoredView();

    RefreshFromViewModel();
}

auto
    SCkGoapDebugger_SearchTracePanel::
    RefreshFromViewModel()
    -> void
{
    using namespace ck_goap_debugger_search_trace;

    if (NOT _ViewModel.IsValid() || NOT _Body.IsValid()) { return; }

    if (_AuthoredView.IsValid())
    {
        _AuthoredView->PollFiles(Tokens());
        if (!_AuthoredView->GetLastResult().Succeeded)
        {
            // PollFiles retains the last admitted document on failure. Keep that mounted surface and
            // collection intact so a corrected authored file can be accepted on a later poll.
            _AuthoredLoadFailure = FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n"));
        }
        else
        {
            _AuthoredLoadFailure.Reset();
        }
    }

    const auto* Planner = _ViewModel->GetSelectedPlannerInfo();

    auto NewHash = uint32{0};
    if (Planner != nullptr)
    {
        NewHash = HashCombine(NewHash, GetTypeHash(Planner->PlannerHandle));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->PlanAttemptCount));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->SearchDebug.Num()));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->SearchStats.Get_Iterations()));
    }

    if (NewHash == _LastHash) { return; }
    _LastHash = NewHash;

    _Body->ClearChildren();

    if (Planner == nullptr || Planner->SearchDebug.Num() == 0)
    {
        _Body->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, CkStyle::SpaceXL))
            [
                SNew(STextBlock)
                    .Text(FText::FromString(Planner == nullptr
                        ? TEXT("Select a Planner to see its last search trace.")
                        : TEXT("No search trace yet — this Planner hasn't run a search this session.")))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    .Justification(ETextJustify::Center)
            ];
        PublishAuthoredProjection();
        return;
    }

    // ---- Explainer + stats strip ----------------------------------------------
    _Body->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT(
                    "Regressive A* — the search walks BACKWARD from the goal. Each row is a constraint set still "
                    "unsatisfied at that state; an action trades it for the set its preconditions demand. Green rows "
                    "are already satisfied by the world state — the search terminates there.")))
                .Font_Lambda([]() -> FSlateFontInfo
                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
        ];

    const auto& Stats = Planner->SearchStats;
    _Body->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceM))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(FString::Printf(
                    TEXT("iterations %d · state pool %d · elapsed %lld µs · plan length %d · cost %.1f · seeded from flattened WS snapshot"),
                    Stats.Get_Iterations(),
                    Stats.Get_StatePoolSize(),
                    Stats.Get_ElapsedMicroseconds(),
                    Stats.Get_PlanLength(),
                    Stats.Get_PlanCost())))
                .Font_Lambda([]() -> FSlateFontInfo
                { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
        ];

    // ---- Rows ------------------------------------------------------------------
    for (auto Index = 0; Index < Planner->SearchDebug.Num(); ++Index)
    {
        _Body->AddSlot()
            .AutoHeight()
            .Padding(ck_goap_debugger_axes::Live_RowDensity(FMargin{0.0f, 0.0f, 0.0f, 2.0f}))
            [
                DoBuildRow(Planner->SearchDebug[Index], Index)
            ];
    }

    _Body->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, CkStyle::SpaceS, 0.0f, 0.0f))
        [
            SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(
                    TEXT("%d explored constraint sets retained from the last search (pool holds every state the search touched)."),
                    Planner->SearchDebug.Num())))
                .Font_Lambda([]() -> FSlateFontInfo
                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
        ];

    PublishAuthoredProjection();

}

auto SCkGoapDebugger_SearchTracePanel::PublishAuthoredProjection() -> void
{
    using namespace ck_goap_debugger_search_trace;

    if (!_AuthoredCollection.IsValid()) { return; }

    TArray<FCkUiRecordData> Records;
    const auto* Planner = _ViewModel.IsValid() ? _ViewModel->GetSelectedPlannerInfo() : nullptr;
    if (Planner != nullptr)
    {
        Records.Reserve(Planner->SearchDebug.Num());
        for (int32 Index = 0; Index < Planner->SearchDebug.Num(); ++Index)
        {
            const FCk_Goap_SearchDebugRow& Row = Planner->SearchDebug[Index];
            FString Conditions;
            for (const FCk_GoapWS_Condition_Authored& Condition : Row.Get_Conditions())
            {
                if (!Conditions.IsEmpty()) { Conditions += TEXT(", "); }
                Conditions += Condition.Get_Key().ToString();
                if (!Condition.Get_Value()) { Conditions += TEXT(" = false"); }
            }
            if (Conditions.IsEmpty()) { Conditions = TEXT("(empty set)"); }

            FCkUiRecordData Record;
            // Selection identity plus discovery index is stable for the retained last-search snapshot.
            Record.Key = FString::Printf(TEXT("goap-search-trace:%d:%d:%d"),
                static_cast<int32>(Planner->PlannerHandle.Get_Entity().Get_EntityNumber()),
                static_cast<int32>(Planner->PlannerHandle.Get_Entity().Get_VersionNumber()), Index);
            Record.Fields.Add(TEXT("trace-index"), TextField(FString::Printf(TEXT("%02d"), Index)));
            Record.Fields.Add(TEXT("trace-conditions"), TextField(MoveTemp(Conditions)));
            Record.Fields.Add(TEXT("trace-via"), TextField(FString::Printf(TEXT("via %s"), *LeafOfClass(Row.Get_ViaActionClass()))));
            Record.Fields.Add(TEXT("trace-heuristic"), TextField(FString::Printf(TEXT("h=%d"), Row.Get_UnsatisfiedCount())));
            Record.Fields.Add(TEXT("trace-row-color"), ColorField(Row.Get_SatisfiedByWorldState() ? CkStyle::Ok() : CkStyle::TextMute()));
            Record.Fields.Add(TEXT("trace-satisfaction"), TextField(Row.Get_SatisfiedByWorldState() ? TEXT("SATISFIED") : TEXT("OPEN")));
            Record.Fields.Add(TEXT("trace-satisfaction-foreground"), ColorField(Row.Get_SatisfiedByWorldState() ? CkStyle::Ok() : CkStyle::TextMute()));
            Record.Fields.Add(TEXT("trace-satisfaction-background"), ColorField(Row.Get_SatisfiedByWorldState() ? CkStyle::OkDim() : CkStyle::Bg2()));
            Records.Add(MoveTemp(Record));
        }
    }

    const FCkUiLoadResult Result = _AuthoredCollection->TrySetRecords(MoveTemp(Records));
    if (!Result.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(Result.Errors, TEXT("\n"));
        if (!_AuthoredView.IsValid()) { ActivateNativeFallback(); }
    }
}

auto SCkGoapDebugger_SearchTracePanel::TryActivateAuthoredView() -> void
{
    using namespace ck_goap_debugger_search_trace;

    if (_AuthoredView.IsValid() || !_ContentHost.IsValid() || !_AuthoredProjectionReady || !_AuthoredCollection.IsValid()) { return; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (!RegistryResult.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(RegistryResult.Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid())
    {
        _AuthoredLoadFailure = TEXT("CkDebugger plugin is unavailable.");
        ActivateNativeFallback();
        return;
    }

    const TWeakPtr<SCkGoapDebugger_SearchTracePanel> WeakPanel{SharedThis(this)};
    FCkUiView::FDataBindings Data;
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPanel]() { return WeakPanel.IsValid(); });
    Data.Text.Add(TEXT("goap-search-trace-empty"), TAttribute<FText>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkGoapDebugger_SearchTracePanel> Panel = WeakPanel.Pin();
        const auto* Planner = Panel.IsValid() && Panel->_ViewModel.IsValid() ? Panel->_ViewModel->GetSelectedPlannerInfo() : nullptr;
        return FText::FromString(Planner == nullptr
            ? TEXT("Select a Planner to see its last search trace.")
            : (Planner->SearchDebug.IsEmpty() ? TEXT("No search trace yet — this Planner hasn't run a search this session.") : TEXT("")));
    }));
    Data.Visibility.Add(TEXT("goap-search-trace-has-records"), TAttribute<bool>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkGoapDebugger_SearchTracePanel> Panel = WeakPanel.Pin();
        const auto* Planner = Panel.IsValid() && Panel->_ViewModel.IsValid() ? Panel->_ViewModel->GetSelectedPlannerInfo() : nullptr;
        return Planner != nullptr && !Planner->SearchDebug.IsEmpty();
    }));
    Data.Text.Add(TEXT("goap-search-trace-stats"), TAttribute<FText>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkGoapDebugger_SearchTracePanel> Panel = WeakPanel.Pin();
        const auto* Planner = Panel.IsValid() && Panel->_ViewModel.IsValid() ? Panel->_ViewModel->GetSelectedPlannerInfo() : nullptr;
        if (Planner == nullptr) { return FText::GetEmpty(); }
        const auto& Stats = Planner->SearchStats;
        return FText::FromString(FString::Printf(TEXT("iterations %d · state pool %d · elapsed %lld µs · plan length %d · cost %.1f · seeded from flattened WS snapshot"), Stats.Get_Iterations(), Stats.Get_StatePoolSize(), Stats.Get_ElapsedMicroseconds(), Stats.Get_PlanLength(), Stats.Get_PlanCost()));
    }));
    Data.Text.Add(TEXT("goap-search-trace-footer"), TAttribute<FText>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkGoapDebugger_SearchTracePanel> Panel = WeakPanel.Pin();
        const auto* Planner = Panel.IsValid() && Panel->_ViewModel.IsValid() ? Panel->_ViewModel->GetSelectedPlannerInfo() : nullptr;
        return Planner == nullptr ? FText::GetEmpty() : FText::FromString(FString::Printf(TEXT("%d explored constraint sets retained from the last search (pool holds every state the search touched)."), Planner->SearchDebug.Num()));
    }));
    Data.Collections.Add(TEXT("goap-search-trace-records"), _AuthoredCollection);
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create({}, {}, Tokens(), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = Candidate->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Directory, TEXT("GoapDebuggerSearchTrace.ui.html")), FPaths::Combine(Directory, TEXT("GoapDebuggerSearchTrace.ui.css")));
    Candidate->PollFiles();
    if (!Candidate->GetLastResult().Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }
    _AuthoredLoadFailure.Reset();
    _AuthoredView = Candidate;
    _ContentHost->SetContent(Main);
}

auto SCkGoapDebugger_SearchTracePanel::ActivateNativeFallback() -> void
{
    _AuthoredView.Reset();
    if (_ContentHost.IsValid() && _NativeContent.IsValid()) { _ContentHost->SetContent(_NativeContent.ToSharedRef()); }
}

// ====================================================================================================================
// BUILD — ROW
// ====================================================================================================================

auto
    SCkGoapDebugger_SearchTracePanel::
    DoBuildRow(
        const FCk_Goap_SearchDebugRow& InRow,
        int32 InIndex)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_search_trace;

    const auto Satisfied = InRow.Get_SatisfiedByWorldState();

    auto Chips = SNew(SWrapBox)
        .UseAllottedSize(true)
        .InnerSlotPadding(FVector2D(CkStyle::SpaceXS, CkStyle::SpaceXS));

    if (InRow.Get_Conditions().Num() == 0)
    {
        Chips->AddSlot()
        [
            SNew(SCkDebug_Chip)
                .Text(FText::FromString(TEXT("(empty set)")))
                .Kind(ECkDebug_ChipKind::Satisfied)
        ];
    }

    for (const auto& Cond : InRow.Get_Conditions())
    {
        auto Label = LeafOfTag(Cond.Get_Key());
        if (NOT Cond.Get_Value()) { Label += TEXT(" = false"); }

        Chips->AddSlot()
        [
            SNew(SCkDebug_Chip)
                .Text(FText::FromString(Label))
                .ToolTipText(FText::FromString(Cond.Get_Key().ToString()))
                .Kind(Satisfied ? ECkDebug_ChipKind::Satisfied : ECkDebug_ChipKind::Neutral)
        ];
    }

    return SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(FSlateColor(Satisfied ? CkStyle::OkDim() : CkStyle::Bg2()))
        .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, 0.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("%02d"), InIndex)))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        Chips
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(FString::Printf(TEXT("via %s"), *LeafOfClass(InRow.Get_ViaActionClass()))))
                            .ToolTipText(FText::FromString(TEXT("The action whose preconditions introduced this constraint set (regressive step).")))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("h=%d"), InRow.Get_UnsatisfiedCount())))
                            .ToolTipText(FText::FromString(TEXT("Heuristic: unsatisfied-condition count at this state.")))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(Satisfied ? CkStyle::Ok() : CkStyle::TextMute()))
                    ]
        ];
}

// ====================================================================================================================
