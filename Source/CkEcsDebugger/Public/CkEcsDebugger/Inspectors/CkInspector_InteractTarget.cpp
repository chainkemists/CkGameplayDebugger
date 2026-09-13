#include "CkInspector_InteractTarget.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_InteractTarget)

namespace ck_inspector_interact_target
{
    auto GetHandleKey(const FCk_Handle& InHandle) -> FString
    { return ck::IsValid(InHandle) ? ck::Format_UE(TEXT("{}"), InHandle.Get_Entity()) : FString{}; }

    auto IsTargetComplete(const FCk_Handle_InteractTarget& InTarget) -> bool
    {
        return ck::IsValid(InTarget) && InTarget.Has<ck::FFragment_InteractTarget_Params>()
            && InTarget.Has<ck::FFragment_InteractTarget_Current>();
    }

    auto GatherTargets(const FCk_Handle& InOwner) -> TArray<FCk_Handle_InteractTarget>
    {
        auto Targets = TArray<FCk_Handle_InteractTarget>{};
        if (ck::Is_NOT_Valid(InOwner) || NOT InOwner.Has<ck::FFragment_RecordOfInteractTargets>()) { return Targets; }
        UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(InOwner, [&Targets](FCk_Handle_InteractTarget InTarget)
        {
            if (IsTargetComplete(InTarget))
            { Targets.Add(InTarget); }
        });
        return Targets;
    }

    auto TryResolveTarget(const FCk_Handle& InOwner, const FCk_Handle_InteractTarget& InExpected,
        FCk_Handle_InteractTarget& OutTarget) -> bool
    {
        if (ck::Is_NOT_Valid(InExpected)) { return false; }
        const FString ExpectedKey = GetHandleKey(InExpected);
        for (const FCk_Handle_InteractTarget& Candidate : GatherTargets(InOwner))
        {
            if (Candidate == InExpected && GetHandleKey(Candidate) == ExpectedKey)
            { OutTarget = Candidate; return true; }
        }
        return false;
    }

    auto GatherInteractions(FCk_Handle_InteractTarget InTarget) -> TArray<FCk_Handle>
    {
        auto Handles = TArray<FCk_Handle>{};
        if (ck::Is_NOT_Valid(InTarget)) { return Handles; }
        for (const FCk_Handle_Interaction& Interaction : UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(InTarget))
        { if (ck::IsValid(Interaction)) { Handles.Add(Interaction); } }
        return Handles;
    }

    auto TextField(const FString& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    auto DiffColor(const bool bMarked) -> FLinearColor
    { return bMarked ? CkStyle::Accent() : CkStyle::Text(); }
}

auto SCkInspector_InteractTargetAuthored::Construct(const FArguments& InArgs) -> void
{
    _Owner = InArgs._Owner; _Target = InArgs._Target; _SelectionModel = InArgs._SelectionModel;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox); ChildSlot[Host];
    if (Refresh_Interactions() && Build_AuthoredView())
    { Host->SetContent(_View->GetRegion(TEXT("main"))); return; }
    Release(); Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_InteractTargetAuthored::~SCkInspector_InteractTargetAuthored() { Release(); }

auto SCkInspector_InteractTargetAuthored::Get_IsAvailable() const -> bool
{
    auto Target = FCk_Handle_InteractTarget{};
    return _Active && ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target);
}

auto SCkInspector_InteractTargetAuthored::Get_CanRequest() const -> bool
{
    auto Target = FCk_Handle_InteractTarget{};
    return _Active && ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)
        && ck::DebugRequestGate::Evaluate(Target, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_InteractTargetAuthored::Get_ChannelText() const -> FString
{
    auto Target = FCk_Handle_InteractTarget{};
    if (NOT ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)) { return TEXT("--"); }
    const FGameplayTag& Channel = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(Target);
    return Channel.IsValid() ? Channel.GetTagName().ToString() : TEXT("None");
}

auto SCkInspector_InteractTargetAuthored::Get_EnabledText() const -> FString
{
    auto Target = FCk_Handle_InteractTarget{};
    return ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_Enabled(Target)) : TEXT("--");
}

