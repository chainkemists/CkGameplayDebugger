#include "CkInspector_Compass.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkCompass/CkCompass_Fragment.h"
#include "CkCompass/CkCompass_Utils.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Compass)

namespace ck_inspector_compass
{
    auto TryGetCompass(const FCk_Handle& InEntity, FCk_Handle_Compass& OutCompass) -> bool
    {
        OutCompass = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Compass_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        OutCompass = UCk_Utils_Compass_UE::Cast(MutableEntity);
        return ck::IsValid(OutCompass)
            && OutCompass.Has<ck::FFragment_Compass_Current>()
            && OutCompass.Has<ck::FFragment_Compass_Params>();
    }

    auto GetHeading(const FCk_Handle& InEntity) -> float
    {
        auto Compass = FCk_Handle_Compass{};
        return TryGetCompass(InEntity, Compass) ? UCk_Utils_Compass_UE::Get_Heading(Compass) : 0.0f;
    }

    auto GetHeadingText(const FCk_Handle& InEntity) -> FString
    {
        auto Compass = FCk_Handle_Compass{};
        if (NOT TryGetCompass(InEntity, Compass)) { return TEXT("--"); }
        const float Heading = UCk_Utils_Compass_UE::Get_Heading(Compass);
        return ck::Format_UE(TEXT("{:.1f}°  {}"), Heading, UCk_Utils_Compass_UE::Get_CardinalDirection(Heading));
    }

    auto GetManualHeadingText(const FCk_Handle& InEntity) -> FString
    {
        auto Compass = FCk_Handle_Compass{};
        return TryGetCompass(InEntity, Compass)
            ? ck::Format_UE(TEXT("{:.1f}"), UCk_Utils_Compass_UE::Get_Heading(Compass)) : TEXT("--");
    }

    auto GetSourceText(const FCk_Handle& InEntity) -> FString
    {
        auto Compass = FCk_Handle_Compass{};
        return TryGetCompass(InEntity, Compass)
            ? ck::Format_UE(TEXT("{}"), Compass.Get<ck::FFragment_Compass_Params>().Get_HeadingSource()) : TEXT("--");
    }

    auto GetObserver(const FCk_Handle& InEntity) -> FCk_Handle
    {
        auto Compass = FCk_Handle_Compass{};
        return TryGetCompass(InEntity, Compass) ? UCk_Utils_Compass_UE::Get_Observer(Compass) : FCk_Handle{};
    }

    auto GetArcText(const FCk_Handle& InEntity) -> FString
    {
        auto Compass = FCk_Handle_Compass{};
        return TryGetCompass(InEntity, Compass)
            ? ck::Format_UE(TEXT("{:.0f}°"), UCk_Utils_Compass_UE::Get_ArcDegrees(Compass)) : TEXT("--");
    }

    auto GetEntriesText(const FCk_Handle& InEntity) -> FString
    {
        auto Compass = FCk_Handle_Compass{};
        if (NOT TryGetCompass(InEntity, Compass)) { return TEXT("--"); }
        return ck::Format_UE(TEXT("{} / {}"), UCk_Utils_Compass_UE::Get_Entries(Compass).Num(),
            Compass.Get<ck::FFragment_Compass_Params>().Get_MaxEntries());
    }

    auto GetEntriesFraction(const FCk_Handle& InEntity) -> float
    {
        auto Compass = FCk_Handle_Compass{};
        if (NOT TryGetCompass(InEntity, Compass)) { return 0.0f; }
        const int32 MaxEntries = Compass.Get<ck::FFragment_Compass_Params>().Get_MaxEntries();
        return MaxEntries > 0
            ? static_cast<float>(UCk_Utils_Compass_UE::Get_Entries(Compass).Num()) / static_cast<float>(MaxEntries)
            : 0.0f;
    }

    auto GetFilterText(const FCk_Handle& InEntity) -> FString
    {
        auto Compass = FCk_Handle_Compass{};
        if (NOT TryGetCompass(InEntity, Compass)) { return TEXT("--"); }
        const FGameplayTagQuery& Filter = Compass.Get<ck::FFragment_Compass_Params>().Get_CategoryFilter();
        return Filter.IsEmpty() ? TEXT("(accepts all)") : Filter.GetDescription();
    }

    auto GetIntervalText(const FCk_Handle& InEntity) -> FString
    {
        auto Compass = FCk_Handle_Compass{};
        if (NOT TryGetCompass(InEntity, Compass)) { return TEXT("--"); }
        const FCk_Time Interval = Compass.Get<ck::FFragment_Compass_Params>().Get_UpdateInterval();
        return Interval <= FCk_Time::ZeroSecond()
            ? TEXT("0 (every frame)") : ck::Format_UE(TEXT("{:.2f}s"), Interval.Get_Seconds());
    }

