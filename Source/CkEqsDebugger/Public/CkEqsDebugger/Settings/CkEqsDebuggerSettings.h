#pragma once

#include "Engine/DeveloperSettings.h"

#include "CkEqsDebuggerSettings.generated.h"

// ====================================================================================================================
// Per-user (editor-local) settings for the EQS debugger. Lives in EditorPerProjectUserSettings so toggles persist
// across editor sessions but are NOT checked into source. Read via GetDefault<>; mutate via GetMutableDefault<> +
// SaveConfig() (the toolbar's View-menu checkboxes do exactly that on click).
// ====================================================================================================================

UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig, meta = (DisplayName = "Ck EQS Debugger"))
class CKEQSDEBUGGER_API UCkEqsDebuggerSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual auto GetCategoryName() const -> FName override { return TEXT("CkGameplayDebugger"); }

    // ---- Master overlay toggle ------------------------------------------------------------------------------------

    // Master switch — when off, no in-world overlay is drawn regardless of the per-feature toggles below. Useful for
    // quickly suppressing all debug visualization while keeping the panel open.
    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay",
        meta = (DisplayName = "Show In-World Overlay",
            ToolTip = "Master toggle. When off, no spheres / lines / markers are drawn for the selected query."))
    bool bShow_Overlay = true;

    // ---- Per-feature toggles --------------------------------------------------------------------------------------

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay",
        meta = (DisplayName = "Show All Queries (ignore selection)",
            ToolTip = "When on, draw the overlay for every query in the world, not just the row currently selected in the debugger panel. Useful for 'just show me everything happening right now' without having to click a row first. The selection-driven inspection panels (candidate list / test breakdown) are unaffected and still operate on the selected query.",
            EditCondition = "bShow_Overlay"))
    bool bShow_AllQueriesAlways = false;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay",
        meta = (DisplayName = "Show All Candidate Spheres",
            ToolTip = "Draw a sphere at every candidate's location, color-lerped by final score (blue=low, green=high).",
            EditCondition = "bShow_Overlay"))
    bool bShow_AllCandidateSpheres = true;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay",
        meta = (DisplayName = "Highlight Best Candidate",
            ToolTip = "Highlight the top-scoring candidate with a larger amber sphere.",
            EditCondition = "bShow_Overlay"))
    bool bShow_BestCandidateHighlight = true;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay",
        meta = (DisplayName = "Show Filter-Failed Candidates",
            ToolTip = "Draw filter-failed candidates as muted gray spheres (so you can see WHY they failed). Off = only show passing candidates.",
            EditCondition = "bShow_Overlay"))
    bool bShow_FailedCandidates = false;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay",
        meta = (DisplayName = "Show Querier Marker",
            ToolTip = "Draw a small sphere at the querier's location.",
            EditCondition = "bShow_Overlay"))
    bool bShow_QuerierMarker = true;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay",
        meta = (DisplayName = "Show Best-Location Line",
            ToolTip = "Draw a line from the querier to the best candidate location.",
            EditCondition = "bShow_Overlay"))
    bool bShow_BestLocationLine = true;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay",
        meta = (DisplayName = "Show Grid Lines (SimpleGrid / Grid)",
            ToolTip = "For SimpleGrid and Grid generators, draw a wireframe lattice connecting the cells. Mirrors UE EQS's debug visualization. Skipped for Donut / Cone / EntitiesWithTag (no grid structure).",
            EditCondition = "bShow_Overlay"))
    bool bShow_GridLines = true;

    // ---- Sizing ---------------------------------------------------------------------------------------------------

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay - Sizing",
        meta = (DisplayName = "Candidate Sphere Radius (cm)",
            ClampMin = "1.0", UIMin = "1.0", UIMax = "200.0",
            EditCondition = "bShow_Overlay"))
    float CandidateSphereRadius = 16.0f;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay - Sizing",
        meta = (DisplayName = "Best Pick Sphere Radius (cm)",
            ClampMin = "1.0", UIMin = "1.0", UIMax = "300.0",
            EditCondition = "bShow_Overlay"))
    float BestPickSphereRadius = 28.0f;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay - Sizing",
        meta = (DisplayName = "Querier Marker Radius (cm)",
            ClampMin = "1.0", UIMin = "1.0", UIMax = "200.0",
            EditCondition = "bShow_Overlay"))
    float QuerierMarkerRadius = 24.0f;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay - Sizing",
        meta = (DisplayName = "Best-Location Line Thickness",
            ClampMin = "0.5", UIMin = "0.5", UIMax = "10.0",
            EditCondition = "bShow_Overlay"))
    float BestLocationLineThickness = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category = "In-World Overlay - Sizing",
        meta = (DisplayName = "Grid Line Thickness",
            ClampMin = "0.5", UIMin = "0.5", UIMax = "10.0",
            EditCondition = "bShow_Overlay"))
    float GridLineThickness = 1.5f;

    // ---- Accessors ------------------------------------------------------------------------------------------------

    static auto Get() -> const UCkEqsDebuggerSettings*
    {
        return GetDefault<UCkEqsDebuggerSettings>();
    }

    static auto GetMutable() -> UCkEqsDebuggerSettings*
    {
        return GetMutableDefault<UCkEqsDebuggerSettings>();
    }
};

// ====================================================================================================================