auto SCkInspector_InteractTargetAuthored::Get_EnabledTone() const -> FLinearColor
{
    auto Target = FCk_Handle_InteractTarget{};
    if (NOT ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target))
    { return CkStyle::GetToneColor(ECk_Tone::Neutral); }
    return CkStyle::GetToneColor(UCk_Utils_InteractTarget_UE::Get_Enabled(Target) == ECk_EnableDisable::Enable
        ? ECk_Tone::Ok : ECk_Tone::Err);
}

auto SCkInspector_InteractTargetAuthored::Get_EnabledBackground() const -> FLinearColor
{
    auto Target = FCk_Handle_InteractTarget{};
    if (NOT ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target))
    { return CkStyle::GetToneDimColor(ECk_Tone::Neutral); }
    return CkStyle::GetToneDimColor(UCk_Utils_InteractTarget_UE::Get_Enabled(Target) == ECk_EnableDisable::Enable
        ? ECk_Tone::Ok : ECk_Tone::Err);
}

auto SCkInspector_InteractTargetAuthored::Get_IsEnabled() const -> bool
{
    auto Target = FCk_Handle_InteractTarget{};
    return ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)
        && UCk_Utils_InteractTarget_UE::Get_Enabled(Target) == ECk_EnableDisable::Enable;
}

auto SCkInspector_InteractTargetAuthored::Get_CompletionText() const -> FString
{
    auto Target = FCk_Handle_InteractTarget{};
    return ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(Target)) : TEXT("--");
}

auto SCkInspector_InteractTargetAuthored::Get_DurationText() const -> FString
{
    auto Target = FCk_Handle_InteractTarget{};
    if (NOT ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)) { return TEXT("--"); }
    return UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(Target) == ECk_Interaction_CompletionPolicy::Timed
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_InteractionDuration(Target)) : TEXT("N/A");
}

auto SCkInspector_InteractTargetAuthored::Get_DurationColor() const -> FLinearColor
{
    auto Target = FCk_Handle_InteractTarget{};
    if (NOT ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)) { return CkStyle::None(); }
    return UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(Target) == ECk_Interaction_CompletionPolicy::Timed
        ? CkStyle::Value_Numeric() : CkStyle::None();
}

auto SCkInspector_InteractTargetAuthored::Get_ConcurrentText() const -> FString
{
    auto Target = FCk_Handle_InteractTarget{};
    return ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_ConcurrentInteractionsPolicy(Target)) : TEXT("--");
}

auto SCkInspector_InteractTargetAuthored::Get_RequestDisabledReason() const -> FString
{
    auto Target = FCk_Handle_InteractTarget{};
    return _Active && ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)
        ? ck::DebugRequestGate::Evaluate(Target, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString()
        : TEXT("Interact Target is unavailable.");
}

auto SCkInspector_InteractTargetAuthored::Set_Enabled(const bool InEnabled) -> void
{
    if (NOT Get_CanRequest()) { return; }
    auto Target = FCk_Handle_InteractTarget{};
    if (ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target))
    { UCk_Utils_InteractTarget_UE::Set_Enabled(Target, InEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable); }
}

auto SCkInspector_InteractTargetAuthored::Request_CancelAll() -> void
{
    if (NOT Get_CanRequest()) { return; }
    auto Target = FCk_Handle_InteractTarget{};
    if (ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target))
    { UCk_Utils_InteractTarget_UE::Request_CancelAllInteractions(Target); }
}

