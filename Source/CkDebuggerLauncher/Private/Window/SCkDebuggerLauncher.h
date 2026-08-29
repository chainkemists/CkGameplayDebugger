#pragma once

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Widgets/SCompoundWidget.h"

class SCkDebug_SearchBar;
class SVerticalBox;

// --------------------------------------------------------------------------------------------------------------------

// Bound only in EMBEDDED mode (the suite host). Reports which tool the user picked instead of the
// rail opening a global tab behind the host's back.
DECLARE_DELEGATE_OneParam(FCkDebuggerLauncher_OnToolSelected, FName);

// --------------------------------------------------------------------------------------------------------------------

class SCkDebuggerLauncher : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerLauncher) {}
        /**
         * Binding this switches the rail to embedded mode: a click reports the tool id and the
         * active marker follows SelectedToolId instead of the global tab manager. Unbound (the
         * default) is the standalone rail, unchanged.
         */
        SLATE_EVENT(FCkDebuggerLauncher_OnToolSelected, OnToolSelected)

        /** The embedded host's current selection, for a highlight that persists between clicks. */
        SLATE_ATTRIBUTE(FName, SelectedToolId)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkDebuggerLauncher() override;

    auto Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime) -> void override;

    auto OnKeyDown(
        const FGeometry& InAllottedGeometry,
        const FKeyEvent& InKeyEvent) -> FReply override;

private:
    auto RebuildTools() -> void;
    auto Build_ToolButton(const FCkDebuggerToolDescriptor& InTool) -> TSharedRef<SWidget>;
    auto Build_CategoryHeader(ECkDebuggerToolCategory InCategory, bool InAddSeparator) -> TSharedRef<SWidget>;
    auto Get_LabelVisibility() const -> EVisibility;
    auto Get_SearchVisibility() const -> EVisibility;

    auto Get_IsEmbedded() const -> bool;
    auto Get_SelectedToolId() const -> FName;
    auto Activate_Tool(FName InTabId) -> void;

    auto Handle_SearchTextChanged(const FString& InText) -> void;
    auto Handle_SearchTextCommitted(const FString& InText, ETextCommit::Type InCommitType) -> void;
    auto Clear_Search() -> void;

    // The rail is a plain SCompoundWidget — no window id, no refresh gate — so it carries its own
    // copy of SCkDebugger_WindowBase's style-revision watch instead of inheriting one.
    auto Poll_StyleRevision() -> void;

    static auto Get_CategoryDisplayName(ECkDebuggerToolCategory InCategory) -> FText;

    TSharedPtr<SVerticalBox> _ToolList;
    TSharedPtr<SCkDebug_SearchBar> _SearchBar;
    FDelegateHandle _RegistryChangedHandle;

    FCkDebuggerLauncher_OnToolSelected _OnToolSelected;
    TAttribute<FName> _SelectedToolId;

    FString _SearchQuery;

    // Rebuild order of what is currently on screen. Element 0 is the entry Enter activates.
    TArray<FName> _VisibleToolIds;

    bool _ShowLabels = false;

    // Transient, like the revision itself — resets with the widget, never serialized.
    uint32 _LastSeenStyleRevision = 0;
};

// --------------------------------------------------------------------------------------------------------------------
