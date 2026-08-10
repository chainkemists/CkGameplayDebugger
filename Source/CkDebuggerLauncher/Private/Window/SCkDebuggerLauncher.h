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

    // The rail is a plain SCompoundWidget — no window id, no refresh gate — so it carries its own
    // copy of SCkDebugger_WindowBase's style-revision watch instead of inheriting one.
    auto Poll_StyleRevision() -> void;

    static auto Get_CategoryDisplayName(ECkDebuggerToolCategory InCategory) -> FText;

    TSharedPtr<SVerticalBox> _ToolList;
    FDelegateHandle _RegistryChangedHandle;
    bool _ShowLabels = false;

    // Transient, like the revision itself — resets with the widget, never serialized.
    uint32 _LastSeenStyleRevision = 0;
};