auto SCkInspector_InteractTargetAuthored::Refresh_Interactions() -> bool
{
    if (NOT _Interactions.IsValid())
    {
        const FCkUiLoadResult Result = FCkUiCollection::TryCreate(
            {{TEXT("interaction-id"), ECkUiFieldKind::Text}, {TEXT("interaction-name"), ECkUiFieldKind::Text}}, _Interactions);
        if (NOT Result.Succeeded || NOT _Interactions.IsValid())
        { _LoadError = FString::Join(Result.Errors, TEXT("\n")); return false; }
    }
    auto Records = TArray<FCkUiRecordData>{};
    auto ByKey = TMap<FString, FCk_Handle>{};
    auto Snapshot = TArray<FString>{};
    auto Target = FCk_Handle_InteractTarget{};
    if (ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target))
    {
        for (const FCk_Handle& Interaction : ck_inspector_interact_target::GatherInteractions(Target))
        {
            const FString Key = ck_inspector_interact_target::GetHandleKey(Interaction);
            if (Key.IsEmpty()) { continue; }
            auto Record = FCkUiRecordData{}; Record.Key = Key;
            Record.Fields.Add(TEXT("interaction-id"), ck_inspector_interact_target::TextField(
                ck::Format_UE(TEXT("{}"), Interaction.Get_Entity())));
            Record.Fields.Add(TEXT("interaction-name"), ck_inspector_interact_target::TextField(
                UCk_Utils_Handle_UE::Get_DebugName(Interaction).ToString()));
            Snapshot.Add(Key + TEXT("|") + UCk_Utils_Handle_UE::Get_DebugName(Interaction).ToString());
            Records.Add(MoveTemp(Record)); ByKey.Add(Key, Interaction);
        }
    }
    if (Snapshot == _InteractionSnapshot)
    { _InteractionsByKey = MoveTemp(ByKey); return true; }
    const FCkUiLoadResult Result = _Interactions->TrySetRecords(MoveTemp(Records));
    if (NOT Result.Succeeded) { _LoadError = FString::Join(Result.Errors, TEXT("\n")); return false; }
    _InteractionsByKey = MoveTemp(ByKey); _InteractionSnapshot = MoveTemp(Snapshot); return true;
}

auto SCkInspector_InteractTargetAuthored::Navigate_Interaction(const FString& InStableKey) -> void
{
    if (NOT Get_IsAvailable()) { return; }
    const FCk_Handle* Expected = _InteractionsByKey.Find(InStableKey);
    if (Expected == nullptr || ck::Is_NOT_Valid(*Expected)) { return; }
    auto Target = FCk_Handle_InteractTarget{};
    if (NOT ck_inspector_interact_target::TryResolveTarget(_Owner, _Target, Target)) { return; }
    for (const FCk_Handle& Current : ck_inspector_interact_target::GatherInteractions(Target))
    {
        if (Current != *Expected || ck_inspector_interact_target::GetHandleKey(Current) != InStableKey) { continue; }
        if (const TSharedPtr<FCkDebuggerModel_EntitySelection> Selection = _SelectionModel.Pin(); Selection.IsValid())
        { Selection->Set_SelectedEntities({Current}); }
        else { ck::DebugNav::Goto_Entity(Current); }
        return;
    }
}

auto SCkInspector_InteractTargetAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n")); return false;
    }
    const TWeakPtr<SCkInspector_InteractTargetAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& Name, FString (SCkInspector_InteractTargetAuthored::* Getter)() const)
    {
        Data.Text.Add(Name, TAttribute<FText>::CreateLambda([Weak, Getter]()
        {
            const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*Getter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("interact-target-channel"), &SCkInspector_InteractTargetAuthored::Get_ChannelText);
    BindText(TEXT("interact-target-enabled-text"), &SCkInspector_InteractTargetAuthored::Get_EnabledText);
    BindText(TEXT("interact-target-completion"), &SCkInspector_InteractTargetAuthored::Get_CompletionText);
    BindText(TEXT("interact-target-duration"), &SCkInspector_InteractTargetAuthored::Get_DurationText);
    BindText(TEXT("interact-target-concurrent"), &SCkInspector_InteractTargetAuthored::Get_ConcurrentText);
    BindText(TEXT("interact-target-disabled-reason"), &SCkInspector_InteractTargetAuthored::Get_RequestDisabledReason);
    Data.Text.Add(TEXT("interact-target-cancel-label"), FText::FromString(TEXT("Cancel All")));
    Data.Text.Add(TEXT("interact-target-cancel-tooltip"), FText::FromString(
        TEXT("UCk_Utils_InteractTarget_UE::Request_CancelAllInteractions")));
    Data.Color.Add(TEXT("interact-target-enabled-tone"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_EnabledTone() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("interact-target-enabled-background"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_EnabledBackground() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("interact-target-duration-color"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_DurationColor() : FLinearColor::Transparent; }));
    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("channel"), TEXT("Channel:")}, {TEXT("enabled"), TEXT("Enabled:")},
        {TEXT("set-enabled"), TEXT("Set Enabled:")}, {TEXT("actions"), TEXT("Interactions:")},
        {TEXT("completion"), TEXT("Completion:")}, {TEXT("duration"), TEXT("Duration:")},
        {TEXT("concurrent"), TEXT("Concurrent:")}, {TEXT("current"), TEXT("Interactions:")}})
    {
        Data.Color.Add(TEXT("interact-target-") + Pair.Key + TEXT("-diff-color"),
            TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]()
            { const auto Widget = Weak.Pin(); return Widget.IsValid() ? ck_inspector_interact_target::DiffColor(Widget->Is_DiffMarked(Label)) : FLinearColor::Transparent; }));
    }
    Data.Visibility.Add(TEXT("interact-target-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("interact-target-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Data.Visibility.Add(TEXT("interact-target-enabled"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsEnabled(); }));
    Data.BoolChanged.Add(TEXT("interact-target-enabled-changed"), FCkUiOnBoolChanged::CreateLambda([Weak](const bool bEnabled)
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Set_Enabled(bEnabled); } }));
    Data.Collections.Add(TEXT("interact-target-interactions"), _Interactions);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    Data.ItemActions.Add(TEXT("interact-target-navigate-interaction"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& Key) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Navigate_Interaction(Key); } }));
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("interact-target-cancel-all"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_CancelAll(); } }));
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorInteractTarget.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorInteractTarget.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate; _Mounted = true; _LoadError.Reset(); return true;
}

