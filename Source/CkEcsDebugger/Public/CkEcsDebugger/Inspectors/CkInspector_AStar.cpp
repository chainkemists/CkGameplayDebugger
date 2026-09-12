#include "CkInspector_AStar.h"

#include "CkAStar/CkAStar_Fragment.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_AStar)

namespace ck_inspector_astar
{
    auto HasSearch(const FCk_Handle& InEntity) -> bool
    {
        return ck::IsValid(InEntity) && InEntity.Has<ck::FFragment_AStar_Debug>();
    }

    auto HasParams(const FCk_Handle& InEntity) -> bool
    {
        return ck::IsValid(InEntity) && InEntity.Has<ck::FFragment_AStar_Params>();
    }

    auto HasAny(const FCk_Handle& InEntity) -> bool
    {
        return HasSearch(InEntity) || HasParams(InEntity);
    }

    // CostThresholdReached is a bounded stop, not a failure: the search exhausted the caller's budget.
    auto GetSearchStatusTone(const ECk_AStarSearchStatus InStatus) -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_AStarSearchStatus::InProgress:           return ECk_Tone::Info;
            case ECk_AStarSearchStatus::Complete:             return ECk_Tone::Ok;
            case ECk_AStarSearchStatus::Failed:               return ECk_Tone::Err;
            case ECk_AStarSearchStatus::CostThresholdReached: return ECk_Tone::Warn;
            default:                                          return ECk_Tone::Neutral;
        }
    }

    auto GetBudgetTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        if (NOT HasSearch(InEntity)) { return ECk_Tone::Info; }
        const float Fraction = InEntity.Get<ck::FFragment_AStar_Debug>().Get_BudgetUsagePercent() / 100.0f;
        if (Fraction >= 1.0f) { return ECk_Tone::Err; }
        if (Fraction >= 0.8f) { return ECk_Tone::Warn; }
        return ECk_Tone::Info;
    }

    auto BuildStatusText(const FCk_Handle& InEntity) -> FString
    {
        if (NOT HasSearch(InEntity)) { return TEXT("--"); }
        return ck::Format_UE(TEXT("{}"), InEntity.Get<ck::FFragment_AStar_Debug>().Get_SearchStatus());
    }

    auto BuildOpenSetText(const FCk_Handle& InEntity) -> FString
    {
        return HasSearch(InEntity)
            ? LexToString(InEntity.Get<ck::FFragment_AStar_Debug>().Get_OpenSetSize()) : TEXT("--");
    }

    auto BuildClosedSetText(const FCk_Handle& InEntity) -> FString
    {
        return HasSearch(InEntity)
            ? LexToString(InEntity.Get<ck::FFragment_AStar_Debug>().Get_ClosedSetSize()) : TEXT("--");
    }

    auto BuildIterationsText(const FCk_Handle& InEntity) -> FString
    {
        return HasSearch(InEntity)
            ? LexToString(InEntity.Get<ck::FFragment_AStar_Debug>().Get_IterationsThisFrame()) : TEXT("--");
    }

    auto BuildTimeText(const FCk_Handle& InEntity) -> FString
    {
        return HasSearch(InEntity)
            ? LexToString(InEntity.Get<ck::FFragment_AStar_Debug>().Get_TimeThisFrameMicroseconds()) : TEXT("--");
    }

    auto BuildBudgetUsedText(const FCk_Handle& InEntity) -> FString
    {
        if (NOT HasSearch(InEntity)) { return TEXT("--"); }
        return FString::Printf(TEXT("%.1f%%"),
            InEntity.Get<ck::FFragment_AStar_Debug>().Get_BudgetUsagePercent());
    }

    auto BuildBudgetFraction(const FCk_Handle& InEntity) -> float
    {
        return HasSearch(InEntity)
            ? InEntity.Get<ck::FFragment_AStar_Debug>().Get_BudgetUsagePercent() / 100.0f : 0.0f;
    }

    auto BuildBudgetText(const FCk_Handle& InEntity) -> FString
    {
        if (NOT HasParams(InEntity)) { return TEXT("--"); }
        const int64 BudgetUs = InEntity.Get<ck::FFragment_AStar_Params>().Get_BudgetMicroseconds();
        return BudgetUs == 0 ? TEXT("0 (unbounded)") : LexToString(BudgetUs);
    }

    auto BuildMaxIterationsText(const FCk_Handle& InEntity) -> FString
    {
        if (NOT HasParams(InEntity)) { return TEXT("--"); }
        const int32 MaxIterations = InEntity.Get<ck::FFragment_AStar_Params>().Get_MaxIterationsPerTick();
        return MaxIterations == 0 ? TEXT("0 (unbounded)") : LexToString(MaxIterations);
    }

    auto BuildCostThresholdText(const FCk_Handle& InEntity) -> FString
    {
        if (NOT HasParams(InEntity)) { return TEXT("--"); }
        const float CostThreshold = InEntity.Get<ck::FFragment_AStar_Params>().Get_CostThreshold();
        return CostThreshold > 0.0f ? FString::Printf(TEXT("%.3f"), CostThreshold) : TEXT("0 (disabled)");
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

auto SCkInspector_AStarAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _StatusDiffMarked = InArgs._StatusDiffMarked;
    _OpenSetDiffMarked = InArgs._OpenSetDiffMarked;
    _ClosedSetDiffMarked = InArgs._ClosedSetDiffMarked;
    _IterationsDiffMarked = InArgs._IterationsDiffMarked;
    _TimeDiffMarked = InArgs._TimeDiffMarked;
    _BudgetUsedDiffMarked = InArgs._BudgetUsedDiffMarked;
    _BudgetDiffMarked = InArgs._BudgetDiffMarked;
    _MaxIterationsDiffMarked = InArgs._MaxIterationsDiffMarked;
    _CostThresholdDiffMarked = InArgs._CostThresholdDiffMarked;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_AuthoredView())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_AStarAuthored::~SCkInspector_AStarAuthored()
{
    Release();
}