    auto GetManualHeadingGate(const FCk_Handle& InEntity) -> FCk_DebugRequest_GateVerdict
    {
        auto Compass = FCk_Handle_Compass{};
        if (NOT TryGetCompass(InEntity, Compass))
        { return FCk_DebugRequest_GateVerdict{false, FText::FromString(TEXT("Compass is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::CosmeticOnly);
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// =====================================================================================================================

auto SCkInspector_CompassAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _ManualHeadingEditScope = MakeShared<FCkInspectorEditScope>(InArgs._EditGuard);
    _HeadingDiffMarked = InArgs._HeadingDiffMarked;
    _ManualHeadingDiffMarked = InArgs._ManualHeadingDiffMarked;
    _SourceDiffMarked = InArgs._SourceDiffMarked;
    _ObserverDiffMarked = InArgs._ObserverDiffMarked;
    _ArcDiffMarked = InArgs._ArcDiffMarked;
    _EntriesDiffMarked = InArgs._EntriesDiffMarked;
    _FilterDiffMarked = InArgs._FilterDiffMarked;
    _IntervalDiffMarked = InArgs._IntervalDiffMarked;

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

SCkInspector_CompassAuthored::~SCkInspector_CompassAuthored()
{
    Release();
}

auto SCkInspector_CompassAuthored::Get_IsAvailable() const -> bool
{
    auto Compass = FCk_Handle_Compass{};
    return _Active && ck_inspector_compass::TryGetCompass(_Entity, Compass);
}

auto SCkInspector_CompassAuthored::Get_HeadingText() const -> FString
{
    return _Active ? ck_inspector_compass::GetHeadingText(_Entity) : FString{};
}

auto SCkInspector_CompassAuthored::Get_ManualHeading() const -> float
{
    return _Active ? ck_inspector_compass::GetHeading(_Entity) : 0.0f;
}

auto SCkInspector_CompassAuthored::Get_ManualHeadingText() const -> FString
{
    return _Active ? ck_inspector_compass::GetManualHeadingText(_Entity) : FString{};
}

auto SCkInspector_CompassAuthored::Get_SourceText() const -> FString
{
    return _Active ? ck_inspector_compass::GetSourceText(_Entity) : FString{};
}

auto SCkInspector_CompassAuthored::Get_Observer() const -> FCk_Handle
{
    return _Active ? ck_inspector_compass::GetObserver(_Entity) : FCk_Handle{};
}

auto SCkInspector_CompassAuthored::Get_ArcText() const -> FString
{
    return _Active ? ck_inspector_compass::GetArcText(_Entity) : FString{};
}

auto SCkInspector_CompassAuthored::Get_EntriesText() const -> FString
{
    return _Active ? ck_inspector_compass::GetEntriesText(_Entity) : FString{};
}

auto SCkInspector_CompassAuthored::Get_EntriesFraction() const -> float
{
    return _Active ? ck_inspector_compass::GetEntriesFraction(_Entity) : 0.0f;
}

auto SCkInspector_CompassAuthored::Get_FilterText() const -> FString
{
    return _Active ? ck_inspector_compass::GetFilterText(_Entity) : FString{};
}

auto SCkInspector_CompassAuthored::Get_IntervalText() const -> FString
{
    return _Active ? ck_inspector_compass::GetIntervalText(_Entity) : FString{};
}

auto SCkInspector_CompassAuthored::Get_CanEditManualHeading() const -> bool
{
    return _Active && ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection())
        && ck_inspector_compass::GetManualHeadingGate(_Entity).IsEnabled;
}

auto SCkInspector_CompassAuthored::Get_ManualHeadingDisabledReason() const -> FString
{
    if (NOT _Active) { return FString{}; }
    return ck_inspector_compass::GetManualHeadingGate(_Entity).Reason.ToString();
}

auto SCkInspector_CompassAuthored::Commit_ManualHeading(const float InHeadingDegrees) -> void
{
    auto Compass = FCk_Handle_Compass{};
    if (NOT _Active || NOT ck_inspector_compass::TryGetCompass(_Entity, Compass)
        || NOT ck_inspector_compass::GetManualHeadingGate(_Entity).IsEnabled)
    { return; }
    UCk_Utils_Compass_UE::Request_SetManualHeading(Compass, InHeadingDegrees, {});
}

auto SCkInspector_CompassAuthored::Build_ManualHeadingValue() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_CompassAuthored> WeakWidget{SharedThis(this)};
    const TSharedPtr<FCkInspectorEditScope> EditScope = _ManualHeadingEditScope;
    const TSharedRef<SCkDebug_NumericEditor> Editor = SNew(SCkDebug_NumericEditor)
        .Tag(FName{TEXT("compass-manual-heading-input")})
        .Value_Lambda([WeakWidget]() -> double
        {
            const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() ? Widget->Get_ManualHeading() : 0.0;
        })
        .Kind(ECkDebug_NumericKind::Float)
        .MinValue(0.0)
        .MaxValue(360.0)
        .Width(78.0f)
        .FractionalDigits(1)
        .ForegroundColor(CkStyle::Value_Numeric())
        .OnValueCommitted_Lambda([WeakWidget](const double InValue)
        {
            if (const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Commit_ManualHeading(static_cast<float>(InValue)); }
        })
        .OnEditStateChanged_Lambda([EditScope](const bool InIsEditing)
        {
            if (EditScope.IsValid()) { EditScope->Set_Active(InIsEditing); }
        });
    Editor->SetEnabled(TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_CanEditManualHeading();
    }));
    Editor->SetToolTipText(TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_ManualHeadingDisabledReason()) : FText::GetEmpty();
    }));

    const FCkDebuggerStyleSelection& Selection = UCkDebuggerStyleSettings::Get_Selection();
    if (NOT ck::debug_axes::EditControls_AreVisible(Selection))
    {
        return SNew(STextBlock)
            .Tag(FName{TEXT("compass-manual-heading-read-only")})
            .Text_Lambda([WeakWidget]()
            {
                const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
                return Widget.IsValid() ? FText::FromString(Widget->Get_ManualHeadingText()) : FText::GetEmpty();
            })
            .ColorAndOpacity(CkStyle::Value_Numeric());
    }
    if (NOT ck::debug_axes::EditControls_RevealOnHover(Selection)) { return Editor; }

    const TSharedRef<SBox> HoverHost = SNew(SBox).Tag(FName{TEXT("compass-manual-heading-hover-host")});
    const TWeakPtr<SBox> WeakHoverHost{HoverHost};
    const auto IsHovered = [WeakHoverHost]()
    {
        const TSharedPtr<SBox> Host = WeakHoverHost.Pin();
        return Host.IsValid() && Host->IsHovered();
    };
    const TSharedRef<STextBlock> ReadOnly = SNew(STextBlock)
        .Tag(FName{TEXT("compass-manual-heading-read-only")})
        .Text_Lambda([WeakWidget]()
        {
            const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() ? FText::FromString(Widget->Get_ManualHeadingText()) : FText::GetEmpty();
        })
        .ColorAndOpacity(CkStyle::TextDim());
    ReadOnly->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Hidden : EVisibility::Visible; }));
    Editor->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Visible : EVisibility::Hidden; }));
    HoverHost->SetContent(SNew(SOverlay)
        + SOverlay::Slot()[ReadOnly]
        + SOverlay::Slot()[Editor]);
    return HoverHost;
}

