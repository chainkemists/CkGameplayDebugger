#pragma once

#include "Engine/DeveloperSettings.h"

#include "CkInputHud_UserSettings.generated.h"

// ====================================================================================================================

UENUM(BlueprintType)
enum class ECk_InputHud_Palette : uint8
{
    ArcticSignal,
    EmberTerminal,
    OrchidSynth,
    TacticalMint,
};

UENUM(BlueprintType)
enum class ECk_InputHud_Density : uint8
{
    Compact,
    Standard,
    Readable,
};

UENUM(BlueprintType)
enum class ECk_InputHud_MetadataMode : uint8
{
    Keys,
    Compact,
    Full,
};

UENUM(BlueprintType)
enum class ECk_InputHud_FrameNotation : uint8
{
    Press,
    Delta,
    Range,
};

UENUM(BlueprintType)
enum class ECk_InputHud_CornerStyle : uint8
{
    Sharp,
    Soft,
    Rounded,
};

// Which viewport corner the overlay is pinned to. NOT to be confused with ECk_InputHud_CornerStyle, which is the
// key cap's corner RADIUS preset — this one is where the strip sits on screen.
//
// The values deliberately match the ck.InputOverlay.Corner cvar's existing 0-3 encoding, so the console, the QA
// panel's segmented control, and this setting all speak the same numbers.
UENUM(BlueprintType)
enum class ECk_InputHud_AnchorCorner : uint8
{
    TopLeft     = 0,
    TopRight    = 1,
    BottomLeft  = 2,
    BottomRight = 3,
};

UENUM(BlueprintType)
enum class ECk_InputHud_ColorRole : uint8
{
    KeyBorder,
    ContainerOutline,
    Active,
    Resolved,
    Unrouted,
};

// --------------------------------------------------------------------------------------------------------------------

/** Fully-derived feature-local palette. Colors remain semantic inside this HUD and never mutate global CkStyle roles. */
struct FCk_InputHud_PaletteSnapshot
{
    FLinearColor Panel;
    FLinearColor HistoryFill;
    FLinearColor HistoryText;
    FLinearColor ContainerOutline;
    FLinearColor KeyBorder;
    FLinearColor Active;
    FLinearColor Resolved;
    FLinearColor Unrouted;
    FLinearColor Metadata;
};

// ====================================================================================================================

/** Per-user Input HUD presentation/readout tuning. Runtime code polls this packaged-capable store; Style Lab and Intent
 * Debugger are one-way editing surfaces and never own duplicate state. */
UCLASS(Config=GameUserSettings, meta=(DisplayName="Ck Input HUD"))
class CKINPUTHUDOVERLAY_API UCk_InputHud_UserSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // v1 introduced the feature-local presentation settings. v2 raises the three weak visual defaults only when
    // a stored value still exactly matches its v1 default. v3 translates the old corner preset into a numeric radius
    // and raises the press-pop defaults when their v2 values remain exactly untouched. v4 tunes the untouched Compact
    // defaults without changing a user-authored geometry, history-brightness, or border-opacity choice. v5 splits
    // the former shared outline customization into independent container and key outline roles. v6 promotes the
    // visually accepted Signal Strip tuning to the authored defaults.
    static constexpr int32 CurrentVisualSchemaVersion = 6;

public:
    UCk_InputHud_UserSettings();

    virtual auto
    GetContainerName() const -> FName override
    {
        return TEXT("Editor");
    }

    virtual auto
    GetCategoryName() const -> FName override
    {
        return TEXT("Ck");
    }

