#include "CkHangMonitor/Window/SCkHangMonitor.h"

#include "CkHangMonitor/CkHangMonitorController.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include <DesktopPlatformModule.h>
#include <Framework/Application/SlateApplication.h>
#include <HAL/PlatformProcess.h>
#include <Misc/Paths.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

namespace ck_hang_monitor
{
    constexpr auto kProcDumpDocs = TEXT("https://learn.microsoft.com/sysinternals/downloads/procdump");

    auto ToneFor(const FCkHangMonitorSnapshot& InSnapshot) -> ECk_Tone
    {
        if (NOT InSnapshot.LastError.IsEmpty()) { return ECk_Tone::Err; }
        if (InSnapshot.IsStopping) { return ECk_Tone::Warn; }
        return InSnapshot.IsReady ? ECk_Tone::Ok : ECk_Tone::Neutral;
    }

    auto MakeAction(const FText& Label, const FText& Tooltip, FOnClicked OnClicked, TAttribute<bool> IsEnabled = true) -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
            .ContentPadding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
            .ToolTipText(Tooltip)
            .OnClicked(MoveTemp(OnClicked))
            .IsEnabled(MoveTemp(IsEnabled))
            [
                SNew(STextBlock)
                .Text(Label)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
            ];
    }
}

auto SCkHangMonitor::Construct(const FArguments& InArgs) -> void
{
    _Controller = InArgs._Controller;
    const auto Snapshot = _Controller->Get_Snapshot();
    _ProcDumpPath = Snapshot.ProcDumpPath.IsEmpty()
        ? _Controller->Get_DefaultProcDumpPath()
        : Snapshot.ProcDumpPath;
    _MaxDumps = FMath::Clamp(Snapshot.MaxDumps, 1, 3);

    const auto Controls = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [ ck_hang_monitor::MakeAction(FText::FromString(TEXT("Browse ProcDump")), FText::FromString(TEXT("Choose the locally installed procdump64.exe.")), FOnClicked::CreateSP(this, &SCkHangMonitor::DoOnBrowseProcDump), TAttribute<bool>::CreateSP(this, &SCkHangMonitor::Can_EditConfiguration)) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [ ck_hang_monitor::MakeAction(FText::FromString(TEXT("Arm")), FText::FromString(TEXT("Arm a single-process hang capture for the current game.")), FOnClicked::CreateSP(this, &SCkHangMonitor::DoOnArm), TAttribute<bool>::CreateSP(this, &SCkHangMonitor::Can_Arm)) ]
        + SHorizontalBox::Slot().AutoWidth()
        [ ck_hang_monitor::MakeAction(FText::FromString(TEXT("Stop")), FText::FromString(TEXT("Stop the armed ProcDump worker without closing this tab.")), FOnClicked::CreateSP(this, &SCkHangMonitor::DoOnStop), TAttribute<bool>::CreateSP(this, &SCkHangMonitor::Can_Stop)) ];

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(TEXT("CkHangMonitor"))
        .ToolTabId(TEXT("CkHangMonitor"))
        .StatusText(this, &SCkHangMonitor::Get_StatusText)
        .CommandGroups({ FCkDebug_CommandGroup::Primary(TEXT("Capture"), FText::FromString(TEXT("Hang capture controls")), Controls) })
        .Content()
        [
            SNew(SScrollBox)
            + SScrollBox::Slot().Padding(CkStyle::SpaceL)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("HANG MONITOR"))).SubText(FText::FromString(TEXT("A window unresponsive for more than 5 seconds triggers a dump. The worker stays armed when this tab closes."))).Underline(true) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM)
                [ SNew(SCkDebug_StatusPill).Text(this, &SCkHangMonitor::Get_StatusText).Tone(this, &SCkHangMonitor::Get_StatusTone) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                [ SNew(SCkDebug_SelectableLabel).Text(this, &SCkHangMonitor::Get_ProcDumpPathText).Font(CkStyle::MonoFont(CkStyle::FontSizeSmall())) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [ SNew(STextBlock).Text(FText::FromString(TEXT("Dump budget"))).Font(CkStyle::RegularFont(CkStyle::FontSizeBody())) ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [ SNew(SCkDebug_NumericEditor).Value_Lambda([this] { return static_cast<double>(_MaxDumps); }).Kind(ECkDebug_NumericKind::Integer).MinValue(1.0).MaxValue(3.0).FractionalDigits(0).IsEnabled(this, &SCkHangMonitor::Can_EditConfiguration).OnValueCommitted(FOnCkDebug_NumericCommitted::CreateLambda([this](double Value) { _MaxDumps = FMath::Clamp(FMath::RoundToInt(Value), 1, 3); })) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM)
                [ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("LIVE SESSION"))).Underline(true) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                [ SNew(SCkDebug_SelectableLabel).Text(this, &SCkHangMonitor::Get_CurrentProcessText).Font(CkStyle::MonoFont(CkStyle::FontSizeSmall())) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                [ SNew(SCkDebug_SelectableLabel).Text(this, &SCkHangMonitor::Get_DumpCountText).Font(CkStyle::MonoFont(CkStyle::FontSizeSmall())) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                [ SNew(SCkDebug_SelectableLabel).Text(this, &SCkHangMonitor::Get_OutputDirectoryText).Font(CkStyle::MonoFont(CkStyle::FontSizeSmall())) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                [ ck_hang_monitor::MakeAction(FText::FromString(TEXT("Open output folder")), FText::FromString(TEXT("Open the dump output directory in Explorer.")), FOnClicked::CreateSP(this, &SCkHangMonitor::DoOnOpenOutput), TAttribute<bool>::CreateSP(this, &SCkHangMonitor::Can_OpenOutput)) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM)
                [ ck_hang_monitor::MakeAction(FText::FromString(TEXT("ProcDump setup and help")), FText::FromString(TEXT("Open Microsoft's official ProcDump documentation.")), FOnClicked::CreateSP(this, &SCkHangMonitor::DoOnOpenProcDumpHelp)) ]
            ]
        ]
    ];
}

auto SCkHangMonitor::Get_StatusText() const -> FText
{
    const auto Snapshot = _Controller->Get_Snapshot();
    if (NOT _UiError.IsEmpty()) { return FText::FromString(_UiError); }
    if (NOT Snapshot.LastError.IsEmpty()) { return FText::FromString(Snapshot.LastError); }
    return FText::FromString(Snapshot.Status.IsEmpty() ? TEXT("Idle") : Snapshot.Status);
}
auto SCkHangMonitor::Get_StatusTone() const -> ECk_Tone { return _UiError.IsEmpty() ? ck_hang_monitor::ToneFor(_Controller->Get_Snapshot()) : ECk_Tone::Err; }
auto SCkHangMonitor::Get_ProcDumpPathText() const -> FText { return FText::FromString(FString::Printf(TEXT("ProcDump: %s"), *_ProcDumpPath)); }
auto SCkHangMonitor::Get_OutputDirectoryText() const -> FText { return FText::FromString(FString::Printf(TEXT("Output: %s"), *_Controller->Get_Snapshot().OutputDirectory)); }
auto SCkHangMonitor::Get_CurrentProcessText() const -> FText { const auto S = _Controller->Get_Snapshot(); return FText::FromString(S.TargetPid == 0 ? FString::Printf(TEXT("Current process PID: %u"), FPlatformProcess::GetCurrentProcessId()) : FString::Printf(TEXT("Current process PID: %u"), S.TargetPid)); }
auto SCkHangMonitor::Get_DumpCountText() const -> FText { const auto S = _Controller->Get_Snapshot(); return FText::FromString(FString::Printf(TEXT("Dumps: %d / %d"), S.NumDumps, S.MaxDumps)); }
auto SCkHangMonitor::Can_Arm() const -> bool { const auto S = _Controller->Get_Snapshot(); return NOT S.IsMonitoring && NOT S.IsStopping; }
auto SCkHangMonitor::Can_Stop() const -> bool { const auto S = _Controller->Get_Snapshot(); return S.IsMonitoring && NOT S.IsStopping; }
auto SCkHangMonitor::Can_EditConfiguration() const -> bool { return Can_Arm(); }
auto SCkHangMonitor::Can_OpenOutput() const -> bool { const auto S = _Controller->Get_Snapshot(); return NOT S.OutputDirectory.IsEmpty() && FPaths::DirectoryExists(S.OutputDirectory); }

auto SCkHangMonitor::DoOnBrowseProcDump() -> FReply
{
    if (auto* DesktopPlatform = FDesktopPlatformModule::Get(); DesktopPlatform != nullptr)
    {
        TArray<FString> Files;
        if (DesktopPlatform->OpenFileDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()), TEXT("Choose procdump64.exe"), FPaths::GetPath(_ProcDumpPath), TEXT(""), TEXT("procdump64.exe|procdump64.exe"), EFileDialogFlags::None, Files) && !Files.IsEmpty())
        { _ProcDumpPath = Files[0]; _UiError.Reset(); }
    }
    else { _UiError = TEXT("Desktop file browsing is unavailable."); }
    return FReply::Handled();
}
auto SCkHangMonitor::DoOnArm() -> FReply { FString Error; if (_Controller->TryStart(_ProcDumpPath, _MaxDumps, Error)) { _UiError.Reset(); } else { _UiError = Error; } return FReply::Handled(); }
auto SCkHangMonitor::DoOnStop() -> FReply { FString Error; if (_Controller->TryStop(Error)) { _UiError.Reset(); } else { _UiError = Error; } return FReply::Handled(); }
auto SCkHangMonitor::DoOnOpenOutput() -> FReply { const auto OutputDirectory = _Controller->Get_Snapshot().OutputDirectory; if (FPaths::DirectoryExists(OutputDirectory)) { FPlatformProcess::ExploreFolder(*OutputDirectory); } else { _UiError = TEXT("The dump output directory is not available."); } return FReply::Handled(); }
auto SCkHangMonitor::DoOnOpenProcDumpHelp() -> FReply { FPlatformProcess::LaunchURL(ck_hang_monitor::kProcDumpDocs, nullptr, nullptr); return FReply::Handled(); }
