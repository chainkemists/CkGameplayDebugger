#pragma once

#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"

#include "Engine/DeveloperSettings.h"

#include "CkDebuggerStyleSettings.generated.h"

// ====================================================================================================================

/**
 * Per-user home of the Layer-B style selection (Editor Preferences -> Ck -> Debugger Style).
 *
 * Live apply is a POLLING model, deliberately: visual axes are re-read by widgets inside attribute
 * lambdas / paint, so they need no invalidation at all. Only STRUCTURAL axes need a nudge —
 * `Get_Revision()` is a transient counter that `NotifyChanged()` bumps; panels cache it and rebuild
 * when it moves. Nothing is broadcast, so there is no delegate lifetime to manage.
 */
UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Ck Debugger Style"))
class CKDEBUGGERCOMMON_API UCkDebuggerStyleSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // Bumped by any axis add / remove / reorder in CkDebuggerStyleSelection.h.
    // v2: + EditControlStyle (interactive inspector rows).
    // v3: + TreeComplexity (ECS entity tree declutter dial).
    // v4: + IconTreatment, TextScale, EntityRefStyle, CornerStyle, SurfaceElevation, GraphNodeStyle, RowBanding.
    // v5: - EntityIdStyle::HashTintedChip. The treatment it carried is EntityRefStyle::Pill; EntityIdStyle
    //     is now composition only. Ini stores entries by NAME, so a v4 config naming HashTintedChip
    //     falls back to the NameAndId default and keeps whatever EntityRefStyle it already had.
    static constexpr int32 CurrentSchemaVersion = 5;

public:
    UCkDebuggerStyleSettings();

public:
    // ----------------------------------------------------------------------------------------------------------------
    // SELECTION
    // ----------------------------------------------------------------------------------------------------------------

    UPROPERTY(config, EditAnywhere, Category = "Style Axes",
        meta = (DisplayName = "Selection",
            ToolTip = "Per-axis style choices shared by every Ck debugger surface. Colorless — colors always come from Editor Preferences -> Ck -> Style."))
    FCkDebuggerStyleSelection Selection;

    // ----------------------------------------------------------------------------------------------------------------
    // PROFILE
    // ----------------------------------------------------------------------------------------------------------------

    UPROPERTY(config, EditAnywhere, Category = "Profile",
        meta = (DisplayName = "Active Profile",
            ToolTip = "Name of the curated profile last applied. Any manual axis edit flips this to 'Custom'."))
    FString ActiveProfileName = TEXT("Classic");

    // ----------------------------------------------------------------------------------------------------------------
    // SCHEMA
    // ----------------------------------------------------------------------------------------------------------------

    UPROPERTY(config, VisibleAnywhere, Category = "Schema",
        meta = (DisplayName = "Schema Version",
            ToolTip = "Axis-catalog version this config was written against. An older value warns on load; parseable axes are kept, unknown ones fall back to their default."))
    int32 SchemaVersion = CurrentSchemaVersion;

public:
    // ----------------------------------------------------------------------------------------------------------------
    // ACCESSORS
    // ----------------------------------------------------------------------------------------------------------------

    static auto Get() -> const UCkDebuggerStyleSettings*
    {
        return GetDefault<UCkDebuggerStyleSettings>();
    }

    static auto Get_Mutable() -> UCkDebuggerStyleSettings*
    {
        return GetMutableDefault<UCkDebuggerStyleSettings>();
    }

    static auto Get_Selection() -> const FCkDebuggerStyleSelection&
    {
        return Get()->Selection;
    }

    auto Get_Revision() const -> uint32 { return _Revision; }

    // Bumps the revision so structural consumers rebuild on their next gated tick. Broadcasts nothing.
    auto NotifyChanged() -> void;

public:
    virtual auto PostInitProperties() -> void override;

#if WITH_EDITOR
    virtual auto PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) -> void override;
#endif

private:
    // Transient by construction — not a UPROPERTY, never serialized, resets to 0 each editor session.
    uint32 _Revision = 0;
};

// ====================================================================================================================
