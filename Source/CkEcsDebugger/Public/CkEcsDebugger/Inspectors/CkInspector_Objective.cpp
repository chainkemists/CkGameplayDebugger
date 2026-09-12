#include "CkInspector_Objective.h"

#include "CkAttribute/ByteAttribute/CkByteAttribute_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkObjective/Objective/CkObjective_Fragment.h"
#include "CkObjective/Objective/CkObjective_Utils.h"

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
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Objective)

namespace ck_inspector_objective
{
    auto TryGetObjective(const FCk_Handle& InEntity, FCk_Handle_Objective& OutObjective) -> bool
    {
        if (NOT ck::IsValid(InEntity) || NOT UCk_Utils_Objective_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        const FCk_Handle_Objective Candidate = UCk_Utils_Objective_UE::CastChecked(MutableEntity);
        if (ck::Is_NOT_Valid(Candidate)) { return false; }
        const FCk_Handle StatusEntity = Candidate.Get<ck::FFragment_Objective_Current>().Get_StatusAttribute();
        if (NOT ck::IsValid(StatusEntity) || NOT UCk_Utils_ByteAttribute_UE::Has(StatusEntity)) { return false; }
        OutObjective = Candidate;
        return true;
    }

    auto GetStatusTone(const ECk_ObjectiveStatus InStatus) -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_ObjectiveStatus::NotStarted: return ECk_Tone::Neutral;
            case ECk_ObjectiveStatus::Active: return ECk_Tone::Accent;
            case ECk_ObjectiveStatus::Completed: return ECk_Tone::Ok;
            case ECk_ObjectiveStatus::Failed: return ECk_Tone::Err;
            default: return ECk_Tone::Neutral;
        }
    }

    auto GetStatus(const FCk_Handle& InEntity, ECk_ObjectiveStatus& OutStatus) -> bool
    {
        auto Objective = FCk_Handle_Objective{};
        if (NOT TryGetObjective(InEntity, Objective)) { return false; }
        OutStatus = UCk_Utils_Objective_UE::Get_Status(Objective);
        return true;
    }

    auto GetStatusText(const FCk_Handle& InEntity) -> FString
    {
        auto Status = ECk_ObjectiveStatus::NotStarted;
        return GetStatus(InEntity, Status) ? ck::Format_UE(TEXT("{}"), Status) : TEXT("--");
    }

    auto GetStatusTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        auto Status = ECk_ObjectiveStatus::NotStarted;
        return GetStatus(InEntity, Status) ? GetStatusTone(Status) : ECk_Tone::Neutral;
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

auto SCkInspector_ObjectiveAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _NameDiffMarked = InArgs._NameDiffMarked;
    _DisplayDiffMarked = InArgs._DisplayDiffMarked;
    _DescriptionDiffMarked = InArgs._DescriptionDiffMarked;
    _StatusDiffMarked = InArgs._StatusDiffMarked;
    _ControlDiffMarked = InArgs._ControlDiffMarked;
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

SCkInspector_ObjectiveAuthored::~SCkInspector_ObjectiveAuthored() { Release(); }

auto SCkInspector_ObjectiveAuthored::Get_IsAvailable() const -> bool
{
    auto Objective = FCk_Handle_Objective{};
    return _Active && ck_inspector_objective::TryGetObjective(_Entity, Objective);
}

auto SCkInspector_ObjectiveAuthored::Get_NameText() const -> FString
{
    auto Objective = FCk_Handle_Objective{};
    return ck_inspector_objective::TryGetObjective(_Entity, Objective)
        ? UCk_Utils_Objective_UE::Get_Name(Objective).ToString() : TEXT("--");
}

auto SCkInspector_ObjectiveAuthored::Get_DisplayText() const -> FString
{
    auto Objective = FCk_Handle_Objective{};
    return ck_inspector_objective::TryGetObjective(_Entity, Objective)
        ? UCk_Utils_Objective_UE::Get_DisplayName(Objective).ToString() : FString{};
}

auto SCkInspector_ObjectiveAuthored::Get_DescriptionText() const -> FString
{
    auto Objective = FCk_Handle_Objective{};
    return ck_inspector_objective::TryGetObjective(_Entity, Objective)
        ? UCk_Utils_Objective_UE::Get_Description(Objective).ToString() : FString{};
}

