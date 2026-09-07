#pragma once

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SCompoundWidget.h"

class FCkHangMonitorController;

class SCkHangMonitor : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkHangMonitor) {}
        SLATE_ARGUMENT(FCkHangMonitorController*, Controller)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
private:
    auto Get_StatusText() const -> FText;
    auto Get_StatusTone() const -> ECk_Tone;
    auto Get_ProcDumpPathText() const -> FText;
    auto Get_OutputDirectoryText() const -> FText;
    auto Get_CurrentProcessText() const -> FText;
    auto Get_DumpCountText() const -> FText;
    auto Can_Arm() const -> bool;
    auto Can_Stop() const -> bool;
    auto Can_EditConfiguration() const -> bool;
    auto Can_OpenOutput() const -> bool;
    auto DoOnBrowseProcDump() -> FReply;
    auto DoOnArm() -> FReply;
    auto DoOnStop() -> FReply;
    auto DoOnOpenOutput() -> FReply;
    auto DoOnOpenProcDumpHelp() -> FReply;

    FCkHangMonitorController* _Controller = nullptr;
    FString _ProcDumpPath;
    FString _UiError;
    int32 _MaxDumps = 1;
};
