#include "CkInspector_ObjectiveOwner.h"

#include "CkAttribute/ByteAttribute/CkByteAttribute_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEntityCollection/CkEntityCollection_Utils.h"
#include "CkObjective/Objective/CkObjective_Fragment.h"
#include "CkObjective/Objective/CkObjective_Utils.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Fragment.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Utils.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ObjectiveOwner)

namespace ck_inspector_objective_owner
{
    auto TryGetOwner(const FCk_Handle& InEntity, FCk_Handle_ObjectiveOwner& OutOwner) -> bool
    {
        if (NOT ck::IsValid(InEntity) || NOT UCk_Utils_ObjectiveOwner_UE::Has(InEntity)) { return false; }

        auto MutableEntity = InEntity;
        const FCk_Handle_ObjectiveOwner Candidate = UCk_Utils_ObjectiveOwner_UE::CastChecked(MutableEntity);
        if (ck::Is_NOT_Valid(Candidate)
            || NOT Candidate.Has<ck::FFragment_ObjectiveOwner_Params>()
            || NOT Candidate.Has<ck::FFragment_ObjectiveOwner_Current>())
        { return false; }

        const FCk_Handle_EntityCollection Collection = Candidate.Get<ck::FFragment_ObjectiveOwner_Current>().Get_ObjectivesEntityCollection();
        if (ck::Is_NOT_Valid(Collection) || NOT UCk_Utils_EntityCollection_UE::Has(FCk_Handle{Collection}))
        { return false; }

        OutOwner = Candidate;
        return true;
    }

    auto TryGetObjective(const FCk_Handle_Objective& InObjective, FCk_Handle_Objective& OutObjective) -> bool
    {
        if (ck::Is_NOT_Valid(InObjective)) { return false; }

        const FCk_Handle ObjectiveEntity{InObjective};
        if (NOT UCk_Utils_Objective_UE::Has(ObjectiveEntity)
            || NOT InObjective.Has<ck::FFragment_Objective_Params>()
            || NOT InObjective.Has<ck::FFragment_Objective_Current>())
        { return false; }

        const FCk_Handle StatusAttribute = InObjective.Get<ck::FFragment_Objective_Current>().Get_StatusAttribute();
        if (NOT ck::IsValid(StatusAttribute) || NOT UCk_Utils_ByteAttribute_UE::Has(StatusAttribute))
        { return false; }

        OutObjective = InObjective;
        return true;
    }

    auto GetStableKey(const FCk_Handle_Objective& InObjective) -> FString
    {
        FCk_Handle_Objective Objective;
        if (NOT TryGetObjective(InObjective, Objective)) { return FString{}; }

        // The objective tag is author-facing but may be duplicated; the entity text carries number and generation.
        return FString::Printf(TEXT("%s|%s"), *UCk_Utils_Objective_UE::Get_Name(Objective).ToString(), *Objective.Get_Entity().ToString());
    }

