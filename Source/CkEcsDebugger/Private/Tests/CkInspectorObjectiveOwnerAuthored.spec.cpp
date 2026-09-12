#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_ObjectiveOwner.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkAttribute/ByteAttribute/CkByteAttribute_Processor.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEntityCollection/CkEntityCollection_Processor.h"
#include "CkEntityCollection/CkEntityCollection_Utils.h"
#include "CkObjective/Objective/CkObjective_Fragment.h"
#include "CkObjective/Objective/CkObjective_Processor.h"
#include "CkObjective/Objective/CkObjective_Utils.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Fragment.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Processor.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Utils.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_inspector_objective_owner_authored_test
{
    auto GetNotStartedTag() -> FGameplayTag
    { return FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Attack")}, false); }

    auto GetActiveTag() -> FGameplayTag
    { return FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Defense")}, false); }

    auto GetCompletedTag() -> FGameplayTag
    { return FGameplayTag::RequestGameplayTag(FName{TEXT("CueGym.Concurrency.Multiple")}, false); }

    auto GetFailedTag() -> FGameplayTag
    { return FGameplayTag::RequestGameplayTag(FName{TEXT("Poi.Category.Area")}, false); }

    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const TSharedRef<STextBlock> Text = StaticCastSharedRef<STextBlock>(InRoot);
            if (Text->GetText().ToString() == InText) { return true; }
        }
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        {
            const TSharedRef<SCkFlexText> Text = StaticCastSharedRef<SCkFlexText>(InRoot);
            if (Text->GetText().ToString() == InText) { return true; }
        }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; } }
        return false;
    }

    // Each authored status/action row repeats its stable objective-name text. Descend first so identical action tags
    // select the smallest containing row rather than an earlier sibling from the repeat root.
    auto FindActionForObjective(
        const TSharedRef<SWidget>& InRoot,
        const FString& InObjectiveKey,
        const FName InActionTag) -> TSharedPtr<SButton>
    {
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindActionForObjective(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InObjectiveKey, InActionTag); Found.IsValid())
            { return Found; }
        }

        if (NOT ContainsText(InRoot, InObjectiveKey)) { return nullptr; }
        TFunction<TSharedPtr<SButton>(const TSharedRef<SWidget>&)> FindTagged;
        FindTagged = [&FindTagged, InActionTag](const TSharedRef<SWidget>& InWidget) -> TSharedPtr<SButton>
        {
            if (InWidget->GetTypeAsString() == TEXT("SButton") && InWidget->GetTag() == InActionTag)
            { return StaticCastSharedRef<SButton>(InWidget); }
            FChildren* WidgetChildren = InWidget->GetChildren();
            for (int32 ChildIndex = 0; WidgetChildren != nullptr && ChildIndex < WidgetChildren->Num(); ++ChildIndex)
            {
                if (const TSharedPtr<SButton> Found = FindTagged(
                    ConstCastSharedRef<SWidget>(WidgetChildren->GetChildAt(ChildIndex))); Found.IsValid())
                { return Found; }
            }
            return nullptr;
        };
        return FindTagged(InRoot);
    }

    struct FFixture final
    {
        FCk_Handle OwnerA;
        FCk_Handle OwnerB;
        FCk_Handle_ObjectiveOwner ObjectiveOwnerA;
        FCk_Handle_ObjectiveOwner ObjectiveOwnerB;
        FCk_Handle_EntityCollection ObjectivesA;
        FCk_Handle_EntityCollection ObjectivesB;
        FCk_Handle_Objective ANotStarted;
        FCk_Handle_Objective ADuplicateNotStarted;
        FCk_Handle_Objective AActive;
        FCk_Handle_Objective ACompleted;
        FCk_Handle_Objective AFailed;
        FCk_Handle_Objective BNotStarted;
        FCk_Handle_Objective BActive;
        FCk_Handle_Objective BCompleted;
        FCk_Handle_Objective BFailed;
    };

    auto AddAuthorityAndWorld(FCk_Handle& InEntity) -> bool
    {
        if (GWorld == nullptr) { return false; }
        InEntity.Add<TWeakObjectPtr<UWorld>>(GWorld);
        UCk_Utils_Net_UE::Add(InEntity, FCk_Net_ConnectionSettings{
            ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority});
        return true;
    }

    auto CreateObjective(
        FCk_Handle& InOwner,
        const FGameplayTag InName) -> FCk_Handle_Objective
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (NOT AddAuthorityAndWorld(Entity)) { return {}; }
        auto Params = FCk_Objective_ParamsData{InName};
        Params.Set_DisplayName(FText::FromString(InName.ToString()));
        return UCk_Utils_Objective_UE::Add(Entity, Params);
    }

    auto FindObjectiveRecordKey(const FCkUiCollection& InCollection, const FString& InName) -> FString
    {
        for (const TSharedPtr<const FCkUiRecord>& Record : InCollection.GetRecords())
        {
            const FCkUiFieldValue* Name = Record.IsValid() ? Record->FindField(TEXT("name")) : nullptr;
            if (Name != nullptr && Name->Text.ToString() == InName) { return Record->GetKey(); }
        }
        return {};
    }

    auto GetRecordBool(const FCkUiCollection& InCollection, const FString& InKey, const FString& InField) -> bool
    {
        const TSharedPtr<const FCkUiRecord> Record = InCollection.FindRecord(InKey);
        const FCkUiFieldValue* Field = Record.IsValid() ? Record->FindField(InField) : nullptr;
        return Field != nullptr && Field->Kind == ECkUiFieldKind::Bool && Field->Bool;
    }

    auto RequestStatus(
        ck::FProcessor_Objective_HandleRequests& InProcessor,
        ck::FProcessor_ByteAttributeModifier_ComputeAll& InModifierProcessor,
        ck::FProcessor_ByteAttribute_RecomputeAll& InRecomputeProcessor,
        FCk_Handle_Objective& InObjective,
        const ECk_ObjectiveStatus InStatus) -> bool
    {
        if (InStatus == ECk_ObjectiveStatus::NotStarted) { return true; }
        UCk_Utils_Objective_UE::Request_Start(InObjective, FCk_Request_Objective_Start{}, {});
        InProcessor.Pump();
        InRecomputeProcessor.Pump();
        InModifierProcessor.Pump();
        InRecomputeProcessor.Pump();
        if (InStatus == ECk_ObjectiveStatus::Active)
        { return UCk_Utils_Objective_UE::Get_Status(InObjective) == ECk_ObjectiveStatus::Active; }

        if (InStatus == ECk_ObjectiveStatus::Completed)
        { UCk_Utils_Objective_UE::Request_Complete(InObjective, FCk_Request_Objective_Complete{}, {}); }
        else
        { UCk_Utils_Objective_UE::Request_Fail(InObjective, FCk_Request_Objective_Fail{}, {}); }
        InProcessor.Pump();
        InRecomputeProcessor.Pump();
        InModifierProcessor.Pump();
        InRecomputeProcessor.Pump();
        return UCk_Utils_Objective_UE::Get_Status(InObjective) == InStatus;
    }

    auto CreateFixture(ck::FEcsWorld& InWorld, FFixture& OutFixture) -> bool
    {
        const FGameplayTag ObjectivesLabel = FGameplayTag::RequestGameplayTag(
            FName{TEXT("EntityCollection.Objectives")}, false);
        if (NOT ObjectivesLabel.IsValid()) { return false; }

        OutFixture.OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        OutFixture.OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        if (NOT AddAuthorityAndWorld(OutFixture.OwnerA) || NOT AddAuthorityAndWorld(OutFixture.OwnerB)) { return false; }
        OutFixture.ObjectiveOwnerA = UCk_Utils_ObjectiveOwner_UE::Add(OutFixture.OwnerA, {});
        OutFixture.ObjectiveOwnerB = UCk_Utils_ObjectiveOwner_UE::Add(OutFixture.OwnerB, {});
        OutFixture.ObjectivesA = UCk_Utils_EntityCollection_UE::TryGet_EntityCollection(OutFixture.OwnerA, ObjectivesLabel);
        OutFixture.ObjectivesB = UCk_Utils_EntityCollection_UE::TryGet_EntityCollection(OutFixture.OwnerB, ObjectivesLabel);
        if (ck::Is_NOT_Valid(OutFixture.ObjectiveOwnerA) || ck::Is_NOT_Valid(OutFixture.ObjectiveOwnerB)
            || ck::Is_NOT_Valid(OutFixture.ObjectivesA) || ck::Is_NOT_Valid(OutFixture.ObjectivesB)) { return false; }

        const FGameplayTag NotStartedTag = GetNotStartedTag();
        const FGameplayTag ActiveTag = GetActiveTag();
        const FGameplayTag CompletedTag = GetCompletedTag();
        const FGameplayTag FailedTag = GetFailedTag();
        if (NOT NotStartedTag.IsValid() || NOT ActiveTag.IsValid() || NOT CompletedTag.IsValid() || NOT FailedTag.IsValid())
        { return false; }

        OutFixture.ANotStarted = CreateObjective(OutFixture.OwnerA, NotStartedTag);
        OutFixture.ADuplicateNotStarted = CreateObjective(OutFixture.OwnerA, NotStartedTag);
        OutFixture.AActive = CreateObjective(OutFixture.OwnerA, ActiveTag);
        OutFixture.ACompleted = CreateObjective(OutFixture.OwnerA, CompletedTag);
        OutFixture.AFailed = CreateObjective(OutFixture.OwnerA, FailedTag);
        OutFixture.BNotStarted = CreateObjective(OutFixture.OwnerB, NotStartedTag);
        OutFixture.BActive = CreateObjective(OutFixture.OwnerB, ActiveTag);
        OutFixture.BCompleted = CreateObjective(OutFixture.OwnerB, CompletedTag);
        OutFixture.BFailed = CreateObjective(OutFixture.OwnerB, FailedTag);
        const TArray<FCk_Handle_Objective> AllObjectives{
            OutFixture.ANotStarted, OutFixture.ADuplicateNotStarted, OutFixture.AActive, OutFixture.ACompleted, OutFixture.AFailed,
            OutFixture.BNotStarted, OutFixture.BActive, OutFixture.BCompleted, OutFixture.BFailed};
        for (const FCk_Handle_Objective& Objective : AllObjectives)
        { if (ck::Is_NOT_Valid(Objective)) { return false; } }

        auto ObjectiveRequests = ck::FProcessor_Objective_HandleRequests{InWorld.Get_Registry()};
        auto ObjectiveModifiers = ck::FProcessor_ByteAttributeModifier_ComputeAll{InWorld.Get_Registry()};
        auto ObjectiveRecompute = ck::FProcessor_ByteAttribute_RecomputeAll{InWorld.Get_Registry()};
        if (NOT RequestStatus(ObjectiveRequests, ObjectiveModifiers, ObjectiveRecompute,
                OutFixture.AActive, ECk_ObjectiveStatus::Active)
            || NOT RequestStatus(ObjectiveRequests, ObjectiveModifiers, ObjectiveRecompute,
                OutFixture.ACompleted, ECk_ObjectiveStatus::Completed)
            || NOT RequestStatus(ObjectiveRequests, ObjectiveModifiers, ObjectiveRecompute,
                OutFixture.AFailed, ECk_ObjectiveStatus::Failed)) { return false; }

        UCk_Utils_EntityCollection_UE::Request_AddEntities(OutFixture.ObjectivesA,
            FCk_Request_EntityCollection_AddEntities{TArray<FCk_Handle>{
                OutFixture.ANotStarted, OutFixture.ADuplicateNotStarted,
                OutFixture.AActive, OutFixture.ACompleted, OutFixture.AFailed}}, {});
        UCk_Utils_EntityCollection_UE::Request_AddEntities(OutFixture.ObjectivesB,
            FCk_Request_EntityCollection_AddEntities{TArray<FCk_Handle>{
                OutFixture.BNotStarted, OutFixture.BActive, OutFixture.BCompleted, OutFixture.BFailed}}, {});
        ck::FProcessor_EntityCollection_HandleRequests{InWorld.Get_Registry()}.Pump();

        return UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(OutFixture.ObjectiveOwnerA).Num() == 5
            && UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(OutFixture.ObjectiveOwnerB).Num() == 4;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorObjectiveOwnerAuthored,
    "Ck.UiAuthoring.EcsDebugger.ObjectiveOwnerInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorObjectiveOwnerAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_objective_owner_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto InvalidInspector = FCkInspector_ObjectiveOwner{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build still mounts the authored ObjectiveOwner shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_ObjectiveOwnerAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_ObjectiveOwnerAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(InvalidRendered);
    const TWeakPtr<FCkUiView> InvalidView = InvalidAuthored->Get_View();
    TestTrue(TEXT("default-invalid ObjectiveOwner projection is mounted and completely fail-closed"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && InvalidAuthored->Get_ProgressText() == TEXT("--")
            && InvalidAuthored->Get_ObjectiveCount() == 0);
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid authored ObjectiveOwner shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted() && NOT InvalidView.IsValid());

    auto World = ck::FEcsWorld{};
    auto Fixture = FFixture{};
    if (NOT TestTrue(TEXT("fixture composes two independent owners, objectives, and collections through public APIs"),
        CreateFixture(World, Fixture))) { return false; }

    auto Inspector = FCkInspector_ObjectiveOwner{};
    const TSharedPtr<FCkDebuggerModel_EntitySelection> Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    Inspector.Set_SelectionModel(Selection);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(Fixture.OwnerA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(Fixture.OwnerB);
    if (NOT TestEqual(TEXT("A mounts the authored ObjectiveOwner inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_ObjectiveOwnerAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored ObjectiveOwner inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_ObjectiveOwnerAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_ObjectiveOwnerAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(RenderedA);
    const TSharedRef<SCkInspector_ObjectiveOwnerAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TSharedPtr<FCkUiCollection> ObjectivesA = AuthoredA->Get_ObjectivesCollection();
    TSharedPtr<FCkUiCollection> ObjectivesB = AuthoredB->Get_ObjectivesCollection();
    if (NOT TestTrue(TEXT("each production build owns independent accepted ObjectiveOwner view and repeat state"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ObjectivesA.IsValid() && ObjectivesB.IsValid() && ViewA != ViewB && ObjectivesA != ObjectivesB
            && ObjectivesA->GetRecords().Num() == 5 && ObjectivesB->GetRecords().Num() == 4
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    { AddError(AuthoredA->Get_LoadError()); AddError(AuthoredB->Get_LoadError()); return false; }

    const FString NotStartedName = GetNotStartedTag().ToString();
    const FString ActiveName = GetActiveTag().ToString();
    const FString CompletedName = GetCompletedTag().ToString();
    const FString FailedName = GetFailedTag().ToString();
    const FString ActiveKey = FindObjectiveRecordKey(*ObjectivesA, ActiveName);
    const FString CompletedKey = FindObjectiveRecordKey(*ObjectivesA, CompletedName);
    const FString FailedKey = FindObjectiveRecordKey(*ObjectivesA, FailedName);
    auto DuplicateNameKeys = TSet<FString>{};
    for (const TSharedPtr<const FCkUiRecord>& Record : ObjectivesA->GetRecords())
    {
        const FCkUiFieldValue* Name = Record.IsValid() ? Record->FindField(TEXT("name")) : nullptr;
        if (Name != nullptr && Name->Text.ToString() == NotStartedName) { DuplicateNameKeys.Add(Record->GetKey()); }
    }
    if (NOT TestTrue(TEXT("authored ObjectiveOwner records expose collision-safe stable keys keyed by name"),
        NOT ActiveKey.IsEmpty() && NOT CompletedKey.IsEmpty() && NOT FailedKey.IsEmpty()
            && ActiveKey != ActiveName && CompletedKey != CompletedName && FailedKey != FailedName
            && DuplicateNameKeys.Num() == 2)) { return false; }
    TestTrue(TEXT("authored owner projects completed/total and every objective status with exact tones"),
        AuthoredA->Get_IsAvailable() && AuthoredB->Get_IsAvailable()
            && AuthoredA->Get_ProgressText() == TEXT("1 / 5") && AuthoredA->Get_ProgressFraction() == 0.2f
            && AuthoredB->Get_ProgressText() == TEXT("0 / 4") && AuthoredB->Get_ProgressFraction() == 0.0f
            && AuthoredA->Get_ObjectiveStatusText(ActiveKey) == TEXT("Active")
            && AuthoredA->Get_ObjectiveStatusText(CompletedKey) == TEXT("Completed")
            && AuthoredA->Get_ObjectiveStatusText(FailedKey) == TEXT("Failed")
            && AuthoredA->Get_ObjectiveStatusForeground(ActiveKey) == CkStyle::GetToneColor(ECk_Tone::Info)
            && AuthoredA->Get_ObjectiveStatusForeground(CompletedKey) == CkStyle::GetToneColor(ECk_Tone::Ok)
            && AuthoredA->Get_ObjectiveStatusForeground(FailedKey) == CkStyle::GetToneColor(ECk_Tone::Err)
            && AuthoredA->Get_ObjectiveStatusBackground(ActiveKey) == CkStyle::GetToneDimColor(ECk_Tone::Info)
            && AuthoredA->Get_ObjectiveStatusBackground(CompletedKey) == CkStyle::GetToneDimColor(ECk_Tone::Ok)
            && AuthoredA->Get_ObjectiveStatusBackground(FailedKey) == CkStyle::GetToneDimColor(ECk_Tone::Err)
            && ContainsText(RenderedA, CompletedName));

    const TSharedPtr<const FCkUiRecord> StableCompleted = ObjectivesA->FindRecord(CompletedKey);
    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.OwnerA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.OwnerB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native capture remains the exact ObjectiveOwner comparison authority"),
        RowsA.FindRef(TEXT("Progress:")) == TEXT("1 / 5") && RowsB.FindRef(TEXT("Progress:")) == TEXT("0 / 4")
            && Differing.Num() == 1 && Differing.Contains(TEXT("Progress:")));
    TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(Inspector.Build_Inspector(Fixture.OwnerA));
    }
    TestTrue(TEXT("only the authored progress row receives the exact native diff mark"),
        DiffAuthored.IsValid() && DiffAuthored->Is_ProgressDiffMarked()
            && NOT DiffAuthored->Is_ObjectiveDiffMarked(ActiveKey)
            && NOT DiffAuthored->Is_ObjectiveDiffMarked(CompletedKey));
    const TSet<FString> ObjectiveDifference{ActiveName};
    TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> ObjectiveDiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&ObjectiveDifference};
        ObjectiveDiffAuthored = StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(Inspector.Build_Inspector(Fixture.OwnerA));
    }
    ObjectiveDiffAuthored->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    TestTrue(TEXT("per-objective diff marks retain their stable-key verdict after the compose-time scope ends"),
        ObjectiveDiffAuthored->Is_ObjectiveDiffMarked(ActiveKey)
            && NOT ObjectiveDiffAuthored->Is_ObjectiveDiffMarked(CompletedKey));

    const TSharedPtr<SButton> HeldSelect = FindActionForObjective(RenderedA, ActiveName, TEXT("objective-owner-select"));
    const TSharedPtr<SButton> HeldRemove = FindActionForObjective(RenderedA, ActiveName, TEXT("objective-owner-remove"));
    if (NOT TestTrue(TEXT("authored repeat mounts physical select and Remove actions for the requested objective"),
        HeldSelect.IsValid() && HeldRemove.IsValid() && HeldSelect->IsEnabled() && HeldRemove->IsEnabled())) { return false; }
    HeldSelect->SimulateClick();
    TestTrue(TEXT("physical select action routes to the configured selection model"),
        Selection->Get_SelectedEntities().Num() == 1 && Selection->Get_SelectedEntities()[0] == FCk_Handle{Fixture.AActive});

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed ObjectiveOwner HTML and CSS resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorObjectiveOwner.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorObjectiveOwner.ui.css"))))) { return false; }
    TestTrue(TEXT("resource owns the exact progress, repeated objectives, statuses, and actions"),
        Markup.Contains(TEXT("fraction-bind=\"objective-owner-progress-fraction\""))
            && Markup.Contains(TEXT("objective-owner-objectives")) && Markup.Contains(TEXT("label-field=\"status\""))
            && Markup.Contains(TEXT("foreground-field=\"status-foreground\""))
            && Markup.Contains(TEXT("background-field=\"status-background\""))
            && Markup.Contains(TEXT("item-action=\"objective-owner-select\""))
            && Markup.Contains(TEXT("item-action=\"objective-owner-remove\"")) && Markup.Contains(TEXT("Remove")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible ObjectiveOwner reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("ObjectiveOwner A compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload preserves view, repeated record, and physical action identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore
            && ObjectivesA->FindRecord(CompletedKey) == StableCompleted
            && FindActionForObjective(RenderedA, ActiveName, TEXT("objective-owner-remove")) == HeldRemove);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing ObjectiveOwner meter binding is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("fraction-bind=\"objective-owner-progress-fraction\""),
            TEXT("fraction-bind=\"objective-owner-missing-progress-fraction\"")),
        Stylesheet, TEXT("ObjectiveOwner B missing meter binding")).Succeeded);
    TestFalse(TEXT("missing ObjectiveOwner status binding is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("label-field=\"status\""), TEXT("label-field=\"missing-status\"")),
        Stylesheet, TEXT("ObjectiveOwner B missing status binding")).Succeeded);
    TestFalse(TEXT("missing ObjectiveOwner action binding is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("item-action=\"objective-owner-remove\""), TEXT("item-action=\"objective-owner-missing-remove\"")),
        Stylesheet, TEXT("ObjectiveOwner B missing action binding")).Succeeded);
    TestTrue(TEXT("every rejected candidate retains B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    const TSharedRef<SCkInspector_ObjectiveOwnerAuthored> Filtered =
        StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(Inspector.Build_Inspector(Fixture.OwnerA, CompletedName));
    const TSharedRef<SCkInspector_ObjectiveOwnerAuthored> NoMatch =
        StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(Inspector.Build_Inspector(Fixture.OwnerA, TEXT("objectiveowner-missing-filter")));
    const TSharedRef<SCkInspector_ObjectiveOwnerAuthored> RemoveOnly =
        StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(Inspector.Build_Inspector(Fixture.OwnerA, TEXT("Remove")));
    TestTrue(TEXT("filter independently matches status and Remove placements and collapses unmatched ObjectiveOwner rows"),
        Filtered->Get_ObjectivesCollection().IsValid() && NoMatch->Get_ObjectivesCollection().IsValid()
            && RemoveOnly->Get_ObjectivesCollection().IsValid()
            && GetRecordBool(*Filtered->Get_ObjectivesCollection(), CompletedKey, TEXT("status-visible"))
            && GetRecordBool(*Filtered->Get_ObjectivesCollection(), CompletedKey, TEXT("remove-visible"))
            && NOT GetRecordBool(*Filtered->Get_ObjectivesCollection(), ActiveKey, TEXT("status-visible"))
            && NOT GetRecordBool(*NoMatch->Get_ObjectivesCollection(), CompletedKey, TEXT("status-visible"))
            && NOT GetRecordBool(*NoMatch->Get_ObjectivesCollection(), CompletedKey, TEXT("remove-visible"))
            && NOT GetRecordBool(*RemoveOnly->Get_ObjectivesCollection(), CompletedKey, TEXT("status-visible"))
            && GetRecordBool(*RemoveOnly->Get_ObjectivesCollection(), CompletedKey, TEXT("remove-visible")));
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Hidden;
    const TSharedRef<SCkInspector_ObjectiveOwnerAuthored> HiddenEdits =
        StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(Inspector.Build_Inspector(Fixture.OwnerA, CompletedName));
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;
    TestTrue(TEXT("Hidden edit-controls axis suppresses Remove placement without suppressing the matching status row"),
        HiddenEdits->Get_ObjectivesCollection().IsValid()
            && GetRecordBool(*HiddenEdits->Get_ObjectivesCollection(), CompletedKey, TEXT("status-visible"))
            && NOT GetRecordBool(*HiddenEdits->Get_ObjectivesCollection(), CompletedKey, TEXT("remove-visible")));

    HeldRemove->SimulateClick();
    if (NOT TestTrue(TEXT("physical Remove queues one public ObjectiveOwner request before deferred pumps"),
        Fixture.OwnerA.Has<ck::FFragment_ObjectiveOwner_Requests>()
            && Fixture.OwnerA.Get<ck::FFragment_ObjectiveOwner_Requests>().Get_Requests().Num() == 1)) { return false; }
    ck::FProcessor_ObjectiveOwner_HandleRequests{World.Get_Registry()}.Pump();
    ck::FProcessor_EntityCollection_HandleRequests{World.Get_Registry()}.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    TestTrue(TEXT("deferred ObjectiveOwner then EntityCollection pumps remove only the selected member"),
        UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(Fixture.ObjectiveOwnerA).Num() == 4
            && NOT ObjectivesA->FindRecord(ActiveKey).IsValid()
            && ObjectivesA->FindRecord(CompletedKey) == StableCompleted
            && AuthoredA->Get_ProgressText() == TEXT("1 / 4"));

    TSharedPtr<SCkInspector_ObjectiveOwnerAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_ObjectiveOwner>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_ObjectiveOwnerAuthored>(
            DestructorInspector->Build_Inspector(Fixture.OwnerB));
    }
    TestTrue(TEXT("ObjectiveOwner inspector destruction makes its retained authored build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());

    TestTrue(TEXT("objective Current partial teardown leaves the owner live"),
        Fixture.BCompleted.Try_Remove<ck::FFragment_Objective_Current>() && ck::IsValid(Fixture.OwnerB));
    const TSharedPtr<SButton> HeldPartialObjectiveRemove =
        FindActionForObjective(RenderedB, CompletedName, TEXT("objective-owner-remove"));
    AuthoredB->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    TestTrue(TEXT("partial objective data fails its authored status and tone closed"),
        AuthoredB->Get_ObjectiveStatusText(CompletedKey) == TEXT("--")
            && AuthoredB->Get_ObjectiveStatusForeground(CompletedKey) == CkStyle::GetToneColor(ECk_Tone::Neutral)
            && AuthoredB->Get_ObjectiveStatusBackground(CompletedKey) == CkStyle::GetToneDimColor(ECk_Tone::Neutral));
    if (NOT TestTrue(TEXT("held ObjectiveOwner Remove control survives long enough to exercise stale-objective routing"),
        HeldPartialObjectiveRemove.IsValid())) { return false; }
    HeldPartialObjectiveRemove->SimulateClick();
    TestFalse(TEXT("held ObjectiveOwner Remove action is inert after objective Current teardown"),
        Fixture.OwnerB.Has<ck::FFragment_ObjectiveOwner_Requests>());

    Selection->Set_SelectedEntities({FCk_Handle{Fixture.BNotStarted}});
    TestTrue(TEXT("owner Current partial teardown leaves the owner entity live"),
        Fixture.OwnerA.Try_Remove<ck::FFragment_ObjectiveOwner_Current>()
            && ck::IsValid(Fixture.OwnerA) && NOT Inspector.CanInspect(Fixture.OwnerA));
    HeldRemove->SlatePrepass();
    TestFalse(TEXT("partial owner data makes the mounted authored projection unavailable"), AuthoredA->Get_IsAvailable());
    TestEqual(TEXT("partial owner data fails authored progress text closed"), AuthoredA->Get_ProgressText(), FString{TEXT("--")});
    TestEqual(TEXT("partial owner data fails authored objective count closed"), AuthoredA->Get_ObjectiveCount(), 0);
    TestFalse(TEXT("partial owner data disables the held Remove control"), HeldRemove->IsEnabled());
    HeldSelect->SimulateClick();
    TestTrue(TEXT("held Select control cannot change selection after owner Current teardown"),
        Selection->Get_SelectedEntities().Num() == 1
            && Selection->Get_SelectedEntities()[0] == FCk_Handle{Fixture.BNotStarted});
    HeldRemove->SimulateClick();
    TestFalse(TEXT("held ObjectiveOwner Remove action is inert after owner Current teardown"),
        Fixture.OwnerA.Has<ck::FFragment_ObjectiveOwner_Requests>());

    Inspector.OnDeactivated();
    TestTrue(TEXT("ObjectiveOwner deactivation releases every retained view and repeat"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert() && ObjectiveDiffAuthored->Is_Inert()
            && Filtered->Is_Inert() && NoMatch->Is_Inert() && RemoveOnly->Is_Inert() && HiddenEdits->Is_Inert()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid()
            && NOT AuthoredA->Get_ObjectivesCollection().IsValid() && NOT AuthoredB->Get_ObjectivesCollection().IsValid());
    HeldSelect->SimulateClick();
    HeldRemove->SimulateClick();
    TestTrue(TEXT("held ObjectiveOwner controls remain inert after inspector release"),
        Selection->Get_SelectedEntities().Num() == 1 && NOT Fixture.OwnerA.Has<ck::FFragment_ObjectiveOwner_Requests>());
    return true;
}

#endif
