#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_ByteAttributes.h"
#include "CkEcsDebugger/Inspectors/CkInspectorAttributeRows.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkAttribute/ByteAttribute/CkByteAttribute_Fragment.h"
#include "CkAttribute/ByteAttribute/CkByteAttribute_Fragment_Data.h"
#include "CkAttribute/ByteAttribute/CkByteAttribute_Utils.h"
#include "CkAttribute/FloatAttribute/CkFloatAttribute_Fragment_Data.h"
#include "CkAnimation/AnimPlan/CkAnimPlan_Fragment_Data.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_byte_attributes_authored_test
{
    auto FindTagged(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTagged(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableTextBox")
            || InRoot->GetTypeAsString() == TEXT("SCkUiTextInputBox"))
        { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindEditor(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SEditableTextBox>
    {
        const TSharedPtr<SWidget> Host = FindTagged(InRoot, InTag);
        return Host.IsValid() ? FindEditor(Host.ToSharedRef()) : nullptr;
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if ((InRoot->GetTypeAsString() == TEXT("SButton") || InRoot->GetTypeAsString() == TEXT("SCkUiStyledButton"))
            && InRoot->GetTag() == InTag) { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto Commit(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InInput, const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InInput, EFocusCause::SetDirectly);
        TickSlate(InSlate);
        InInput->SetText(FText::FromString(InText));
        const bool bHandled = InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
        TickSlate(InSlate);
        return bHandled;
    }

    auto FindKey(const FCkUiCollection& InRecords, const FString& InName) -> FString
    {
        for (const TSharedPtr<const FCkUiRecord>& Record : InRecords.GetRecords())
        {
            const FCkUiFieldValue* Name = Record.IsValid() ? Record->FindField(TEXT("name")) : nullptr;
            if (Name != nullptr && Name->Kind == ECkUiFieldKind::Text && Name->Text.ToString() == InName)
            { return Record->GetKey(); }
        }
        return {};
    }

    auto GetInteger(const FCkUiCollection& InRecords, const FString& InKey, const FString& InField) -> int32
    {
        const TSharedPtr<const FCkUiRecord> Record = InRecords.FindRecord(InKey);
        const FCkUiFieldValue* Value = Record.IsValid() ? Record->FindField(InField) : nullptr;
        return Value != nullptr && Value->Kind == ECkUiFieldKind::Integer ? Value->Integer : INDEX_NONE;
    }

    auto GetNumber(const FCkUiCollection& InRecords, const FString& InKey, const FString& InField) -> float
    {
        const TSharedPtr<const FCkUiRecord> Record = InRecords.FindRecord(InKey);
        const FCkUiFieldValue* Value = Record.IsValid() ? Record->FindField(InField) : nullptr;
        return Value != nullptr && Value->Kind == ECkUiFieldKind::Number ? Value->Number : -1.0f;
    }

    auto CountVisible(const FCkUiCollection& InRecords, const FString& InField) -> int32
    {
        auto Count = 0;
        for (const TSharedPtr<const FCkUiRecord>& Record : InRecords.GetRecords())
        {
            const FCkUiFieldValue* Value = Record.IsValid() ? Record->FindField(InField) : nullptr;
            Count += Value != nullptr && Value->Kind == ECkUiFieldKind::Bool && Value->Bool ? 1 : 0;
        }
        return Count;
    }

    auto IsAccent(
        const FCkUiCollection& InRecords,
        const FString& InKey,
        const FString& InField) -> bool
    {
        const TSharedPtr<const FCkUiRecord> Record = InRecords.FindRecord(InKey);
        const FCkUiFieldValue* Value = Record.IsValid() ? Record->FindField(InField) : nullptr;
        return Value != nullptr
            && Value->Kind == ECkUiFieldKind::Color
            && Value->Color.Equals(CkStyle::Accent());
    }

    auto FindUnlabeledModifier(
        FCk_Handle_ByteAttribute InAttribute,
        const ECk_MinMaxCurrent InComponent) -> FCk_Handle_ByteAttributeModifier
    {
        auto Result = FCk_Handle_ByteAttributeModifier{};
        UCk_Utils_ByteAttributeModifier_UE::ForEach(
            InAttribute,
            [&Result](FCk_Handle_ByteAttributeModifier InModifier)
            {
                if (ck::IsValid(InModifier)
                    && NOT ck_inspector_attribute_rows::Get_ModifierLabel(InModifier).IsValid())
                { Result = InModifier; }
            },
            InComponent);
        return Result;
    }

    struct FFixture final
    {
        FCk_Handle OwnerA;
        FCk_Handle OwnerB;
        FCk_Handle_ByteAttribute Bounded;
        FCk_Handle_ByteAttribute BoundedB;
        FCk_Handle_ByteAttribute MaxOnly;
        FCk_Handle_ByteAttributeModifier CurrentModifier;
        FCk_Handle_ByteAttributeModifier MinModifier;
        FCk_Handle_ByteAttributeModifier MaxModifier;
    };

    auto MakeParams(const FGameplayTag InName, const uint8 InBase, const ECk_MinMax InMinMax,
        const uint8 InMin = 0, const uint8 InMax = 0) -> FCk_Fragment_ByteAttribute_ParamsData
    {
        auto Params = FCk_Fragment_ByteAttribute_ParamsData{InName, InBase};
        Params.Set_MinMax(InMinMax);
        Params.Set_MinValue(InMin);
        Params.Set_MaxValue(InMax);
        return Params;
    }

    auto CreateFixture(ck::FEcsWorld& InWorld, FFixture& Out) -> bool
    {
        Out.OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        Out.OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        if (ck::Is_NOT_Valid(Out.OwnerA) || ck::Is_NOT_Valid(Out.OwnerB) || GWorld == nullptr) { return false; }
        Out.OwnerA.Add<TWeakObjectPtr<UWorld>>(GWorld);
        Out.OwnerB.Add<TWeakObjectPtr<UWorld>>(GWorld);
        Out.Bounded = UCk_Utils_ByteAttribute_UE::Add(Out.OwnerA,
            MakeParams(TAG_Label_ByteAttribute.GetTag(), 30, ECk_MinMax::MinMax, 10, 90),
            ECk_Replication::DoesNotReplicate);
        Out.MaxOnly = UCk_Utils_ByteAttribute_UE::Add(Out.OwnerA,
            MakeParams(TAG_Label_FloatAttribute.GetTag(), 80, ECk_MinMax::Max, 0, 200),
            ECk_Replication::DoesNotReplicate);
        Out.BoundedB = UCk_Utils_ByteAttribute_UE::Add(Out.OwnerB,
            MakeParams(TAG_Label_ByteAttribute.GetTag(), 60, ECk_MinMax::MinMax, 10, 90),
            ECk_Replication::DoesNotReplicate);
        auto ParamsCurrent = FCk_Fragment_ByteAttributeModifier_ParamsData{7, ECk_MinMaxCurrent::Current};
        auto ParamsMin = FCk_Fragment_ByteAttributeModifier_ParamsData{4, ECk_MinMaxCurrent::Min};
        auto ParamsMax = FCk_Fragment_ByteAttributeModifier_ParamsData{5, ECk_MinMaxCurrent::Max};
        Out.CurrentModifier = UCk_Utils_ByteAttributeModifier_UE::Add_Revocable(Out.Bounded,
            TAG_Label_AnimPlan_Goal.GetTag(), ECk_AttributeModifier_Operation::Add, ParamsCurrent);
        Out.MinModifier = UCk_Utils_ByteAttributeModifier_UE::Add_Revocable(Out.Bounded,
            Tag_Label_AnimPlan_Cluster.GetTag(), ECk_AttributeModifier_Operation::Add, ParamsMin);
        Out.MaxModifier = UCk_Utils_ByteAttributeModifier_UE::Add_Revocable(Out.Bounded,
            TAG_Label_AnimPlan_State.GetTag(), ECk_AttributeModifier_Operation::Add, ParamsMax);
        return ck::IsValid(Out.Bounded) && ck::IsValid(Out.BoundedB) && ck::IsValid(Out.MaxOnly)
            && ck::IsValid(Out.CurrentModifier) && ck::IsValid(Out.MinModifier)
            && ck::IsValid(Out.MaxModifier);
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope()
        {
            Slate.ClearUserFocus(0, EFocusCause::SetDirectly);
            if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
        }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorByteAttributesAuthored,
    "Ck.UiAuthoring.EcsDebugger.ByteAttributesInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorByteAttributesAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_byte_attributes_authored_test;
    if (NOT FSlateApplication::IsInitialized()) { AddError(TEXT("Byte Attributes authored test requires Slate.")); return false; }

    UCkDebuggerStyleSettings* Style = UCkDebuggerStyleSettings::Get_Mutable();
    if (Style == nullptr) { AddError(TEXT("Byte Attributes authored test requires debugger style settings.")); return false; }
    const auto PreviousStyle = Style->Selection.EditControlStyle;
    ON_SCOPE_EXIT { Style->Selection.EditControlStyle = PreviousStyle; };
    Style->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto Fixture = FFixture{};
    if (NOT TestTrue(
        TEXT("public byte utility fixture creates bounded, max-only, and revocable modifiers"),
        CreateFixture(World, Fixture)))
    { return false; }

    auto Inspector = FCkInspector_ByteAttributes{};
    const TSharedRef<FCkDebuggerModel_EntitySelection> Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    Inspector.Set_SelectionModel(Selection);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(Fixture.OwnerA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(Fixture.OwnerB);
    if (NOT TestEqual(TEXT("A mounts authored Byte Attributes"), RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_ByteAttributesAuthored")})
        || NOT TestEqual(TEXT("B mounts independent authored Byte Attributes"), RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_ByteAttributesAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_ByteAttributesAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_ByteAttributesAuthored>(RenderedA);
    const TSharedRef<SCkInspector_ByteAttributesAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_ByteAttributesAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TSharedPtr<FCkUiCollection> RecordsA = AuthoredA->Get_RecordsCollection();
    if (NOT TestTrue(TEXT("builds retain independent views and collections"), ViewA.IsValid() && ViewB.IsValid()
        && RecordsA.IsValid() && RecordsA != AuthoredB->Get_RecordsCollection() && ViewA != ViewB)) { return false; }

    const FString BoundedName = TAG_Label_ByteAttribute.GetTag().ToString();
    const FString MaxOnlyName = TAG_Label_FloatAttribute.GetTag().ToString();
    const FString BoundedKey = FindKey(*RecordsA, BoundedName);
    const FString MaxOnlyKey = FindKey(*RecordsA, MaxOnlyName);
    if (NOT TestTrue(TEXT("retained records preserve stable public attribute identities"),
        NOT BoundedKey.IsEmpty() && NOT MaxOnlyKey.IsEmpty()))
    { return false; }
    TestTrue(TEXT("MinMax meter uses nonzero minimum in its fraction"), FMath::IsNearlyEqual(GetNumber(*RecordsA, BoundedKey, TEXT("fraction")), 0.25f));
    TestTrue(TEXT("max-only branch remains plain and projects Max"), GetInteger(*RecordsA, MaxOnlyKey, TEXT("max")) == 200);
    TestEqual(TEXT("one Clear All is authored per populated component"),
        CountVisible(*RecordsA, TEXT("clear-visible")), 3);

    auto NativeRowsA = TMap<FString, FString>{};
    auto NativeRowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture{}; Inspector.Build_Inspector(Fixture.OwnerA); NativeRowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture{}; Inspector.Build_Inspector(Fixture.OwnerB); NativeRowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels(
        {NativeRowsA, NativeRowsB});
    const FString NativeComponentLabel = FString::Printf(TEXT("  ↳ %s component:"), *BoundedName);
    const FString NativeOverrideLabel = FString::Printf(TEXT("  ↳ %s override:"), *BoundedName);
    TestTrue(TEXT("native capture remains comparison and filter authority"), NativeRowsA.Contains(BoundedName)
        && NativeRowsA.Contains(MaxOnlyName) && NativeRowsA.Contains(NativeComponentLabel)
        && NativeRowsA.Contains(NativeOverrideLabel) && Differing.Contains(BoundedName)
        && Differing.Contains(NativeOverrideLabel));

    TSharedPtr<SCkInspector_ByteAttributesAuthored> DiffAuthored;
    {
        const FCkInspector_DiffMarkScope DiffScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_ByteAttributesAuthored>(
            Inspector.Build_Inspector(Fixture.OwnerA));
    }
    const TSharedPtr<FCkUiCollection> DiffRecords = DiffAuthored.IsValid()
        ? DiffAuthored->Get_RecordsCollection() : nullptr;
    TestTrue(TEXT("authored primary and expanded override labels consume native diff authority"),
        DiffRecords.IsValid() && IsAccent(*DiffRecords, BoundedKey, TEXT("attribute-diff-color"))
        && IsAccent(*DiffRecords, BoundedKey, TEXT("override-diff-color")));

    const TSharedPtr<SCkUiRepeat> Repeat = ViewA->GetRepeat(TEXT("byte-attribute-records"));
    TSharedPtr<SWidget> BoundedItem = Repeat.IsValid() ? Repeat->GetItemWidget(BoundedKey) : nullptr;
    if (NOT TestTrue(TEXT("authored repeat mounts the stable bounded record"), BoundedItem.IsValid())) { return false; }
    const TSharedPtr<SWidget> CurrentHost = FindTagged(
        BoundedItem.ToSharedRef(), TEXT("byte-attribute-current-input"));
    const TSharedPtr<SWidget> MinHost = FindTagged(
        BoundedItem.ToSharedRef(), TEXT("byte-attribute-min-input"));
    const TSharedPtr<SWidget> MaxHost = FindTagged(
        BoundedItem.ToSharedRef(), TEXT("byte-attribute-max-input"));
    auto CurrentInput = CurrentHost.IsValid() ? FindEditor(CurrentHost.ToSharedRef()) : nullptr;
    auto MinInput = MinHost.IsValid() ? FindEditor(MinHost.ToSharedRef()) : nullptr;
    auto MaxInput = MaxHost.IsValid() ? FindEditor(MaxHost.ToSharedRef()) : nullptr;
    const TSharedPtr<SButton> SelectionButton = FindButton(BoundedItem.ToSharedRef(), TEXT("byte-attribute-name"));
    auto bHasPhysicalControls = true;
    bHasPhysicalControls &= TestTrue(TEXT("HTML owns the Current int32-input host"), CurrentHost.IsValid());
    bHasPhysicalControls &= TestTrue(TEXT("HTML owns the Min int32-input host"), MinHost.IsValid());
    bHasPhysicalControls &= TestTrue(TEXT("HTML owns the Max int32-input host"), MaxHost.IsValid());
    bHasPhysicalControls &= TestTrue(TEXT("Current int32-input owns an editable leaf"), CurrentInput.IsValid());
    bHasPhysicalControls &= TestTrue(TEXT("Min int32-input owns an editable leaf"), MinInput.IsValid());
    bHasPhysicalControls &= TestTrue(TEXT("Max int32-input owns an editable leaf"), MaxInput.IsValid());
    bHasPhysicalControls &= TestTrue(
        TEXT("HTML owns the physical attribute selection button"), SelectionButton.IsValid());
    if (NOT bHasPhysicalControls) { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope Scope{Slate};
    Scope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{620.0f, 480.0f})
        .CreateTitleBar(false).HasCloseButton(false)[RenderedA];
    Slate.AddWindow(Scope.Window.ToSharedRef(), true);
    TickSlate(Slate);
    SelectionButton->SimulateClick();
    TestTrue(TEXT("physical attribute selection targets the typed attribute"),
        Selection->Get_PrimarySelection() == FCk_Handle{Fixture.Bounded});
    if (NOT TestTrue(TEXT("physical Current commit dispatches"),
        Commit(Slate, CurrentInput.ToSharedRef(), TEXT("-1"))))
    { return false; }
    const FCk_Handle_ByteAttributeModifier CurrentOverride = FindUnlabeledModifier(
        Fixture.Bounded, ECk_MinMaxCurrent::Current);
    TestTrue(TEXT("negative Current commit admits a zero override rather than wrapping"),
        ck::IsValid(CurrentOverride)
        && UCk_Utils_ByteAttributeModifier_UE::Get_Delta(
            CurrentOverride, ECk_MinMaxCurrent::Current) == 0);
    if (NOT TestTrue(TEXT("physical Min commit dispatches"), Commit(Slate, MinInput.ToSharedRef(), TEXT("256")))
        || NOT TestTrue(TEXT("physical Max commit dispatches"), Commit(Slate, MaxInput.ToSharedRef(), TEXT("256")))) { return false; }
    const FCk_Handle_ByteAttributeModifier MinOverride = FindUnlabeledModifier(
        Fixture.Bounded, ECk_MinMaxCurrent::Min);
    const FCk_Handle_ByteAttributeModifier MaxOverride = FindUnlabeledModifier(
        Fixture.Bounded, ECk_MinMaxCurrent::Max);
    TestTrue(TEXT("high typed commits admit 255 overrides rather than wrapping"),
        ck::IsValid(MinOverride) && ck::IsValid(MaxOverride)
        && UCk_Utils_ByteAttributeModifier_UE::Get_Delta(MinOverride, ECk_MinMaxCurrent::Min) == 255
        && UCk_Utils_ByteAttributeModifier_UE::Get_Delta(MaxOverride, ECk_MinMaxCurrent::Max) == 255);

    const FString ModifierKey = [&]() -> FString
    {
        for (const TSharedPtr<const FCkUiRecord>& Record : RecordsA->GetRecords())
        {
            const FCkUiFieldValue* Label = Record.IsValid() ? Record->FindField(TEXT("row-label")) : nullptr;
            if (Label != nullptr && Label->Kind == ECkUiFieldKind::Text
                && Label->Text.ToString().Contains(TAG_Label_AnimPlan_Goal.GetTag().ToString())) { return Record->GetKey(); }
        }
        return {};
    }();
    BoundedItem = Repeat->GetItemWidget(ModifierKey);
    const TSharedPtr<SEditableTextBox> ModifierInput = BoundedItem.IsValid()
        ? FindEditor(BoundedItem.ToSharedRef(), TEXT("byte-attribute-modifier-input")) : nullptr;
    const TSharedPtr<SButton> RemoveButton = BoundedItem.IsValid()
        ? FindButton(BoundedItem.ToSharedRef(), TEXT("byte-attribute-remove")) : nullptr;
    if (NOT TestTrue(TEXT("modifier row owns physical delta and labeled Remove controls"),
        ModifierInput.IsValid() && RemoveButton.IsValid()))
    { return false; }
    Commit(Slate, ModifierInput.ToSharedRef(), TEXT("256"));
    TestEqual(TEXT("modifier delta commits synchronously and clamps"),
        UCk_Utils_ByteAttributeModifier_UE::Get_Delta(Fixture.CurrentModifier), static_cast<uint8>(255));
    const FString ClearMinKey = [&]() -> FString
    {
        for (const TSharedPtr<const FCkUiRecord>& Record : RecordsA->GetRecords())
        {
            const FCkUiFieldValue* Label = Record.IsValid() ? Record->FindField(TEXT("row-label")) : nullptr;
            if (Label != nullptr && Label->Kind == ECkUiFieldKind::Text
                && Label->Text.ToString().Contains(TEXT("[Min] modifiers:"))) { return Record->GetKey(); }
        }
        return {};
    }();
    const TSharedPtr<SWidget> ClearMinItem = Repeat->GetItemWidget(ClearMinKey);
    const TSharedPtr<SButton> ClearMinButton = ClearMinItem.IsValid()
        ? FindButton(ClearMinItem.ToSharedRef(), TEXT("byte-attribute-clear-action")) : nullptr;
    if (NOT TestTrue(TEXT("each component Clear All action is physical"), ClearMinButton.IsValid())) { return false; }
    ClearMinButton->SimulateClick();
    TestTrue(TEXT("Clear All synchronously schedules its component modifier removal"),
        Fixture.MinModifier.Has<ck::FTag_DestroyEntity_Initiate>());
    RemoveButton->SimulateClick();
    TestTrue(TEXT("labeled Remove schedules public modifier destruction"), Fixture.CurrentModifier.Has<ck::FTag_DestroyEntity_Initiate>());

    const FString MaxModifierKey = [&]() -> FString
    {
        for (const TSharedPtr<const FCkUiRecord>& Record : RecordsA->GetRecords())
        {
            const FCkUiFieldValue* Label = Record.IsValid() ? Record->FindField(TEXT("row-label")) : nullptr;
            if (Label != nullptr && Label->Kind == ECkUiFieldKind::Text
                && Label->Text.ToString().Contains(
                    TAG_Label_AnimPlan_State.GetTag().ToString()))
            { return Record->GetKey(); }
        }
        return {};
    }();
    const TSharedPtr<SWidget> MaxModifierItem = Repeat->GetItemWidget(MaxModifierKey);
    const TSharedPtr<SButton> HeldMaxRemove = MaxModifierItem.IsValid()
        ? FindButton(MaxModifierItem.ToSharedRef(), TEXT("byte-attribute-remove")) : nullptr;
    const FString ClearMaxKey = [&]() -> FString
    {
        for (const TSharedPtr<const FCkUiRecord>& Record : RecordsA->GetRecords())
        {
            const FCkUiFieldValue* Label = Record.IsValid() ? Record->FindField(TEXT("row-label")) : nullptr;
            if (Label != nullptr && Label->Kind == ECkUiFieldKind::Text
                && Label->Text.ToString().Contains(TEXT("[Max] modifiers:")))
            { return Record->GetKey(); }
        }
        return {};
    }();
    const TSharedPtr<SWidget> ClearMaxItem = Repeat->GetItemWidget(ClearMaxKey);
    const TSharedPtr<SButton> HeldMaxClear = ClearMaxItem.IsValid()
        ? FindButton(ClearMaxItem.ToSharedRef(), TEXT("byte-attribute-clear-action")) : nullptr;
    if (NOT TestTrue(TEXT("live Max modifier retains held Remove and Clear controls"),
        HeldMaxRemove.IsValid() && HeldMaxClear.IsValid()))
    { return false; }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Byte Attributes resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorByteAttributes.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorByteAttributes.ui.css"))))) { return false; }
    TestTrue(TEXT("resource is complete HTML/CSS ownership with no native port"), Markup.Contains(TEXT("<int32-input"))
        && Markup.Contains(TEXT("byte-attribute-clear")) && NOT Markup.Contains(TEXT("<native")) && Stylesheet.Contains(TEXT(".byte-attribute-row")));
    TestTrue(TEXT("compatible reload is accepted"), ViewA->TryReload(Markup, Stylesheet, TEXT("ByteAttributes compatible")).Succeeded);
    const int64 AcceptedRevision = ViewA->GetRevision();
    const TSharedRef<SWidget> AcceptedRoot = ViewA->GetRegion(TEXT("main"));
    TestFalse(TEXT("missing keyed typed handler rejects atomically"), ViewA->TryReload(
        Markup.Replace(TEXT(" item-committed=\"byte-attribute-current-override\""), TEXT("")), Stylesheet,
        TEXT("ByteAttributes missing current handler")).Succeeded);
    TestTrue(TEXT("rejected reload retains the accepted revision and root"), ViewA->GetRevision() == AcceptedRevision
        && &ViewA->GetRegion(TEXT("main")).Get() == &AcceptedRoot.Get());

    const TSharedRef<SWidget> Filtered = Inspector.Build_Inspector(Fixture.OwnerA, MaxOnlyName);
    const TSharedRef<SCkInspector_ByteAttributesAuthored> FilteredAuthored =
        StaticCastSharedRef<SCkInspector_ByteAttributesAuthored>(Filtered);
    TestTrue(TEXT("filter retains only matching max-only record visibility"), CountVisible(
        *FilteredAuthored->Get_RecordsCollection(), TEXT("record-visible")) == 1
        && NOT FindKey(*FilteredAuthored->Get_RecordsCollection(), MaxOnlyName).IsEmpty());
    const TSharedRef<SCkInspector_ByteAttributesAuthored> ComponentFiltered =
        StaticCastSharedRef<SCkInspector_ByteAttributesAuthored>(
            Inspector.Build_Inspector(Fixture.OwnerA, TEXT("component:")));
    const TSharedPtr<FCkUiCollection> ComponentFilteredRecords =
        ComponentFiltered->Get_RecordsCollection();
    TestTrue(TEXT("native component filter alias exposes every authored replacement input"),
        ComponentFilteredRecords.IsValid()
        && CountVisible(*ComponentFilteredRecords, TEXT("current-visible")) == 2
        && CountVisible(*ComponentFilteredRecords, TEXT("min-visible")) == 1
        && CountVisible(*ComponentFilteredRecords, TEXT("max-visible")) == 2);
    const TSharedRef<SCkInspector_ByteAttributesAuthored> OverrideFiltered =
        StaticCastSharedRef<SCkInspector_ByteAttributesAuthored>(
            Inspector.Build_Inspector(Fixture.OwnerA, TEXT("override:")));
    const TSharedPtr<FCkUiCollection> OverrideFilteredRecords = OverrideFiltered->Get_RecordsCollection();
    TestTrue(TEXT("native override filter alias exposes every authored replacement input"),
        OverrideFilteredRecords.IsValid()
        && CountVisible(*OverrideFilteredRecords, TEXT("current-visible")) == 2
        && CountVisible(*OverrideFilteredRecords, TEXT("min-visible")) == 1
        && CountVisible(*OverrideFilteredRecords, TEXT("max-visible")) == 2);

    Fixture.Bounded.Try_Remove<ck::FFragment_ByteAttribute_Min>();
    Commit(Slate, MinInput.ToSharedRef(), TEXT("99"));
    TestTrue(TEXT("held Min control is inert after component loss"), NOT Fixture.Bounded.Has<ck::FFragment_ByteAttribute_Min>());
    Fixture.Bounded.Try_Remove<ck::FFragment_ByteAttribute_Current>();
    Commit(Slate, CurrentInput.ToSharedRef(), TEXT("99"));
    TestTrue(TEXT("held Current control is inert after typed attribute loss"), NOT Fixture.Bounded.Has<ck::FFragment_ByteAttribute_Current>());
    Selection->Set_SelectedEntities({FCk_Handle{Fixture.MaxOnly}});
    SelectionButton->SimulateClick();
    HeldMaxRemove->SimulateClick();
    HeldMaxClear->SimulateClick();
    TestTrue(TEXT("held navigation, Remove, and Clear controls fail closed after typed attribute loss"),
        Selection->Get_PrimarySelection() == FCk_Handle{Fixture.MaxOnly}
        && NOT Fixture.MaxModifier.Has<ck::FTag_DestroyEntity_Initiate>());

    Inspector.OnDeactivated();
    HeldMaxRemove->SimulateClick();
    HeldMaxClear->SimulateClick();
    TestTrue(TEXT("deactivation releases every retained view and collection"), AuthoredA->Is_Inert()
        && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredA->Get_RecordsCollection().IsValid());
    TestFalse(TEXT("held controls remain inert after deactivation"),
        Fixture.MaxModifier.Has<ck::FTag_DestroyEntity_Initiate>());
    ViewA.Reset(); ViewB.Reset(); RecordsA.Reset();
    return true;
}

#endif