auto SCkInspector_InteractTargetAuthored::Tick(const FGeometry& InGeometry, const double InTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InGeometry, InTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    if (NOT Refresh_Interactions()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_InteractTargetAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false; _Owner = {}; _Target = {}; _SelectionModel.Reset(); _DiffLabels.Reset();
    _InteractionsByKey.Reset(); _InteractionSnapshot.Reset(); _Interactions.Reset(); _View.Reset(); _Mounted = false;
}

FCkInspector_InteractTarget::~FCkInspector_InteractTarget() { OnDeactivated(); }
auto FCkInspector_InteractTarget::Get_ComponentName() const -> FText { return FText::FromString(TEXT("Interact Targets")); }
auto FCkInspector_InteractTarget::CanInspect(const FCk_Handle& Entity) const -> bool
{ return ck::IsValid(Entity) && Entity.Has<ck::FFragment_RecordOfInteractTargets>(); }
auto FCkInspector_InteractTarget::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{ return SNullWidget::NullWidget; }

auto FCkInspector_InteractTarget::Build_TargetNativeBody(const FCk_Handle_InteractTarget& InTarget) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder(); Builder.SetEditGuard(Get_EditGuard()); auto Target = InTarget;
    Builder.AddRow(FText::FromString(TEXT("Channel:")), [Target](const FCk_Handle&)
    { if (NOT ck_inspector_interact_target::IsTargetComplete(Target)) { return FText::FromString(TEXT("--")); } const auto& Tag = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(Target); return FText::FromString(Tag.IsValid() ? Tag.ToString() : TEXT("None")); }, CkStyle::Value_Tag());
    Builder.AddStatusPillRow(FText::FromString(TEXT("Enabled:")),
        TAttribute<FText>::CreateLambda([Target]() { return ck_inspector_interact_target::IsTargetComplete(Target) ? FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_Enabled(Target))) : FText::FromString(TEXT("--")); }),
        TAttribute<ECk_Tone>::CreateLambda([Target]() { return ck_inspector_interact_target::IsTargetComplete(Target) && UCk_Utils_InteractTarget_UE::Get_Enabled(Target) == ECk_EnableDisable::Enable ? ECk_Tone::Ok : ECk_Tone::Err; }));
    Builder.AddToggleRow(FText::FromString(TEXT("Set Enabled:")),
        TAttribute<bool>::CreateLambda([Target]() { return ck_inspector_interact_target::IsTargetComplete(Target) && UCk_Utils_InteractTarget_UE::Get_Enabled(Target) == ECk_EnableDisable::Enable; }),
        [Target](const bool bEnabled) { auto MutableTarget = Target; if (ck_inspector_interact_target::IsTargetComplete(MutableTarget)) { UCk_Utils_InteractTarget_UE::Set_Enabled(MutableTarget, bEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable); } },
        ECk_DebugRequest_Requirement::LocalOk);
    Builder.AddActionRow(FText::FromString(TEXT("Interactions:")), {{FText::FromString(TEXT("Cancel All")),
        FText::FromString(TEXT("UCk_Utils_InteractTarget_UE::Request_CancelAllInteractions")),
        [Target]() { auto MutableTarget = Target; if (ck_inspector_interact_target::IsTargetComplete(MutableTarget)) { UCk_Utils_InteractTarget_UE::Request_CancelAllInteractions(MutableTarget); } }, ECk_DebugRequest_Requirement::LocalOk}});
    Builder.AddRow(FText::FromString(TEXT("Completion:")), [Target](const FCk_Handle&)
    { return ck_inspector_interact_target::IsTargetComplete(Target) ? FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(Target))) : FText::FromString(TEXT("--")); }, CkStyle::Value_Enum());
    Builder.AddConditionalRow(FText::FromString(TEXT("Duration:")), [Target](const FCk_Handle&)
    { if (NOT ck_inspector_interact_target::IsTargetComplete(Target)) { return FText::FromString(TEXT("--")); } return FText::FromString(UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(Target) == ECk_Interaction_CompletionPolicy::Timed
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_InteractionDuration(Target)) : TEXT("N/A")); },
        [Target](const FCk_Handle&) { return ck_inspector_interact_target::IsTargetComplete(Target) && UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(Target) == ECk_Interaction_CompletionPolicy::Timed ? CkStyle::Value_Numeric() : CkStyle::None(); });
    Builder.AddRow(FText::FromString(TEXT("Concurrent:")), [Target](const FCk_Handle&)
    { return ck_inspector_interact_target::IsTargetComplete(Target) ? FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_ConcurrentInteractionsPolicy(Target))) : FText::FromString(TEXT("--")); }, CkStyle::Value_Enum());
    Builder.AddWidgetRow(FText::FromString(TEXT("Interactions:")),
        FCkInspectorWidgetBuilder::MakeBadgeBox(ck_inspector_interact_target::GatherInteractions(Target)));
    return Builder.Build(InTarget, FString{});
}