    auto GetStatusTone(const ECk_ObjectiveStatus InStatus) -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_ObjectiveStatus::NotStarted: return ECk_Tone::Neutral;
            case ECk_ObjectiveStatus::Active: return ECk_Tone::Info;
            case ECk_ObjectiveStatus::Completed: return ECk_Tone::Ok;
            case ECk_ObjectiveStatus::Failed: return ECk_Tone::Err;
            default: return ECk_Tone::Neutral;
        }
    }

    auto GetObjectives(const FCk_Handle& InEntity) -> TArray<FCk_Handle_Objective>
    {
        auto Owner = FCk_Handle_ObjectiveOwner{};
        if (NOT TryGetOwner(InEntity, Owner)) { return {}; }

        auto Objectives = TArray<FCk_Handle_Objective>{};
        for (const FCk_Handle_Objective& Candidate : UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(Owner))
        {
            auto Objective = FCk_Handle_Objective{};
            if (TryGetObjective(Candidate, Objective)) { Objectives.Add(Objective); }
        }
        return Objectives;
    }

    auto FindObjectiveByKey(const FCk_Handle& InEntity, const FString& InStableKey, FCk_Handle_Objective& OutObjective) -> bool
    {
        if (InStableKey.IsEmpty()) { return false; }
        for (const FCk_Handle_Objective& Objective : GetObjectives(InEntity))
        {
            if (GetStableKey(Objective) == InStableKey)
            {
                OutObjective = Objective;
                return true;
            }
        }
        return false;
    }

    auto GetProgressText(const FCk_Handle& InEntity) -> FString
    {
        auto Owner = FCk_Handle_ObjectiveOwner{};
        if (NOT TryGetOwner(InEntity, Owner)) { return TEXT("--"); }

        const TArray<FCk_Handle_Objective> Objectives = GetObjectives(InEntity);
        auto CompletedCount = 0;
        for (const FCk_Handle_Objective& Objective : Objectives)
        {
            if (UCk_Utils_Objective_UE::Get_Status(Objective) == ECk_ObjectiveStatus::Completed) { ++CompletedCount; }
        }
        return ck::Format_UE(TEXT("{} / {}"), CompletedCount, Objectives.Num());
    }

    auto GetProgressFraction(const FCk_Handle& InEntity) -> float
    {
        const TArray<FCk_Handle_Objective> Objectives = GetObjectives(InEntity);
        if (Objectives.IsEmpty()) { return 0.0f; }

        auto CompletedCount = 0;
        for (const FCk_Handle_Objective& Objective : Objectives)
        {
            if (UCk_Utils_Objective_UE::Get_Status(Objective) == ECk_ObjectiveStatus::Completed) { ++CompletedCount; }
        }
        return static_cast<float>(CompletedCount) / static_cast<float>(Objectives.Num());
    }

    auto GetStatusText(const FCk_Handle_Objective& InObjective) -> FString
    {
        auto Objective = FCk_Handle_Objective{};
        return TryGetObjective(InObjective, Objective)
            ? ck::Format_UE(TEXT("{}"), UCk_Utils_Objective_UE::Get_Status(Objective)) : TEXT("--");
    }

    auto GetStatusTone(const FCk_Handle_Objective& InObjective) -> ECk_Tone
    {
        auto Objective = FCk_Handle_Objective{};
        return TryGetObjective(InObjective, Objective)
            ? GetStatusTone(UCk_Utils_Objective_UE::Get_Status(Objective)) : ECk_Tone::Neutral;
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkInspector_ObjectiveOwnerAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _Filter = InArgs._Filter;
    _SelectionModel = InArgs._SelectionModel;
    _ProgressDiffMarked = InArgs._ProgressDiffMarked;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_ObjectiveRecords() && Build_AuthoredView())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_ObjectiveOwnerAuthored::~SCkInspector_ObjectiveOwnerAuthored()
{
    Release();
}

auto SCkInspector_ObjectiveOwnerAuthored::Get_IsAvailable() const -> bool
{
    auto Owner = FCk_Handle_ObjectiveOwner{};
    return _Active && ck_inspector_objective_owner::TryGetOwner(_Entity, Owner);
}

auto SCkInspector_ObjectiveOwnerAuthored::Get_ProgressText() const -> FString
{
    return _Active ? ck_inspector_objective_owner::GetProgressText(_Entity) : FString{};
}

auto SCkInspector_ObjectiveOwnerAuthored::Get_ProgressFraction() const -> float
{
    return _Active ? ck_inspector_objective_owner::GetProgressFraction(_Entity) : 0.0f;
}

auto SCkInspector_ObjectiveOwnerAuthored::Get_ObjectiveCount() const -> int32
{
    return _Active ? ck_inspector_objective_owner::GetObjectives(_Entity).Num() : 0;
}

auto SCkInspector_ObjectiveOwnerAuthored::Get_ObjectiveStatusText(const FString& InStableKey) const -> FString
{
    auto Objective = FCk_Handle_Objective{};
    return _Active && ck_inspector_objective_owner::FindObjectiveByKey(_Entity, InStableKey, Objective)
        ? ck_inspector_objective_owner::GetStatusText(Objective) : TEXT("--");
}

auto SCkInspector_ObjectiveOwnerAuthored::Get_ObjectiveTone(const FString& InStableKey) const -> ECk_Tone
{
    auto Objective = FCk_Handle_Objective{};
    return _Active && ck_inspector_objective_owner::FindObjectiveByKey(_Entity, InStableKey, Objective)
        ? ck_inspector_objective_owner::GetStatusTone(Objective) : ECk_Tone::Neutral;
}

