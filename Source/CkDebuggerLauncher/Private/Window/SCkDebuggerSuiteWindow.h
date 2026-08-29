#pragma once

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Widgets/SCompoundWidget.h"

class SBox;
class SCkDebuggerLauncher;
class SDockTab;

// ====================================================================================================================
// One host window for the whole debugger suite: the category rail on the left, the selected tool's
// content on the right.
//
// The host NEVER re-parents a live global SDockTab. Selecting a tool resolves to exactly one of:
//
//   a) its global nomad tab is already open  -> focus that tab, show a "lives in its own tab" card.
//   b) its descriptor has a TabFactory       -> build the tab ONCE, keep the SDockTab alive in
//                                               _EmbeddedTools, and show TabRef->GetContent() here.
//                                               Switching tools swaps which cached content is
//                                               shown, so a tool the user comes back to still has
//                                               its state.
//   c) no factory                            -> invoke the global tab, same card as (a).
//
// The cached SDockTab is deliberately kept rather than discarded: it owns the content, and it is
// what the owning module's OnTabClosed callback expects to be handed when the embed is released.
// ====================================================================================================================

class SCkDebuggerSuiteWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerSuiteWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkDebuggerSuiteWindow() override;

    /**
     * Releases every embedded tool. InRunCloseCallbacks drives the owning modules' OnTabClosed:
     * true for a user-driven close (they must learn their window is gone), false on engine exit,
     * where those callbacks belong to modules that are already being torn down.
     */
    auto Release_AllEmbeddedTools(bool InRunCloseCallbacks) -> void;

private:
    auto Handle_ToolSelected(FName InTabId) -> void;
    auto Show_Tool(FName InTabId) -> void;
    auto Release_EmbeddedTool(FName InTabId, bool InRunCloseCallbacks) -> void;

    auto Handle_PopOutClicked() -> FReply;
    auto Handle_FocusExternalClicked() -> FReply;

    auto Get_SelectedToolId() const -> FName;
    auto Get_SelectedDisplayName() const -> FText;
    auto Get_PopOutVisibility() const -> EVisibility;

    auto Build_ExternalCard(const FCkDebuggerToolDescriptor& InTool) -> TSharedRef<SWidget>;
    auto Build_EmptyCard() -> TSharedRef<SWidget>;

    static auto TryFind_Tool(FName InTabId, FCkDebuggerToolDescriptor& OutTool) -> bool;

    TSharedPtr<SCkDebuggerLauncher> _Rail;
    TSharedPtr<SBox> _ContentHost;

    // TabId -> the factory-built tab whose content is on loan to _ContentHost. Owns the embedded
    // tool's lifetime; nothing else holds these.
    TMap<FName, TSharedPtr<SDockTab>> _EmbeddedTools;

    FName _SelectedToolId;

    // Cached at selection time. The title text is attribute-bound (read per frame), and resolving
    // it through the registry would copy and sort the whole catalog on every paint.
    FText _SelectedDisplayName;
};

// ====================================================================================================================
