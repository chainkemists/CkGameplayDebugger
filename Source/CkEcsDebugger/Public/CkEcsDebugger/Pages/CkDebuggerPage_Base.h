#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerModel_InspectorFilter;

struct FCkDebuggerPageContext
{
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TSharedPtr<FCkDebuggerModel_InspectorFilter> FilterModel;

    /** Pushes a query into the entity list's Filter bar (archetype card click-through). */
    TFunction<void(const FString&)> RequestEntityFilter;

    /** Reads the Filter bar's current text — cards derive their toggled state from it. */
    TFunction<FString()> GetEntityFilter;
};

class ICkDebuggerPage_Base
{
public:
    virtual ~ICkDebuggerPage_Base() = default;

    virtual auto Get_PageName() const -> FText = 0;
    virtual auto Get_PageIcon() const -> const FSlateBrush* = 0;
    virtual auto Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget> = 0;
    virtual auto Tick(float InDeltaTime) -> void = 0;
    virtual auto IsActive() const -> bool = 0;
    virtual auto Set_IsActive(bool InIsActive) -> void = 0;

    /** Unseen-count shown on the page's tab (0 = no badge). Activity uses it. */
    virtual auto Get_BadgeCount() const -> int32 { return 0; }

    /**
     * The Layer-B style selection changed structurally. Routed from the window's
     * SCkDebugger_WindowBase::OnStyleRevisionChanged rather than from a re-Build_Content:
     * Build_Content is NOT idempotent for every page (Overview roots a fresh UEdGraph and
     * re-binds its model delegates), so the window hands the change to the page instead of
     * re-running the builder.
     *
     * Default is a no-op — a page whose axis reads are all attribute-bound needs nothing.
     */
    virtual auto OnStyleRevisionChanged() -> void {}
};