auto SCkInspector_ObjectiveOwnerAuthored::Get_ObjectiveStatusForeground(const FString& InStableKey) const -> FLinearColor
{
    return CkStyle::GetToneColor(Get_ObjectiveTone(InStableKey));
}

auto SCkInspector_ObjectiveOwnerAuthored::Get_ObjectiveStatusBackground(const FString& InStableKey) const -> FLinearColor
{
    return CkStyle::GetToneDimColor(Get_ObjectiveTone(InStableKey));
}

auto SCkInspector_ObjectiveOwnerAuthored::Is_ObjectiveDiffMarked(const FString& InStableKey) const -> bool
{
    return _ObjectiveDiffMarkedKeys.Contains(InStableKey);
}

auto SCkInspector_ObjectiveOwnerAuthored::Build_ObjectiveRecords() -> bool
{
    if (NOT _ObjectivesCollection.IsValid())
    {
        const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreate(
            {
                FCkUiFieldSchema{TEXT("name"), ECkUiFieldKind::Text},
                FCkUiFieldSchema{TEXT("status"), ECkUiFieldKind::Text},
                FCkUiFieldSchema{TEXT("status-foreground"), ECkUiFieldKind::Color},
                FCkUiFieldSchema{TEXT("status-background"), ECkUiFieldKind::Color},
                FCkUiFieldSchema{TEXT("objective-diff-color"), ECkUiFieldKind::Color},
                FCkUiFieldSchema{TEXT("status-visible"), ECkUiFieldKind::Bool},
                FCkUiFieldSchema{TEXT("select-label"), ECkUiFieldKind::Text},
                FCkUiFieldSchema{TEXT("select-tooltip"), ECkUiFieldKind::Text},
                FCkUiFieldSchema{TEXT("remove-label"), ECkUiFieldKind::Text},
                FCkUiFieldSchema{TEXT("remove-visible"), ECkUiFieldKind::Bool},
                FCkUiFieldSchema{TEXT("remove-tooltip"), ECkUiFieldKind::Text},
            },
            _ObjectivesCollection);
        if (NOT CreateResult.Succeeded || NOT _ObjectivesCollection.IsValid())
        {
            _LoadError = FString::Join(CreateResult.Errors, TEXT("\n"));
            return false;
        }
    }

    const FString ProgressText = Get_ProgressText();
    _ProgressVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("Progress:"), ProgressText);
    const bool bShowEditControls = ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection());

    const bool bCaptureDiffMarks = NOT _ObjectiveDiffMarksCaptured;
    auto RetainedDiffMarkedKeys = TSet<FString>{};
    auto Records = TArray<FCkUiRecordData>{};
    for (const FCk_Handle_Objective& Objective : ck_inspector_objective_owner::GetObjectives(_Entity))
    {
        const FString Key = ck_inspector_objective_owner::GetStableKey(Objective);
        if (Key.IsEmpty()) { continue; }

        const FString Name = UCk_Utils_Objective_UE::Get_Name(Objective).ToString();
        const FString Status = ck_inspector_objective_owner::GetStatusText(Objective);
        const ECk_Tone Tone = ck_inspector_objective_owner::GetStatusTone(Objective);
        const bool bDiffMarked = bCaptureDiffMarks
            ? FCkInspector_DiffMarkScope::Is_LabelMarked(Name)
            : _ObjectiveDiffMarkedKeys.Contains(Key);
        if (bDiffMarked) { RetainedDiffMarkedKeys.Add(Key); }
        const bool bMatchesStatus = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, Name, Status);
        const bool bMatchesRemove = bShowEditControls && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, Name, TEXT("Remove"));

        auto Record = FCkUiRecordData{};
        Record.Key = Key;
        Record.Fields.Add(TEXT("name"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Name)});
        Record.Fields.Add(TEXT("status"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Status)});
        Record.Fields.Add(TEXT("status-foreground"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = CkStyle::GetToneColor(Tone)});
        Record.Fields.Add(TEXT("status-background"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = CkStyle::GetToneDimColor(Tone)});
        Record.Fields.Add(TEXT("objective-diff-color"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color,
            .Color = ck_inspector_objective_owner::DiffColor(bDiffMarked)});
        Record.Fields.Add(TEXT("status-visible"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = bMatchesStatus});
        Record.Fields.Add(TEXT("select-label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(TEXT("Select"))});
        Record.Fields.Add(TEXT("select-tooltip"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text,
            .Text = FText::FromString(TEXT("Select objective in the ECS inspector"))});
        Record.Fields.Add(TEXT("remove-label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(TEXT("Remove"))});
        Record.Fields.Add(TEXT("remove-visible"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = bMatchesRemove});
        Record.Fields.Add(TEXT("remove-tooltip"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text,
            .Text = FText::FromString(FString::Printf(TEXT("UCk_Utils_ObjectiveOwner_UE::Request_RemoveObjective(%s)"), *Name))});
        Records.Add(MoveTemp(Record));
    }

    const FCkUiLoadResult RecordsResult = _ObjectivesCollection->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded)
    {
        _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n"));
        return false;
    }
    _ObjectiveDiffMarkedKeys = MoveTemp(RetainedDiffMarkedKeys);
    _ObjectiveDiffMarksCaptured = true;
    return true;
}

auto SCkInspector_ObjectiveOwnerAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_ObjectiveOwnerAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("objective-owner-progress"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString(Widget->Get_ProgressText()) : FText::GetEmpty();
    }));
    Data.Number.Add(TEXT("objective-owner-progress-fraction"), TAttribute<float>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() ? Widget->Get_ProgressFraction() : 0.0f;
    }));
    Data.Text.Add(TEXT("objective-owner-disabled-reason"), FText::FromString(TEXT("Objective Owner is unavailable.")));
    Data.Color.Add(TEXT("objective-owner-progress-diff-color"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? ck_inspector_objective_owner::DiffColor(Widget->Is_ProgressDiffMarked()) : FLinearColor::Transparent;
    }));
    Data.Color.Add(TEXT("objective-owner-progress-fill"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable() ? CkStyle::GetToneColor(ECk_Tone::Accent) : CkStyle::GetToneColor(ECk_Tone::Neutral);
    }));
    Data.Visibility.Add(TEXT("objective-owner-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    }));
    Data.Visibility.Add(TEXT("objective-owner-progress-visible"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert() && Widget->_ProgressVisible;
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    Data.Collections.Add(TEXT("objective-owner-objectives"), _ObjectivesCollection);
    Data.ItemActions.Add(TEXT("objective-owner-select"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InStableKey)
    {
        if (const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
        { Widget->Request_SelectObjective(InStableKey); }
    }));
    Data.ItemActions.Add(TEXT("objective-owner-remove"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InStableKey)
    {
        if (const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
        { Widget->Request_RemoveObjective(InStableKey); }
    }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorObjectiveOwner.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorObjectiveOwner.ui.css")));
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

auto SCkInspector_ObjectiveOwnerAuthored::Request_SelectObjective(const FString& InStableKey) -> void
{
    auto Objective = FCk_Handle_Objective{};
    if (NOT _Active || NOT ck_inspector_objective_owner::FindObjectiveByKey(_Entity, InStableKey, Objective)) { return; }

    if (const TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel = _SelectionModel.Pin(); SelectionModel.IsValid())
    { SelectionModel->Set_SelectedEntities({ FCk_Handle{Objective} }); }
}

auto SCkInspector_ObjectiveOwnerAuthored::Request_RemoveObjective(const FString& InStableKey) -> void
{
    auto Owner = FCk_Handle_ObjectiveOwner{};
    auto Objective = FCk_Handle_Objective{};
    if (NOT _Active
        || NOT ck_inspector_objective_owner::TryGetOwner(_Entity, Owner)
        || NOT ck_inspector_objective_owner::FindObjectiveByKey(_Entity, InStableKey, Objective))
    { return; }

    UCk_Utils_ObjectiveOwner_UE::Request_RemoveObjective(Owner, FCk_Request_ObjectiveOwner_RemoveObjective{Objective}, {});
}

auto SCkInspector_ObjectiveOwnerAuthored::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }

    // Per-instance publication retains matching record identity while reconciling membership, status, and tones.
    if (NOT Build_ObjectiveRecords()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_ObjectiveOwnerAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = FCk_Handle{};
    _SelectionModel.Reset();
    _ObjectivesCollection.Reset();
    _ObjectiveDiffMarkedKeys.Reset();
    _ObjectiveDiffMarksCaptured = false;
    _View.Reset();
    _Mounted = false;
}

// --------------------------------------------------------------------------------------------------------------------

FCkInspector_ObjectiveOwner::~FCkInspector_ObjectiveOwner()
{
    OnDeactivated();
}

auto FCkInspector_ObjectiveOwner::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Objectives"));
}

auto FCkInspector_ObjectiveOwner::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Owner = FCk_Handle_ObjectiveOwner{};
    return ck_inspector_objective_owner::TryGetOwner(Entity, Owner);
}

auto FCkInspector_ObjectiveOwner::Build_NativeBody(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto Owner = FCk_Handle_ObjectiveOwner{};
    if (NOT ck_inspector_objective_owner::TryGetOwner(InEntity, Owner)) { return Builder.Build(InEntity, InFilter); }

    const FCk_Handle CapturedEntity = InEntity;
    Builder.AddMeterRow(
        FText::FromString(TEXT("Progress:")),
        TAttribute<float>::CreateLambda([CapturedEntity]() { return ck_inspector_objective_owner::GetProgressFraction(CapturedEntity); }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedEntity]() { return FText::FromString(ck_inspector_objective_owner::GetProgressText(CapturedEntity)); }));

    for (const FCk_Handle_Objective& Objective : ck_inspector_objective_owner::GetObjectives(InEntity))
    {
        const FString Name = UCk_Utils_Objective_UE::Get_Name(Objective).ToString();
        Builder.AddStatusPillRow(
            FText::FromString(Name),
            TAttribute<FText>::CreateLambda([Objective]() { return FText::FromString(ck_inspector_objective_owner::GetStatusText(Objective)); }),
            TAttribute<ECk_Tone>::CreateLambda([Objective]() { return ck_inspector_objective_owner::GetStatusTone(Objective); }),
            [CapturedSelectionModel = SelectionModel, CapturedObjective = Objective]()
            {
                auto CurrentObjective = FCk_Handle_Objective{};
                if (CapturedSelectionModel.IsValid()
                    && ck_inspector_objective_owner::TryGetObjective(CapturedObjective, CurrentObjective))
                { CapturedSelectionModel->Set_SelectedEntities({ FCk_Handle{CurrentObjective} }); }
            });

        const FString StableKey = ck_inspector_objective_owner::GetStableKey(Objective);
        Builder.AddActionRow(
            FText::FromString(Name),
            {FCkInspector_Action{
                FText::FromString(TEXT("Remove")),
                FText::FromString(FString::Printf(TEXT("UCk_Utils_ObjectiveOwner_UE::Request_RemoveObjective(%s)"), *Name)),
                [CapturedEntity, StableKey]()
                {
                    auto CurrentOwner = FCk_Handle_ObjectiveOwner{};
                    auto CurrentObjective = FCk_Handle_Objective{};
                    if (NOT ck_inspector_objective_owner::TryGetOwner(CapturedEntity, CurrentOwner)
                        || NOT ck_inspector_objective_owner::FindObjectiveByKey(CapturedEntity, StableKey, CurrentObjective))
                    { return; }
                    UCk_Utils_ObjectiveOwner_UE::Request_RemoveObjective(
                        CurrentOwner, FCk_Request_ObjectiveOwner_RemoveObjective{CurrentObjective}, {});
                },
                ECk_DebugRequest_Requirement::LocalOk}});
    }

    return Builder.Build(InEntity, InFilter);
}

auto FCkInspector_ObjectiveOwner::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return Build_Inspector(Entity, FString{});
}

auto FCkInspector_ObjectiveOwner::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity, InFilter);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_ObjectiveOwnerAuthored> Authored = SNew(SCkInspector_ObjectiveOwnerAuthored)
        .Entity(Entity)
        .Filter(InFilter)
        .SelectionModel(SelectionModel)
        .ProgressDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Progress:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_ObjectiveOwner::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_ObjectiveOwnerAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_ObjectiveOwner::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_ObjectiveOwnerAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}