auto SCkInspector_AStarAuthored::Get_HasSearch() const -> bool
{
    return _Active && ck_inspector_astar::HasSearch(_Entity);
}

auto SCkInspector_AStarAuthored::Get_HasParams() const -> bool
{
    return _Active && ck_inspector_astar::HasParams(_Entity);
}

auto SCkInspector_AStarAuthored::Get_StatusText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildStatusText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Get_OpenSetText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildOpenSetText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Get_ClosedSetText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildClosedSetText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Get_IterationsText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildIterationsText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Get_TimeText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildTimeText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Get_BudgetUsedText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildBudgetUsedText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Get_BudgetFraction() const -> float
{
    return _Active ? ck_inspector_astar::BuildBudgetFraction(_Entity) : 0.0f;
}

auto SCkInspector_AStarAuthored::Get_StatusForeground() const -> FLinearColor
{
    if (NOT Get_HasSearch()) { return CkStyle::GetToneColor(ECk_Tone::Neutral); }
    return CkStyle::GetToneColor(ck_inspector_astar::GetSearchStatusTone(
        _Entity.Get<ck::FFragment_AStar_Debug>().Get_SearchStatus()));
}

auto SCkInspector_AStarAuthored::Get_StatusBackground() const -> FLinearColor
{
    if (NOT Get_HasSearch()) { return CkStyle::GetToneDimColor(ECk_Tone::Neutral); }
    return CkStyle::GetToneDimColor(ck_inspector_astar::GetSearchStatusTone(
        _Entity.Get<ck::FFragment_AStar_Debug>().Get_SearchStatus()));
}

auto SCkInspector_AStarAuthored::Get_BudgetFill() const -> FLinearColor
{
    return CkStyle::GetToneColor(ck_inspector_astar::GetBudgetTone(_Entity));
}