auto SCkInspector_CompassAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_CompassAuthored> WeakWidget{SharedThis(this)};
    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(TEXT("compass-manual-heading-port"), Build_ManualHeadingValue());
    NativeBindings.Add(TEXT("compass-observer-port"),
        SNew(SCkDebug_EntityRef)
            .Entity_Lambda([WeakWidget]() -> FCk_Handle
            {
                const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
                return Widget.IsValid() ? Widget->Get_Observer() : FCk_Handle{};
            })
            .ShowName(true));

    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, WeakWidget](const FString& InName,
        FString (SCkInspector_CompassAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("compass-heading"), &SCkInspector_CompassAuthored::Get_HeadingText);
    BindText(TEXT("compass-source"), &SCkInspector_CompassAuthored::Get_SourceText);
    BindText(TEXT("compass-arc"), &SCkInspector_CompassAuthored::Get_ArcText);
    BindText(TEXT("compass-entries"), &SCkInspector_CompassAuthored::Get_EntriesText);
    BindText(TEXT("compass-filter"), &SCkInspector_CompassAuthored::Get_FilterText);
    BindText(TEXT("compass-interval"), &SCkInspector_CompassAuthored::Get_IntervalText);
    Data.Number.Add(TEXT("compass-entries-fraction"), TAttribute<float>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() ? Widget->Get_EntriesFraction() : 0.0f;
    }));
    Data.Visibility.Add(TEXT("compass-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    const auto BindDiffColor = [&Data, WeakWidget](const FString& InName,
        const bool SCkInspector_CompassAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_CompassAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_compass::DiffColor(Widget.Get()->*InMember) : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("compass-heading-diff-color"), &SCkInspector_CompassAuthored::_HeadingDiffMarked);
    BindDiffColor(TEXT("compass-manual-heading-diff-color"), &SCkInspector_CompassAuthored::_ManualHeadingDiffMarked);
    BindDiffColor(TEXT("compass-source-diff-color"), &SCkInspector_CompassAuthored::_SourceDiffMarked);
    BindDiffColor(TEXT("compass-observer-diff-color"), &SCkInspector_CompassAuthored::_ObserverDiffMarked);
    BindDiffColor(TEXT("compass-arc-diff-color"), &SCkInspector_CompassAuthored::_ArcDiffMarked);
    BindDiffColor(TEXT("compass-entries-diff-color"), &SCkInspector_CompassAuthored::_EntriesDiffMarked);
    BindDiffColor(TEXT("compass-filter-diff-color"), &SCkInspector_CompassAuthored::_FilterDiffMarked);
    BindDiffColor(TEXT("compass-interval-diff-color"), &SCkInspector_CompassAuthored::_IntervalDiffMarked);
    Data.Color.Add(TEXT("compass-entries-fill"), TAttribute<FLinearColor>::CreateLambda([]()
    { return CkStyle::GetToneColor(ECk_Tone::Accent); }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorCompass.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorCompass.ui.css")));
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

auto SCkInspector_CompassAuthored::Tick(
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

auto SCkInspector_CompassAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    if (_ManualHeadingEditScope.IsValid()) { _ManualHeadingEditScope->Set_Active(false); }
    _Entity = {};
    _View.Reset();
    _ManualHeadingEditScope.Reset();
    _Mounted = false;
}

// =====================================================================================================================

FCkInspector_Compass::~FCkInspector_Compass()
{
    OnDeactivated();
}

auto FCkInspector_Compass::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Compass"));
}

auto FCkInspector_Compass::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Compass = FCk_Handle_Compass{};
    return ck_inspector_compass::TryGetCompass(Entity, Compass);
}

auto FCkInspector_Compass::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto Compass = FCk_Handle_Compass{};
    if (NOT ck_inspector_compass::TryGetCompass(Entity, Compass)) { return Builder.Build(Entity, FString()); }
    const FCk_Handle CapturedEntity = Entity;
    const FCk_Handle_Compass CapturedCompass = Compass;

    Builder.AddRow(FText::FromString(TEXT("Heading:")), [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_compass::GetHeadingText(CapturedEntity)); }, CkStyle::Value_Numeric());
    Builder.AddNumericRow(
        FText::FromString(TEXT("Manual Heading:")),
        TAttribute<float>::CreateLambda([CapturedEntity]() { return ck_inspector_compass::GetHeading(CapturedEntity); }),
        [CapturedEntity](const float InHeadingDegrees)
        {
            auto CurrentCompass = FCk_Handle_Compass{};
            if (ck_inspector_compass::TryGetCompass(CapturedEntity, CurrentCompass))
            { UCk_Utils_Compass_UE::Request_SetManualHeading(CurrentCompass, InHeadingDegrees, {}); }
        },
        0.0f,
        360.0f,
        ECk_DebugRequest_Requirement::CosmeticOnly);
    Builder.AddRow(FText::FromString(TEXT("Source:")), [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_compass::GetSourceText(CapturedEntity)); }, CkStyle::Value_Enum());
    Builder.AddWidgetRow(FText::FromString(TEXT("Observer:")),
        SNew(SCkDebug_EntityRef)
            .Entity_Lambda([CapturedCompass]() -> FCk_Handle
            {
                return ck::IsValid(CapturedCompass) ? UCk_Utils_Compass_UE::Get_Observer(CapturedCompass) : FCk_Handle{};
            })
            .ShowName(true));
    Builder.AddRow(FText::FromString(TEXT("Arc:")), [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_compass::GetArcText(CapturedEntity)); }, CkStyle::Value_Numeric());
    Builder.AddMeterRow(
        FText::FromString(TEXT("Entries:")),
        TAttribute<float>::CreateLambda([CapturedEntity]() { return ck_inspector_compass::GetEntriesFraction(CapturedEntity); }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        { return FText::FromString(ck_inspector_compass::GetEntriesText(CapturedEntity)); }));
    Builder.AddRow(FText::FromString(TEXT("Filter:")), [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_compass::GetFilterText(CapturedEntity)); }, CkStyle::Value_Tag());
    Builder.AddRow(FText::FromString(TEXT("Interval:")), [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_compass::GetIntervalText(CapturedEntity)); }, CkStyle::Value_Numeric());
    return Builder.Build(Entity, FString());
}

auto FCkInspector_Compass::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_CompassAuthored> Authored = SNew(SCkInspector_CompassAuthored)
        .Entity(Entity)
        .EditGuard(Get_EditGuard())
        .HeadingDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Heading:")))
        .ManualHeadingDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Manual Heading:")))
        .SourceDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Source:")))
        .ObserverDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Observer:")))
        .ArcDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Arc:")))
        .EntriesDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Entries:")))
        .FilterDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Filter:")))
        .IntervalDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Interval:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Compass::Tick(const FCk_Handle&, const float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_CompassAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_Compass::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_CompassAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_CompassAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}

// =====================================================================================================================