auto SCkInspector_ObjectiveAuthored::Get_StatusText() const -> FString
{
    return _Active ? ck_inspector_objective::GetStatusText(_Entity) : FString{};
}

auto SCkInspector_ObjectiveAuthored::Get_StatusForeground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneColor(ck_inspector_objective::GetStatusTone(_Entity)) : FLinearColor::Transparent;
}

auto SCkInspector_ObjectiveAuthored::Get_StatusBackground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneDimColor(ck_inspector_objective::GetStatusTone(_Entity)) : FLinearColor::Transparent;
}

auto SCkInspector_ObjectiveAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_ObjectiveAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    auto BindText = [&Data, WeakWidget](const FString& InName, FString (SCkInspector_ObjectiveAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("objective-name"), &SCkInspector_ObjectiveAuthored::Get_NameText);
    BindText(TEXT("objective-display"), &SCkInspector_ObjectiveAuthored::Get_DisplayText);
    BindText(TEXT("objective-description"), &SCkInspector_ObjectiveAuthored::Get_DescriptionText);
    BindText(TEXT("objective-status"), &SCkInspector_ObjectiveAuthored::Get_StatusText);
    Data.Text.Add(TEXT("objective-start-label"), FText::FromString(TEXT("Start")));
    Data.Text.Add(TEXT("objective-complete-label"), FText::FromString(TEXT("Complete")));
    Data.Text.Add(TEXT("objective-fail-label"), FText::FromString(TEXT("Fail")));
    Data.Text.Add(TEXT("objective-start-tooltip"), FText::FromString(TEXT("UCk_Utils_Objective_UE::Request_Start")));
    Data.Text.Add(TEXT("objective-complete-tooltip"), FText::FromString(TEXT("UCk_Utils_Objective_UE::Request_Complete (no metadata tag)")));
    Data.Text.Add(TEXT("objective-fail-tooltip"), FText::FromString(TEXT("UCk_Utils_Objective_UE::Request_Fail (no metadata tag)")));
    Data.Text.Add(TEXT("objective-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable()
            ? FText::GetEmpty() : FText::FromString(TEXT("Objective is unavailable."));
    }));
    Data.Visibility.Add(TEXT("objective-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    }));
    Data.Visibility.Add(TEXT("objective-has-display"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Get_DisplayText().IsEmpty();
    }));
    Data.Visibility.Add(TEXT("objective-has-description"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Get_DescriptionText().IsEmpty();
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    Data.Color.Add(TEXT("objective-status-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() ? Widget->Get_StatusForeground() : FLinearColor::Transparent;
    }));
    Data.Color.Add(TEXT("objective-status-background"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() ? Widget->Get_StatusBackground() : FLinearColor::Transparent;
    }));
    auto BindDiffColor = [&Data, WeakWidget](const FString& InName, const bool SCkInspector_ObjectiveAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_objective::DiffColor(Widget.Get()->*InMember) : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("objective-name-diff-color"), &SCkInspector_ObjectiveAuthored::_NameDiffMarked);
    BindDiffColor(TEXT("objective-display-diff-color"), &SCkInspector_ObjectiveAuthored::_DisplayDiffMarked);
    BindDiffColor(TEXT("objective-description-diff-color"), &SCkInspector_ObjectiveAuthored::_DescriptionDiffMarked);
    BindDiffColor(TEXT("objective-status-diff-color"), &SCkInspector_ObjectiveAuthored::_StatusDiffMarked);
    BindDiffColor(TEXT("objective-control-diff-color"), &SCkInspector_ObjectiveAuthored::_ControlDiffMarked);

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("objective-start"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_Start(); } }));
    Actions.Add(TEXT("objective-complete"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_Complete(); } }));
    Actions.Add(TEXT("objective-fail"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const TSharedPtr<SCkInspector_ObjectiveAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_Fail(); } }));
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorObjective.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorObjective.ui.css")));
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

auto SCkInspector_ObjectiveAuthored::Request_Start() -> void
{
    auto Objective = FCk_Handle_Objective{};
    if (_Active && ck_inspector_objective::TryGetObjective(_Entity, Objective))
    { UCk_Utils_Objective_UE::Request_Start(Objective, FCk_Request_Objective_Start{}, {}); }
}