auto SCkInspector_AStarAuthored::Get_BudgetText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildBudgetText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Get_MaxIterationsText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildMaxIterationsText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Get_CostThresholdText() const -> FString
{
    return _Active ? ck_inspector_astar::BuildCostThresholdText(_Entity) : FString{};
}

auto SCkInspector_AStarAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_AStarAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    auto BindText = [&Data, WeakWidget](const FString& InName,
        FString (SCkInspector_AStarAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_AStarAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("astar-status"), &SCkInspector_AStarAuthored::Get_StatusText);
    BindText(TEXT("astar-open-set"), &SCkInspector_AStarAuthored::Get_OpenSetText);
    BindText(TEXT("astar-closed-set"), &SCkInspector_AStarAuthored::Get_ClosedSetText);
    BindText(TEXT("astar-iterations"), &SCkInspector_AStarAuthored::Get_IterationsText);
    BindText(TEXT("astar-time"), &SCkInspector_AStarAuthored::Get_TimeText);
    BindText(TEXT("astar-budget-used"), &SCkInspector_AStarAuthored::Get_BudgetUsedText);
    BindText(TEXT("astar-budget"), &SCkInspector_AStarAuthored::Get_BudgetText);
    BindText(TEXT("astar-max-iterations"), &SCkInspector_AStarAuthored::Get_MaxIterationsText);
    BindText(TEXT("astar-cost-threshold"), &SCkInspector_AStarAuthored::Get_CostThresholdText);
    Data.Number.Add(TEXT("astar-budget-fraction"), TAttribute<float>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_AStarAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() ? Widget->Get_BudgetFraction() : 0.0f;
    }));
    Data.Visibility.Add(TEXT("astar-search-visible"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_AStarAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_HasSearch();
    }));
    Data.Visibility.Add(TEXT("astar-params-visible"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_AStarAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_HasParams();
    }));
    auto BindColor = [&Data, WeakWidget](const FString& InName,
        FLinearColor (SCkInspector_AStarAuthored::* InGetter)() const)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_AStarAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() ? (Widget.Get()->*InGetter)() : FLinearColor::Transparent;
        }));
    };
    BindColor(TEXT("astar-status-foreground"), &SCkInspector_AStarAuthored::Get_StatusForeground);
    BindColor(TEXT("astar-status-background"), &SCkInspector_AStarAuthored::Get_StatusBackground);
    BindColor(TEXT("astar-budget-fill"), &SCkInspector_AStarAuthored::Get_BudgetFill);
    auto BindDiffColor = [&Data, WeakWidget](const FString& InName,
        const bool SCkInspector_AStarAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_AStarAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_astar::DiffColor(Widget.Get()->*InMember) : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("astar-status-diff-color"), &SCkInspector_AStarAuthored::_StatusDiffMarked);
    BindDiffColor(TEXT("astar-open-set-diff-color"), &SCkInspector_AStarAuthored::_OpenSetDiffMarked);
    BindDiffColor(TEXT("astar-closed-set-diff-color"), &SCkInspector_AStarAuthored::_ClosedSetDiffMarked);
    BindDiffColor(TEXT("astar-iterations-diff-color"), &SCkInspector_AStarAuthored::_IterationsDiffMarked);
    BindDiffColor(TEXT("astar-time-diff-color"), &SCkInspector_AStarAuthored::_TimeDiffMarked);
    BindDiffColor(TEXT("astar-budget-used-diff-color"), &SCkInspector_AStarAuthored::_BudgetUsedDiffMarked);
    BindDiffColor(TEXT("astar-budget-diff-color"), &SCkInspector_AStarAuthored::_BudgetDiffMarked);
    BindDiffColor(TEXT("astar-max-iterations-diff-color"), &SCkInspector_AStarAuthored::_MaxIterationsDiffMarked);
    BindDiffColor(TEXT("astar-cost-threshold-diff-color"), &SCkInspector_AStarAuthored::_CostThresholdDiffMarked);

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorAStar.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorAStar.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_AStarAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_AStarAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _View.Reset();
    _Mounted = false;
}

