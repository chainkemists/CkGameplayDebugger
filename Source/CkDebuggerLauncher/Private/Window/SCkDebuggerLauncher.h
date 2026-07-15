#pragma once

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

// --------------------------------------------------------------------------------------------------------------------

class SCkDebuggerLauncher : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerLauncher) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkDebuggerLauncher() override;

    auto Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime) -> void override;

private:
    auto RebuildTools() -> void;
    auto Build_ToolButton(const FCkDebuggerToolDescriptor& InTool) -> TSharedRef<SWidget>;
    auto Build_CategoryHeader(ECkDebuggerToolCategory InCategory, bool InAddSeparator) -> TSharedRef<SWidget>;
    auto Get_LabelVisibility() const -> EVisibility;

    static auto Get_CategoryDisplayName(ECkDebuggerToolCategory InCategory) -> FText;

    TSharedPtr<SVerticalBox> _ToolList;
    FDelegateHandle _RegistryChangedHandle;
    bool _ShowLabels = false;
};