auto SCkInspector_ObjectiveAuthored::Request_Complete() -> void
{
    auto Objective = FCk_Handle_Objective{};
    if (_Active && ck_inspector_objective::TryGetObjective(_Entity, Objective))
    { UCk_Utils_Objective_UE::Request_Complete(Objective, FCk_Request_Objective_Complete{}, {}); }
}

auto SCkInspector_ObjectiveAuthored::Request_Fail() -> void
{
    auto Objective = FCk_Handle_Objective{};
    if (_Active && ck_inspector_objective::TryGetObjective(_Entity, Objective))
    { UCk_Utils_Objective_UE::Request_Fail(Objective, FCk_Request_Objective_Fail{}, {}); }
}

auto SCkInspector_ObjectiveAuthored::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_ObjectiveAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _View.Reset();
    _Mounted = false;
}

FCkInspector_Objective::~FCkInspector_Objective() { OnDeactivated(); }

auto FCkInspector_Objective::Get_ComponentName() const -> FText { return FText::FromString(TEXT("Objective")); }

auto FCkInspector_Objective::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Objective = FCk_Handle_Objective{};
    return ck_inspector_objective::TryGetObjective(Entity, Objective);
}

auto FCkInspector_Objective::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    auto Objective = FCk_Handle_Objective{};
    if (NOT ck_inspector_objective::TryGetObjective(Entity, Objective)) { return Builder.Build(Entity, FString{}); }
    const FGameplayTag Name = UCk_Utils_Objective_UE::Get_Name(Objective);
    Builder.AddRow(FText::FromString(TEXT("Name:")),
        [Name](const FCk_Handle&) { return FText::FromString(Name.ToString()); }, CkStyle::Status_Active());
    const FText DisplayName = UCk_Utils_Objective_UE::Get_DisplayName(Objective);
    if (NOT DisplayName.IsEmpty())
    { Builder.AddRow(FText::FromString(TEXT("Display:")), [DisplayName](const FCk_Handle&) { return DisplayName; }, CkStyle::Text()); }
    const FText Description = UCk_Utils_Objective_UE::Get_Description(Objective);
    if (NOT Description.IsEmpty())
    { Builder.AddRow(FText::FromString(TEXT("Description:")), [Description](const FCk_Handle&) { return Description; }, CkStyle::TextDim()); }
    const FCk_Handle CapturedEntity = Entity;
    Builder.AddStatusPillRow(FText::FromString(TEXT("Status:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        { return FText::FromString(ck_inspector_objective::GetStatusText(CapturedEntity)); }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        { return ck_inspector_objective::GetStatusTone(CapturedEntity); }));
    const auto MakeAction = [CapturedEntity](const FString& InLabel, const FString& InTooltip, const int32 InKind)
    {
        return FCkInspector_Action{FText::FromString(InLabel), FText::FromString(InTooltip),
            [CapturedEntity, InKind]()
            {
                auto Current = FCk_Handle_Objective{};
                if (NOT ck_inspector_objective::TryGetObjective(CapturedEntity, Current)) { return; }
                if (InKind == 0) { UCk_Utils_Objective_UE::Request_Start(Current, FCk_Request_Objective_Start{}, {}); }
                else if (InKind == 1) { UCk_Utils_Objective_UE::Request_Complete(Current, FCk_Request_Objective_Complete{}, {}); }
                else { UCk_Utils_Objective_UE::Request_Fail(Current, FCk_Request_Objective_Fail{}, {}); }
            }, ECk_DebugRequest_Requirement::LocalOk};
    };
    Builder.AddActionRow(FText::FromString(TEXT("Control:")), {
        MakeAction(TEXT("Start"), TEXT("UCk_Utils_Objective_UE::Request_Start"), 0),
        MakeAction(TEXT("Complete"), TEXT("UCk_Utils_Objective_UE::Request_Complete (no metadata tag)"), 1),
        MakeAction(TEXT("Fail"), TEXT("UCk_Utils_Objective_UE::Request_Fail (no metadata tag)"), 2)});
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_Objective::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    const TSharedRef<SCkInspector_ObjectiveAuthored> Authored = SNew(SCkInspector_ObjectiveAuthored)
        .Entity(Entity)
        .NameDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Name:")))
        .DisplayDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Display:")))
        .DescriptionDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Description:")))
        .StatusDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Status:")))
        .ControlDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Control:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Objective::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_ObjectiveAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_Objective::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_ObjectiveAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_ObjectiveAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