auto FCkInspector_InteractTarget::Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection>
{
    const auto Targets = ck_inspector_interact_target::GatherTargets(Entity);
    auto Sections = TArray<FInspectorSection>{}; _LastTargetKeys.Reset();
    for (const auto& Target : Targets)
    {
        const auto& Tag = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(Target);
        Sections.Add({FText::FromString(Tag.IsValid() ? Tag.ToString() : TEXT("Unknown Channel")), Build_TargetNativeBody(Target)});
        _LastTargetKeys.Add(ck_inspector_interact_target::GetHandleKey(Target));
    }
    if (FCkInspector_RowCaptureScope::Is_Active()) { return Sections; }
    auto Candidates = TArray<TSharedRef<SCkInspector_InteractTargetAuthored>>{};
    for (const auto& Target : Targets)
    {
        auto DiffLabels = TSet<FString>{};
        for (const TCHAR* Label : {TEXT("Channel:"), TEXT("Enabled:"), TEXT("Set Enabled:"), TEXT("Interactions:"),
            TEXT("Completion:"), TEXT("Duration:"), TEXT("Concurrent:")})
        { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }
        const auto Candidate = SNew(SCkInspector_InteractTargetAuthored).Owner(Entity).Target(Target)
            .SelectionModel(Get_SelectionModel()).DiffLabels(MoveTemp(DiffLabels));
        if (NOT Candidate->Is_Mounted())
        {
            _LastAuthoredLoadError = Candidate->Get_LoadError();
            for (const auto& Mounted : Candidates) { Mounted->Release(); }
            return Sections;
        }
        Candidates.Add(Candidate);
    }
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    { Sections[Index].Widget = Candidates[Index]; _AuthoredInstances.Add(Candidates[Index]); }
    _LastAuthoredLoadError.Reset(); return Sections;
}

auto FCkInspector_InteractTarget::Tick(const FCk_Handle& Entity, const float InDeltaTime) -> void
{
    static_cast<void>(InDeltaTime); auto Keys = TArray<FString>{};
    for (const auto& Target : ck_inspector_interact_target::GatherTargets(Entity))
    { Keys.Add(ck_inspector_interact_target::GetHandleKey(Target)); }
    if (Keys != _LastTargetKeys) { RequestRebuild(); }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_InteractTargetAuthored>& Instance)
    { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); });
}

auto FCkInspector_InteractTarget::OnDeactivated() -> void
{
    for (const auto& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset(); _LastTargetKeys.Reset();
}
