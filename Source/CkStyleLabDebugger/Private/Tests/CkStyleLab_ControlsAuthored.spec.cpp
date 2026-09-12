#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_style_lab_controls_authored_tests
{
    struct FGroupCase
    {
        const TCHAR* RepeatId;
        const TCHAR* PreviewId;
        const TCHAR* ValueId;
        const TCHAR* NextId;
        int32 ExpectedAxes;
    };

    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWindow>& InWindow, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        InSlate.SetCursorPos(Position);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftButtonDown{EKeys::LeftMouseButton};
        const FPointerEvent Move(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::Invalid, 0, FModifierKeysState{});
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position, LeftButtonDown, EKeys::LeftMouseButton, 0, FModifierKeysState{});
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::LeftMouseButton, 0, FModifierKeysState{});
        InSlate.ProcessMouseMoveEvent(Move, true);
        const bool bHandled = InSlate.ProcessMouseButtonDownEvent(InWindow->GetNativeWindow(), Down);
        InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return bHandled;
    }

    auto FindTagged(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTagged(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, InTag);
        if (!Tagged.IsValid()) { return {}; }
        if (Tagged->GetTypeAsString() == TEXT("SButton")) { return StaticCastSharedPtr<SButton>(Tagged); }
        const FChildren* Children = Tagged->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (Children->GetChildAt(Index)->GetTypeAsString() == TEXT("SButton"))
            { return StaticCastSharedRef<SButton>(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); }
        }
        return {};
    }

    auto FirstItemKey(const TSharedPtr<SCkUiRepeat>& InRepeat) -> FString
    {
        if (!InRepeat.IsValid()) { return {}; }
        const TArray<FName> Properties = {
            TEXT("SurfaceElevation"), TEXT("BadgeStyle"), TEXT("EntityIdStyle"),
            TEXT("FlashOnChange"), TEXT("IconSize"), TEXT("GraphNodeStyle")};
        for (const FName Property : Properties)
        {
            if (InRepeat->GetItemWidget(Property.ToString()).IsValid()) { return Property.ToString(); }
        }
        return {};
    }

    auto FindFlexText(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkFlexText>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText")) { return StaticCastSharedRef<SCkFlexText>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkFlexText> Found = FindFlexText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto TaggedText(const TSharedRef<SWidget>& InRoot, const FName InTag) -> FString
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, InTag);
        const TSharedPtr<SCkFlexText> Text = Tagged.IsValid() ? FindFlexText(Tagged.ToSharedRef()) : nullptr;
        return Text.IsValid() ? Text->GetText().ToString() : FString{};
    }

    auto FindSamplePane(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkStyleLab_SamplePane>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkStyleLab_SamplePane")) { return StaticCastSharedRef<SCkStyleLab_SamplePane>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkStyleLab_SamplePane> Found = FindSamplePane(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto FindInspector(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SCkDebug_InspectorPanel>
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, InTag);
        if (!Tagged.IsValid()) { return {}; }
        if (Tagged->GetTypeAsString() == TEXT("SCkDebug_InspectorPanel")) { return StaticCastSharedPtr<SCkDebug_InspectorPanel>(Tagged); }
        const FChildren* Children = Tagged->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (Children->GetChildAt(Index)->GetTypeAsString() == TEXT("SCkDebug_InspectorPanel"))
            { return StaticCastSharedRef<SCkDebug_InspectorPanel>(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); }
        }
        return {};
    }

    auto FindNativeErrorText(const TSharedRef<SWidget>& InRoot) -> FString
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const FString Text = StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString();
            if (!Text.IsEmpty()) { return Text; }
        }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const FString Found = FindNativeErrorText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)));
            if (!Found.IsEmpty()) { return Found; }
        }
        return {};
    }

    auto Capture(FSlateApplication& InSlate, const TSharedRef<SWidget>& InRoot, const FString& InPath) -> bool
    {
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!InSlate.TakeScreenshot(InRoot, Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y) { return false; }
        const FImageView Image(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8);
        return FImageUtils::SaveImageByExtension(*InPath, Image);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkStyleLab_ControlsAuthored,
    "Ck.UiAuthoring.StyleLab.ControlsPane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_ControlsAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_style_lab_controls_authored_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Style Lab controls test requires Slate.")); return false; }
    UCkDebuggerStyleSettings* Settings = UCkDebuggerStyleSettings::Get_Mutable();
    if (!TestNotNull(TEXT("Style settings exist"), Settings)) { return false; }
    const FCkDebuggerStyleSelection OriginalSelection = Settings->Selection;
    const FString OriginalName = Settings->ActiveProfileName;
    const int32 OriginalSchema = Settings->SchemaVersion;
    ON_SCOPE_EXIT
    {
        Settings->Selection = OriginalSelection;
        Settings->ActiveProfileName = OriginalName;
        Settings->SchemaVersion = OriginalSchema;
        Settings->SaveConfig();
        Settings->NotifyChanged();
    };

    int32 Notifications = 0;
    TSharedPtr<SCkStyleLab_ControlsPane> Pane = SNew(SCkStyleLab_ControlsPane)
        .OnSelectionChanged(FOnCkStyleLab_SelectionChanged::CreateLambda([&Notifications]() { ++Notifications; }));
    const TSharedPtr<FCkUiView> Controls = Pane->Get_ControlsView();
    const TSharedPtr<FCkUiView> Profiles = Pane->Get_ProfileView();
    const bool bControlsLoaded = Controls.IsValid() && Controls->GetLastResult().Succeeded;
    const bool bProfilesLoaded = Profiles.IsValid() && Profiles->GetLastResult().Succeeded;
    if (!bControlsLoaded || !bProfilesLoaded)
    {
        const FString ControlsError = Controls.IsValid() ? FString::Join(Controls->GetLastResult().Errors, TEXT("\n")) : FindNativeErrorText(Pane.ToSharedRef());
        const FString ProfilesError = Profiles.IsValid() ? FString::Join(Profiles->GetLastResult().Errors, TEXT("\n")) : FindNativeErrorText(Pane.ToSharedRef());
        AddError(FString::Printf(TEXT("Style Lab initial view load failure. Controls: %s\nProfiles: %s"), *ControlsError, *ProfilesError));
        return false;
    }
    const TSharedRef<SWidget> Region = Controls->GetRegion(TEXT("main"));
    if (!TestTrue(TEXT("Controls document mounts its authored root"), FindTagged(Region, TEXT("style-lab-controls-root")).IsValid())) { return false; }

    const FGroupCase Groups[] = {
        {TEXT("style-lab-workbench-surfaces/body/axes-workbench-surfaces"), TEXT("preview-workbench-surfaces"), TEXT("workbench-axis-value"), TEXT("workbench-axis-next"), 6},
        {TEXT("style-lab-tokens-legend/body/axes-tokens-legend"), TEXT("preview-tokens-legend"), TEXT("tokens-axis-value"), TEXT("tokens-axis-next"), 6},
        {TEXT("style-lab-entity-values/body/axes-entity-values"), TEXT("preview-entity-values"), TEXT("entity-axis-value"), TEXT("entity-axis-next"), 3},
        {TEXT("style-lab-hierarchy-editing/body/axes-hierarchy-editing"), TEXT("preview-hierarchy-editing"), TEXT("hierarchy-axis-value"), TEXT("hierarchy-axis-next"), 4},
        {TEXT("style-lab-icons/body/axes-icons"), TEXT("preview-icons"), TEXT("icons-axis-value"), TEXT("icons-axis-next"), 2},
        {TEXT("style-lab-graph-telemetry/body/axes-graph-telemetry"), TEXT("preview-graph-telemetry"), TEXT("graph-axis-value"), TEXT("graph-axis-next"), 3}};
    FSlateApplication& Slate = FSlateApplication::Get();
    const FVector2D PreviousCursor = Slate.GetCursorPos();
    TSharedPtr<SScrollBox> Scroll;
    TSharedPtr<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(900.0f, 800.0f)).CreateTitleBar(false).HasCloseButton(false)
        [SAssignNew(Scroll, SScrollBox) + SScrollBox::Slot().Padding(8.0f)[Pane.ToSharedRef()]];
    ON_SCOPE_EXIT
    {
        if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
        Slate.SetCursorPos(PreviousCursor);
    };
    Slate.AddWindow(Window.ToSharedRef(), true);
    Tick(Slate);
    auto AcceptedRepeats = TArray<TSharedPtr<SCkUiRepeat>>{};
    auto AcceptedPreviews = TArray<TSharedPtr<SCkStyleLab_SamplePane>>{};
    TSharedPtr<SButton> HeldCurrentAction;
    for (const FGroupCase& Group : Groups)
    {
        const TSharedPtr<SCkUiRepeat> Repeat = Controls->GetRepeat(Group.RepeatId);
        const TSharedPtr<SWidget> PreviewPort = FindTagged(Region, Group.PreviewId);
        const TSharedPtr<SCkStyleLab_SamplePane> Preview = PreviewPort.IsValid() ? FindSamplePane(PreviewPort.ToSharedRef()) : nullptr;
        if (!TestTrue(*FString::Printf(TEXT("%s exposes its complete authored axis repeat and native preview"), Group.RepeatId),
            Repeat.IsValid() && Repeat->GetItemCount() == Group.ExpectedAxes && Preview.IsValid())) { return false; }
        const FString Key = FirstItemKey(Repeat);
        const TSharedPtr<SWidget> Item = Repeat->GetItemWidget(Key);
        const TSharedPtr<SButton> Next = Item.IsValid() ? FindButton(Item.ToSharedRef(), Group.NextId) : nullptr;
        const FString ValueBefore = Item.IsValid() ? TaggedText(Item.ToSharedRef(), Group.ValueId) : FString{};
        const uint32 Revision = Settings->Get_Revision();
        if (!TestTrue(*FString::Printf(TEXT("%s cycles a real authored axis action"), Group.RepeatId), Next.IsValid())) { return false; }
        Scroll->ScrollDescendantIntoView(Next.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        if (!TestTrue(*FString::Printf(TEXT("%s action is visibly reachable in the real scroll host"), Group.RepeatId),
            Next->GetCachedGeometry().GetLocalSize().X > 4.0f && Next->GetCachedGeometry().GetLocalSize().Y > 4.0f
            && Click(Slate, Window.ToSharedRef(), Next.ToSharedRef()))) { return false; }
        const FString ValueAfter = TaggedText(Item.ToSharedRef(), Group.ValueId);
        if (!TestEqual(*FString::Printf(TEXT("%s native action selects the Custom settings profile"), Group.RepeatId),
            Settings->ActiveProfileName, FString(TEXT("Custom")))) { return false; }
        if (!TestTrue(*FString::Printf(TEXT("%s native action advances the settings revision"), Group.RepeatId),
            Settings->Get_Revision() > Revision)) { return false; }
        if (!TestTrue(*FString::Printf(TEXT("%s republished authored option text is nonempty and changed"), Group.RepeatId),
            !ValueBefore.IsEmpty() && !ValueAfter.IsEmpty() && ValueAfter != ValueBefore))
        {
            AddInfo(FString::Printf(TEXT("%s authored option text: before='%s', after='%s'."), Group.RepeatId, *ValueBefore, *ValueAfter));
            return false;
        }
        AcceptedRepeats.Add(Repeat);
        AcceptedPreviews.Add(Preview);
        if (!HeldCurrentAction.IsValid()) { HeldCurrentAction = Next; }
    }
    TestEqual(TEXT("Every generic group dispatches exactly one selection notification"), Notifications, static_cast<int32>(UE_ARRAY_COUNT(Groups)));
    TestEqual(TEXT("All reflected generic axes remain authored"), Pane->Get_AxisCount(), 24);
    TestEqual(TEXT("All group previews remain mounted"), Pane->Get_GroupPreviewCount(), 7);
    TestTrue(TEXT("Input HUD controls are authored around their native preview"), FindTagged(Region, TEXT("input-hud-padding-x-input")).IsValid()
        && FindTagged(Region, TEXT("preview-input-hud")).IsValid());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("Debugger plugin resolves controls resources"), Plugin.IsValid())) { return false; }
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    const FString MarkupPath = FPaths::Combine(Directory, TEXT("StyleLabControls.ui.html"));
    const FString CssPath = FPaths::Combine(Directory, TEXT("StyleLabControls.ui.css"));
    const int64 ProfileRevision = Profiles->GetRevision();
    const int64 ControlsRevision = Controls->GetRevision();
    const TSharedPtr<SCkDebug_InspectorPanel> Inspector = FindInspector(Region, TEXT("style-lab-workbench-surfaces"));
    if (!TestTrue(TEXT("Controls resolve the retained Workbench inspector"), Inspector.IsValid() && HeldCurrentAction.IsValid())) { return false; }
    Inspector->Set_Expanded(false);
    if (!TestTrue(TEXT("Compatible controls reload advances only its retained second view"), Controls->ReloadFiles(MarkupPath, CssPath).Succeeded)) { return false; }
    TestTrue(TEXT("Controls reload advances only its retained second view while profiles remain independent"),
        Controls->GetRevision() == ControlsRevision + 1 && Profiles->GetRevision() == ProfileRevision);
    TestTrue(TEXT("Compatible controls reload retains the explicitly collapsed inspector"), FindInspector(Region, TEXT("style-lab-workbench-surfaces")) == Inspector && !Inspector->Is_Expanded());
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Groups); ++Index)
    {
        TestTrue(*FString::Printf(TEXT("Accepted controls reload retains %s repeat"), Groups[Index].RepeatId),
            Controls->GetRepeat(Groups[Index].RepeatId) == AcceptedRepeats[Index]);
        const TSharedPtr<SWidget> PreviewPort = FindTagged(Region, Groups[Index].PreviewId);
        TestTrue(*FString::Printf(TEXT("Accepted controls reload retains %s native preview leaf"), Groups[Index].PreviewId),
            PreviewPort.IsValid() && FindSamplePane(PreviewPort.ToSharedRef()) == AcceptedPreviews[Index]);
    }
    Inspector->Set_Expanded(true);
    const uint32 StaleActionRevision = Settings->Get_Revision();
    HeldCurrentAction->SimulateClick();
    TestEqual(TEXT("Detached pre-reload Workbench axis action is inert"), Settings->Get_Revision(), StaleActionRevision);
    const TSharedPtr<SCkUiRepeat> CurrentWorkbenchRepeat = Controls->GetRepeat(Groups[0].RepeatId);
    const FString CurrentWorkbenchKey = FirstItemKey(CurrentWorkbenchRepeat);
    const TSharedPtr<SWidget> CurrentWorkbenchItem = CurrentWorkbenchRepeat.IsValid() ? CurrentWorkbenchRepeat->GetItemWidget(CurrentWorkbenchKey) : nullptr;
    const TSharedPtr<SButton> CurrentWorkbenchAction = CurrentWorkbenchItem.IsValid()
        ? FindButton(CurrentWorkbenchItem.ToSharedRef(), Groups[0].NextId) : nullptr;
    if (!TestTrue(TEXT("Compatible reload exposes a fresh mounted Workbench axis action"),
        CurrentWorkbenchAction.IsValid() && CurrentWorkbenchAction != HeldCurrentAction)) { return false; }
    const uint32 CurrentActionRevision = Settings->Get_Revision();
    Scroll->ScrollDescendantIntoView(CurrentWorkbenchAction.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    TestTrue(TEXT("Fresh mounted Workbench axis action dispatches after compatible reload"), Click(Slate, Window.ToSharedRef(), CurrentWorkbenchAction.ToSharedRef())
        && Settings->Get_Revision() > CurrentActionRevision);
    const TSharedPtr<SCkUiRepeat> RepublishedWorkbenchRepeat = Controls->GetRepeat(Groups[0].RepeatId);
    const FString RepublishedWorkbenchKey = FirstItemKey(RepublishedWorkbenchRepeat);
    const TSharedPtr<SWidget> RepublishedWorkbenchItem = RepublishedWorkbenchRepeat.IsValid()
        ? RepublishedWorkbenchRepeat->GetItemWidget(RepublishedWorkbenchKey) : nullptr;
    const TSharedPtr<SButton> RepublishedWorkbenchAction = RepublishedWorkbenchItem.IsValid()
        ? FindButton(RepublishedWorkbenchItem.ToSharedRef(), Groups[0].NextId) : nullptr;
    if (!TestTrue(TEXT("Axis record republish exposes a new mounted Workbench action"),
        RepublishedWorkbenchAction.IsValid() && RepublishedWorkbenchAction != CurrentWorkbenchAction)) { return false; }

    FString Markup;
    FString Css;
    if (!TestTrue(TEXT("Installed controls resources read for rejected reload"), FFileHelper::LoadFileToString(Markup, *MarkupPath) && FFileHelper::LoadFileToString(Css, *CssPath))) { return false; }
    TestTrue(TEXT("Rejected controls fixture removes a required collection binding"), Markup.ReplaceInline(TEXT("bind=\"axes-icons\""), TEXT("bind=\"missing-axes\"")) == 1);
    const int64 AcceptedRevision = Controls->GetRevision();
    const FCkUiLoadResult Rejected = Controls->TryReload(Markup, Css);
    TestFalse(TEXT("Missing generic collection binding rejects controls reload"), Rejected.Succeeded);
    TestTrue(TEXT("Rejected controls reload preserves the accepted revision"), Controls->GetRevision() == AcceptedRevision);
    TestTrue(TEXT("Rejected controls reload preserves the accepted icons repeat"),
        Controls->GetRepeat(TEXT("style-lab-icons/body/axes-icons")) == AcceptedRepeats[4]);
    const TSharedPtr<SWidget> RejectedIconsPreviewPort = FindTagged(Region, TEXT("preview-icons"));
    TestTrue(TEXT("Rejected controls reload preserves the accepted icons native preview leaf"),
        RejectedIconsPreviewPort.IsValid() && FindSamplePane(RejectedIconsPreviewPort.ToSharedRef()) == AcceptedPreviews[4]);
    const TSharedPtr<SWidget> RejectedWorkbenchItem = RepublishedWorkbenchRepeat.IsValid()
        ? RepublishedWorkbenchRepeat->GetItemWidget(RepublishedWorkbenchKey) : nullptr;
    const TSharedPtr<SButton> RejectedWorkbenchAction = RejectedWorkbenchItem.IsValid()
        ? FindButton(RejectedWorkbenchItem.ToSharedRef(), Groups[0].NextId) : nullptr;
    if (!TestTrue(TEXT("Rejected controls reload preserves the accepted mounted Workbench action"),
        RejectedWorkbenchAction == RepublishedWorkbenchAction)) { return false; }

    const TSharedPtr<SWidget> Root = FindTagged(Region, TEXT("style-lab-controls-root"));
    if (!TestTrue(TEXT("Wide controls root has arranged geometry"), Root.IsValid() && Root->GetCachedGeometry().GetLocalSize().X > 0.0f)) { return false; }
    Window->Resize(FVector2D(320.0f, 800.0f));
    Tick(Slate);
    if (!TestTrue(TEXT("Narrow controls root remains arranged"), Root->GetCachedGeometry().GetLocalSize().X > 0.0f)) { return false; }
    Scroll->ScrollDescendantIntoView(RepublishedWorkbenchAction.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    const FGeometry ScrollGeometry = Scroll->GetCachedGeometry();
    const FGeometry ActionGeometry = RepublishedWorkbenchAction->GetCachedGeometry();
    const FVector2D ActionCenter = ScrollGeometry.AbsoluteToLocal(ActionGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f)));
    TestTrue(TEXT("Narrow controls keeps a real axis action inside the visible scroll viewport"), ActionGeometry.GetLocalSize().X > 4.0f
        && ActionGeometry.GetLocalSize().Y > 4.0f && ActionCenter.X >= 0.0f && ActionCenter.Y >= 0.0f
        && ActionCenter.X <= ScrollGeometry.GetLocalSize().X && ActionCenter.Y <= ScrollGeometry.GetLocalSize().Y);
    const FString Output = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/StyleLabControls"));
    IFileManager::Get().MakeDirectory(*Output, true);
    TestTrue(TEXT("Narrow controls viewport capture writes"), Capture(Slate, Scroll.ToSharedRef(), FPaths::Combine(Output, TEXT("Controls-Narrow.png"))));
    const TWeakPtr<SCkStyleLab_ControlsPane> WeakPane = Pane;
    const uint32 OwnerReleaseRevision = Settings->Get_Revision();
    Slate.DestroyWindowImmediately(Window.ToSharedRef());
    Window.Reset();
    Scroll.Reset();
    Pane.Reset();
    TestTrue(TEXT("Released controls pane expires while held mounted axis action survives safely"), !WeakPane.IsValid());
    RepublishedWorkbenchAction->SimulateClick();
    TestEqual(TEXT("Held mounted axis action is inert after its owning pane expires"), Settings->Get_Revision(), OwnerReleaseRevision);
    return true;
}

#endif