public:
    UPROPERTY(Config, EditAnywhere, Category="Appearance")
    ECk_InputHud_Palette Palette = ECk_InputHud_Palette::ArcticSignal;

    UPROPERTY(Config, EditAnywhere, Category="Appearance")
    ECk_InputHud_Density Density = ECk_InputHud_Density::Compact;

    UPROPERTY(Config, EditAnywhere, Category="Readout")
    ECk_InputHud_MetadataMode MetadataMode = ECk_InputHud_MetadataMode::Full;

    UPROPERTY(Config, EditAnywhere, Category="Readout")
    ECk_InputHud_FrameNotation FrameNotation = ECk_InputHud_FrameNotation::Delta;

    UPROPERTY(Config, EditAnywhere, Category="Appearance")
    ECk_InputHud_CornerStyle CornerStyle = ECk_InputHud_CornerStyle::Rounded;

    /** Viewport corner the overlay is pinned to. Persisted, so a QA choice survives relaunch. */
    UPROPERTY(Config, EditAnywhere, Category="Layout")
    ECk_InputHud_AnchorCorner AnchorCorner = ECk_InputHud_AnchorCorner::TopRight;

    /** Distance INWARD from the anchored corner, in Slate units. Always positive: on a left anchor it pushes the
     * overlay right, on a right anchor it pushes it left. Switching corners therefore mirrors the overlay rather
     * than flinging it off screen. Negatives are refused - an overlay off the edge cannot be read or reported. */
    UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(ClampMin="0.0", ClampMax="512.0"))
    float AnchorOffsetX = 6.0f;

    /** Distance INWARD from the anchored corner, in Slate units. See AnchorOffsetX. */
    UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(ClampMin="0.0", ClampMax="512.0"))
    float AnchorOffsetY = 4.0f;

    UPROPERTY(Config, EditAnywhere, Category="Geometry", meta=(ClampMin="0.0", ClampMax="12.0"))
    float KeyPaddingX = 5.0f;

    UPROPERTY(Config, EditAnywhere, Category="Geometry", meta=(ClampMin="0.0", ClampMax="6.0"))
    float KeyPaddingY = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category="Geometry", meta=(ClampMin="0.0", ClampMax="12.0"))
    float KeyCornerRadius = 3.0f;

    /** Opacity of the whole Signal Strip against the game - panel, keys and readouts fade together, so lowering this
     * reveals gameplay behind the overlay. Multiplies with the ck.InputOverlay.Opacity cvar and the idle fade-out. */
    UPROPERTY(Config, EditAnywhere, Category="Appearance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float OverallOpacity = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category="Appearance", meta=(ClampMin="0.0", ClampMax="2.0"))
    float KeyBorderWidth = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category="Appearance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float KeyBorderOpacity = 0.50f;

    UPROPERTY(Config, EditAnywhere, Category="Appearance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ActiveFillOpacity = 0.92f;

    UPROPERTY(Config, EditAnywhere, Category="Appearance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ActiveGlowOpacity = 0.10f;

    UPROPERTY(Config, EditAnywhere, Category="Appearance", meta=(ClampMin="0.15", ClampMax="1.0"))
    float PanelOpacity = 0.94f;

    UPROPERTY(Config, EditAnywhere, Category="Pulse", meta=(ClampMin="0.0"))
    float PulseScale = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category="Animation", meta=(ClampMin="0.15", ClampMax="1.0"))
    float HistoryBrightness = 0.82f;

    UPROPERTY(Config, EditAnywhere, Category="Animation", meta=(ClampMin="0.0"))
    float PressPopScale = 1.20f;

    UPROPERTY(Config, EditAnywhere, Category="Animation", meta=(ClampMin="0.0", ClampMax="500.0"))
    float PressPopDurationMs = 250.0f;

    UPROPERTY(Config, EditAnywhere, Category="Animation", meta=(ClampMin="0.0", ClampMax="1000.0"))
    float ReleaseEaseMs = 320.0f;

    UPROPERTY(Config, EditAnywhere, Category="Colors")
    bool UseCustomColors = false;

    UPROPERTY(Config, EditAnywhere, Category="Colors", meta=(EditCondition="UseCustomColors"))
    FLinearColor CustomKeyBorder;

    UPROPERTY(Config, EditAnywhere, Category="Colors", meta=(EditCondition="UseCustomColors"))
    FLinearColor CustomContainerOutline;

    UPROPERTY(Config, EditAnywhere, Category="Colors", meta=(EditCondition="UseCustomColors"))
    FLinearColor CustomActive;

    UPROPERTY(Config, EditAnywhere, Category="Colors", meta=(EditCondition="UseCustomColors"))
    FLinearColor CustomResolved;

    UPROPERTY(Config, EditAnywhere, Category="Colors", meta=(EditCondition="UseCustomColors"))
    FLinearColor CustomUnrouted;

public:
    static auto
    Get() -> const UCk_InputHud_UserSettings*
    {
        return GetDefault<UCk_InputHud_UserSettings>();
    }

    static auto
    Get_Mutable() -> UCk_InputHud_UserSettings*
    {
        return GetMutableDefault<UCk_InputHud_UserSettings>();
    }

    static auto
    Get_PaletteSnapshot() -> FCk_InputHud_PaletteSnapshot;

    static auto
    Get_Revision() -> uint32
    {
        return Get()->_Revision;
    }

    static auto
    Get_Density() -> ECk_InputHud_Density;

    static auto
    Get_MetadataMode() -> ECk_InputHud_MetadataMode;

    static auto
    Get_FrameNotation() -> ECk_InputHud_FrameNotation;

    static auto
    Get_CornerStyle() -> ECk_InputHud_CornerStyle;

    static auto
    Get_AnchorCorner() -> ECk_InputHud_AnchorCorner;

    static auto
    Get_AnchorOffsetX() -> float;

    static auto
    Get_AnchorOffsetY() -> float;

    static auto
    Get_KeyPaddingX() -> float;

    static auto
    Get_KeyPaddingY() -> float;

    static auto
    Get_KeyCornerRadius() -> float;

    static auto
    Get_OverallOpacity() -> float;

    static auto
    Get_KeyBorderWidth() -> float;

    static auto
    Get_KeyBorderOpacity() -> float;

    static auto
    Get_ActiveFillOpacity() -> float;

    static auto
    Get_ActiveGlowOpacity() -> float;

    static auto
    Get_PanelOpacity() -> float;

    static auto
    Get_PulseScale() -> float;

    static auto
    Get_HistoryBrightness() -> float;

    static auto
    Get_PressPopScale() -> float;

    static auto
    Get_PressPopDurationMs() -> float;

    static auto
    Get_ReleaseEaseMs() -> float;

    auto
    Set_Palette(ECk_InputHud_Palette InValue) -> void;

    auto
    Set_Density(ECk_InputHud_Density InValue) -> void;

    auto
    Set_MetadataMode(ECk_InputHud_MetadataMode InValue) -> void;

    auto
    Set_FrameNotation(ECk_InputHud_FrameNotation InValue) -> void;

    auto
    Set_CornerStyle(ECk_InputHud_CornerStyle InValue) -> void;

    auto
    Set_AnchorCorner(ECk_InputHud_AnchorCorner InValue) -> void;

    auto
    Set_AnchorOffsetX(float InValue) -> void;

    auto
    Set_AnchorOffsetY(float InValue) -> void;

    auto
    Set_KeyPaddingX(float InValue) -> void;

    auto
    Set_KeyPaddingY(float InValue) -> void;

    auto
    Set_KeyCornerRadius(float InValue) -> void;

    auto
    Set_OverallOpacity(float InValue) -> void;

    auto
    Set_KeyBorderWidth(float InValue) -> void;

    auto
    Set_KeyBorderOpacity(float InValue) -> void;

    auto
    Set_ActiveFillOpacity(float InValue) -> void;

    auto
    Set_ActiveGlowOpacity(float InValue) -> void;

    auto
    Set_PanelOpacity(float InValue) -> void;

    auto
    Set_PulseScale(float InValue) -> void;

    auto
    Set_HistoryBrightness(float InValue) -> void;

    auto
    Set_PressPopScale(float InValue) -> void;

    auto
    Set_PressPopDurationMs(float InValue) -> void;

    auto
    Set_ReleaseEaseMs(float InValue) -> void;

    auto
    Set_CustomColor(ECk_InputHud_ColorRole InRole, FLinearColor InValue) -> void;

    auto
    Reset_CustomColors() -> void;

    auto
    Reset_VisualTuning() -> void;

    auto
    Reset_ReadoutTuning() -> void;

    /** Applies visual-default migrations. Exact prior defaults are treated as inherited defaults. */
    auto
    Migrate_VisualSettingsIfNeeded() -> void;

    /** Invalidate layout/paint after a project-level setting changes through an owning debugger surface. */
    auto
    NotifyChanged() -> void;

#if WITH_EDITOR
    virtual auto
    PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) -> void override;
#endif

private:
    auto
    Commit_IfChanged(bool InChanged) -> void;

    auto
    Snapshot_CustomColorsIfNeeded() -> bool;

private:
    UPROPERTY(Config)
    int32 VisualSchemaVersion = 1;

    uint32 _Revision = 0;
};

// ====================================================================================================================
