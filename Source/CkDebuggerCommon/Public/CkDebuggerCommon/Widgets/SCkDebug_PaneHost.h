#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

enum class ECkDebugPaneContent : uint8
{
    Passive,
    OpaqueRenderer,
};

// The suite-wide outer owner for splitter and fixed-rail panes.
//
// Cards: one rounded Common ring/surface plus the style-owned outer extent.
// Workbench: one square, ringless, zero-extent surface; the splitter or one fixed-layout separator owns the boundary.
//
// Passive content must not paint opaque pane chrome at its outer edge. Opaque renderers opt into a Cards-only inset so
// their semantic canvas/viewport fill cannot overwrite the rounded Common corners; that inset collapses in Workbench.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_PaneHost : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_PaneHost)
        : _ContentMode(ECkDebugPaneContent::Passive)
    {}
        SLATE_ARGUMENT(ECkDebugPaneContent, ContentMode)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
