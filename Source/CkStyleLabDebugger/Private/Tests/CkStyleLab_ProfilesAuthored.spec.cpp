#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "UObject/Class.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_style_lab_profiles_authored_tests
{
    auto FindLabel(const TSharedRef<SWidget>& Root, const FString& Text) -> TSharedPtr<SCkFlexText>
    {
        if (Root->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedRef<SCkFlexText>(Root)->GetText().ToString() == Text) { return StaticCastSharedRef<SCkFlexText>(Root); }
        const auto* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const auto Found = FindLabel(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), Text);
            if (Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto HasText(const TSharedRef<SWidget>& Root, const FString& Text) -> bool
    {
        if (Root->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedRef<SCkFlexText>(Root)->GetText().ToString() == Text) { return true; }
        const auto* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (HasText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), Text)) { return true; } }
        return false;
    }

    auto FindButton(const TSharedRef<SWidget>& Root, const FString& Text) -> TSharedPtr<SButton>
    {
        if (Root->GetTypeAsString() == TEXT("SButton") && HasText(Root, Text)) { return StaticCastSharedRef<SButton>(Root); }
        const auto* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const auto Found = FindButton(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), Text);
            if (Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto FindInspector(const TSharedRef<SWidget>& Root) -> TSharedPtr<SCkDebug_InspectorPanel>
    {
        if (Root->GetTypeAsString() == TEXT("SCkDebug_InspectorPanel")) { return StaticCastSharedRef<SCkDebug_InspectorPanel>(Root); }
        const auto* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const auto Found = FindInspector(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)));
            if (Found.IsValid()) { return Found; }
        }
        return {};
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkStyleLab_ProfilesAuthored,
    "Ck.UiAuthoring.StyleLab.ProfileControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_ProfilesAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_style_lab_profiles_authored_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Style Lab profile test requires Slate.")); return false; }
    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();
    if (!TestNotNull(TEXT("Style settings exist"), Settings)) { return false; }
    const auto OriginalSelection = Settings->Selection;
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
    const auto View = Pane->Get_ProfileView();
    if (!TestTrue(TEXT("Style Lab loads the installed authored profiles"), View.IsValid() && View->GetLastResult().Succeeded)) { return false; }
    const auto Region = View->GetRegion(TEXT("main"));
    Pane->SlatePrepass();
    const auto Inspector = FindInspector(Region);
    if (!TestTrue(TEXT("Profiles use the shared authored inspector"), Inspector.IsValid())) { return false; }
    const auto& Profiles = ck::debug_axes::Get_StyleProfiles();
    if (!TestTrue(TEXT("Profile fixture has multiple real profiles"), Profiles.Num() >= 2)) { return false; }
    for (const auto& Profile : Profiles)
    { TestTrue(*FString::Printf(TEXT("Authored button exists for %s"), *Profile.Name), FindButton(Region, Profile.Name).IsValid()); }
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const auto Button = FindButton(Region, Profiles[Index].Name);
        if (!Button.IsValid()) { return false; }
        const uint32 Revision = Settings->Get_Revision();
        Button->SimulateClick();
        TestEqual(TEXT("Authored profile action selects the actual profile"), Settings->ActiveProfileName, Profiles[Index].Name);
        TestTrue(TEXT("Authored profile applies the complete axis selection"), FCkDebuggerStyleSelection::StaticStruct()->CompareScriptStruct(&Settings->Selection, &Profiles[Index].Selection, 0));
        TestTrue(TEXT("Profile action notifies live style consumers"), Settings->Get_Revision() > Revision);
        TestEqual(TEXT("Profile action notifies its owning preview pane once"), Notifications, Index + 1);
        TestTrue(TEXT("Authored current-profile label follows selection"), HasText(Region, FString::Printf(TEXT("Current: %s"), *Profiles[Index].Name)));
        const auto ActiveLabel = FindLabel(Region, Profiles[Index].Name);
        TestTrue(TEXT("Active profile label uses the shared accent"), ActiveLabel.IsValid() && ActiveLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(CkStyle::Accent()));
    }
    Inspector->Set_Expanded(false);
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("Debugger plugin resolves authored resources"), Plugin.IsValid())) { return false; }
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    const auto Reload = View->ReloadFiles(FPaths::Combine(Directory, TEXT("StyleLabProfiles.ui.html")), FPaths::Combine(Directory, TEXT("StyleLabProfiles.ui.css")));
    if (!TestTrue(*FString::Join(Reload.Errors, TEXT("\n")), Reload.Succeeded)) { return false; }
    TestTrue(TEXT("Profile reload retains shared inspector collapse"), FindInspector(Region) == Inspector && !Inspector->Is_Expanded());
    const int64 AcceptedRevision = View->GetRevision();
    TestFalse(TEXT("Malformed profile reload rejects"), View->TryReload(TEXT("<ui>"), TEXT("")).Succeeded);
    TestTrue(TEXT("Malformed profile reload preserves accepted inspector"), View->GetRevision() == AcceptedRevision && FindInspector(Region) == Inspector);
    Inspector->Set_Expanded(true);
    const auto Held = FindButton(Region, Profiles[0].Name);
    if (!TestTrue(TEXT("Current profile button remains available"), Held.IsValid())) { return false; }
    const int32 BeforeRelease = Notifications;
    Pane.Reset();
    Held->SimulateClick();
    TestEqual(TEXT("Held authored profile view cannot call released pane"), Notifications, BeforeRelease);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkStyleLab_ProfilesGeometry,
    "Ck.UiAuthoring.StyleLab.ProfileGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_ProfilesGeometry::RunTest(const FString&) -> bool
{
    using namespace ck_style_lab_profiles_authored_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Style Lab geometry requires Slate.")); return false; }
    auto& Slate = FSlateApplication::Get();
    const float OriginalScale = Slate.GetApplicationScale();
    TSharedPtr<SWindow> Window;
    ON_SCOPE_EXIT
    {
        if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
        Slate.SetApplicationScale(OriginalScale);
    };
    const auto Pane = SNew(SCkStyleLab_ControlsPane);
    const auto View = Pane->Get_ProfileView();
    if (!TestTrue(TEXT("Geometry uses installed production profile view"), View.IsValid() && View->GetLastResult().Succeeded)) { return false; }
    const auto Region = View->GetRegion(TEXT("main"));
    Window = SNew(SWindow).ClientSize(FVector2D(900, 800)).CreateTitleBar(false)[Pane];
    Slate.AddWindow(Window.ToSharedRef(), true);
    const FString Output = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/StyleLabProfiles"));
    IFileManager::Get().MakeDirectory(*Output, true);
    const auto& Profiles = ck::debug_axes::Get_StyleProfiles();
    struct FCase { const TCHAR* Name; float Width; float Scale; };
    const FCase Cases[] = {{TEXT("Wide"), 900, 1}, {TEXT("Narrow"), 320, 1}, {TEXT("Narrow150"), 320, 1.5f}};
    double WideHeight = 0;
    for (const auto& Case : Cases)
    {
        Slate.SetApplicationScale(Case.Scale);
        Window->Resize(FVector2D(Case.Width, 800));
        for (int32 Tick = 0; Tick < 6; ++Tick) { Slate.PumpMessages(); Slate.Tick(); }
        const auto RegionGeometry = Region->GetCachedGeometry();
        const auto First = FindButton(Region, Profiles[0].Name);
        const auto Last = FindButton(Region, Profiles.Last().Name);
        if (!TestTrue(TEXT("Every geometry case mounts profile buttons"), First.IsValid() && Last.IsValid())) { return false; }
        for (const auto& Profile : Profiles)
        {
            const auto Button = FindButton(Region, Profile.Name);
            if (!Button.IsValid()) { AddError(TEXT("Profile button disappeared during resize.")); return false; }
            const auto Geometry = Button->GetCachedGeometry();
            const FVector2D TopLeft = RegionGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
            const FVector2D BottomRight = RegionGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
            TestTrue(*FString::Printf(TEXT("%s keeps %s inside authored region"), Case.Name, *Profile.Name),
                Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0 && TopLeft.X >= -1 && TopLeft.Y >= -1
                && BottomRight.X <= RegionGeometry.GetLocalSize().X + 1 && BottomRight.Y <= RegionGeometry.GetLocalSize().Y + 1);
        }
        const FVector2D FirstPosition = RegionGeometry.AbsoluteToLocal(First->GetCachedGeometry().GetAbsolutePosition());
        const FVector2D LastPosition = RegionGeometry.AbsoluteToLocal(Last->GetCachedGeometry().GetAbsolutePosition());
        if (Case.Width > 500)
        {
            WideHeight = RegionGeometry.GetLocalSize().Y;
            TestTrue(TEXT("Wide profile strip fits one line"), FMath::IsNearlyEqual(FirstPosition.Y, LastPosition.Y, 1.0));
        }
        else
        {
            TestTrue(TEXT("Narrow profile strip wraps into additional lines"), LastPosition.Y > FirstPosition.Y + 1);
            TestTrue(TEXT("Narrow authored profile region grows for wrapped content"), RegionGeometry.GetLocalSize().Y > WideHeight);
        }
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!TestTrue(TEXT("Slate captures production profile region"), Slate.TakeScreenshot(Region, Pixels, Size) && Size.X > 0 && Size.Y > 0 && Pixels.Num() >= Size.X * Size.Y)) { return false; }
        const FImageView Image(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8);
        TestTrue(TEXT("Profile capture writes PNG"), FImageUtils::SaveImageByExtension(*FPaths::Combine(Output, FString(Case.Name) + TEXT(".png")), Image));
    }
    return true;
}

#endif