FCkInspector_AStar::~FCkInspector_AStar()
{
    OnDeactivated();
}

auto FCkInspector_AStar::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("A*"));
}

auto FCkInspector_AStar::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck_inspector_astar::HasAny(Entity);
}

auto FCkInspector_AStar::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    const FCk_Handle CapturedEntity = Entity;
    if (ck_inspector_astar::HasSearch(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Search")));
        Builder.AddStatusPillRow(FText::FromString(TEXT("Status:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            { return FText::FromString(ck_inspector_astar::BuildStatusText(CapturedEntity)); }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                return ck_inspector_astar::HasSearch(CapturedEntity)
                    ? ck_inspector_astar::GetSearchStatusTone(
                        CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_SearchStatus())
                    : ECk_Tone::Neutral;
            }));
        Builder.AddRow(FText::FromString(TEXT("Open Set:")), [CapturedEntity](const FCk_Handle&)
            { return FText::FromString(ck_inspector_astar::BuildOpenSetText(CapturedEntity)); }, CkStyle::Value_Numeric());
        Builder.AddRow(FText::FromString(TEXT("Closed Set:")), [CapturedEntity](const FCk_Handle&)
            { return FText::FromString(ck_inspector_astar::BuildClosedSetText(CapturedEntity)); }, CkStyle::Value_Numeric());
        Builder.AddRow(FText::FromString(TEXT("Iterations (frame):")), [CapturedEntity](const FCk_Handle&)
            { return FText::FromString(ck_inspector_astar::BuildIterationsText(CapturedEntity)); }, CkStyle::Value_Numeric());
        Builder.AddRow(FText::FromString(TEXT("Time (frame, us):")), [CapturedEntity](const FCk_Handle&)
            { return FText::FromString(ck_inspector_astar::BuildTimeText(CapturedEntity)); }, CkStyle::Value_Numeric());
        Builder.AddMeterRow(FText::FromString(TEXT("Budget Used:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            { return ck_inspector_astar::BuildBudgetFraction(CapturedEntity); }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            { return ck_inspector_astar::GetBudgetTone(CapturedEntity); }),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            { return FText::FromString(ck_inspector_astar::BuildBudgetUsedText(CapturedEntity)); }));
    }
    if (ck_inspector_astar::HasParams(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Params")));
        Builder.AddRow(FText::FromString(TEXT("Budget (us):")), [CapturedEntity](const FCk_Handle&)
            { return FText::FromString(ck_inspector_astar::BuildBudgetText(CapturedEntity)); }, CkStyle::Value_Numeric());
        Builder.AddRow(FText::FromString(TEXT("Max Iterations:")), [CapturedEntity](const FCk_Handle&)
            { return FText::FromString(ck_inspector_astar::BuildMaxIterationsText(CapturedEntity)); }, CkStyle::Value_Numeric());
        Builder.AddRow(FText::FromString(TEXT("Cost Threshold:")), [CapturedEntity](const FCk_Handle&)
            { return FText::FromString(ck_inspector_astar::BuildCostThresholdText(CapturedEntity)); }, CkStyle::Value_Numeric());
    }
    return Builder.Build(Entity);
}

auto FCkInspector_AStar::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    const TSharedRef<SCkInspector_AStarAuthored> Authored = SNew(SCkInspector_AStarAuthored)
        .Entity(Entity)
        .StatusDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Status:")))
        .OpenSetDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Open Set:")))
        .ClosedSetDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Closed Set:")))
        .IterationsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Iterations (frame):")))
        .TimeDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Time (frame, us):")))
        .BudgetUsedDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Budget Used:")))
        .BudgetDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Budget (us):")))
        .MaxIterationsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Max Iterations:")))
        .CostThresholdDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Cost Threshold:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_AStar::Tick(const FCk_Handle&, const float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_AStarAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_AStar::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_AStarAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_AStarAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}
