#pragma once

#include "CkTextureDebugger/Analysis/CkTextureDebugger_MaterialAnalysis.h"
#include "CkTextureDebugger/Analysis/CkTextureDebugger_UvDensity.h"
#include "CkTextureDebugger/Data/CkTextureDebugger_Types.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_TextureHealthTable.h"

#include "CkSlateLayout/CkUiDocument.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class STableViewBase;
class SVerticalBox;
enum class ECk_Tone : uint8;
class FCkUiView;
class FCkUiCollection;
class SCkUiTable;
class SCkUiRepeat;

/** Runtime-safe UV-density capability and evidence surface. */
class CKTEXTUREDEBUGGER_API SCkTextureDebugger_UvDensityPage final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkTextureDebugger_UvDensityPage) {}
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs)
        -> void;

    auto Set_Context(
        TOptional<FCkTextureDebugger_ComponentRow> InComponent,
        TOptional<FCkTextureDebugger_TextureHealthSelection> InSelectedTexture,
        TArray<int32> InExplicitSlotIndices) -> void;

    auto
    TryReload_Layout(
        const FString& InMarkup,
        const FString& InStylesheet)
        -> FCkUiLoadResult;

    auto
    Reload_LayoutFiles(
        const FString& InMarkupPath,
        const FString& InStylesheetPath)
        -> FCkUiLoadResult;

    auto Poll_LayoutFiles() -> bool;
    auto Get_LayoutRevision() const -> int64;
    auto Get_LayoutError() const -> FText;

private:
    auto Refresh_Result() -> void;
    auto Get_ComponentContextText() const -> FText;
    auto Get_SlotContextText() const -> FText;
    auto Get_TextureContextText() const -> FText;
    auto Get_InputsTitleText() const -> FText;
    auto Get_ResultTitleText() const -> FText;
    auto Get_ResultStatusText() const -> FText;
    auto Get_ResultTone() const -> ECk_Tone;
    auto Get_ResultValueText() const -> FText;
    auto Get_ResultExplanationText() const -> FText;
    auto Are_InputsExpanded() const -> bool;
    auto Is_ResultExpanded() const -> bool;
    auto Toggle_InputsExpanded() -> void;
    auto Toggle_ResultExpanded() -> void;
    auto
    Tick_LayoutFiles(
        double InCurrentTime,
        float InDeltaTime)
        -> EActiveTimerReturnType;

    TSharedPtr<FCkUiView> _LayoutView;
    FString _PublicationError;
    TOptional<FCkTextureDebugger_ComponentRow> _Component;
    TOptional<FCkTextureDebugger_TextureHealthSelection> _SelectedTexture;
    TArray<int32> _ExplicitSlotIndices;
    FCkTextureDebugger_UvDensityResult _Result;
    bool _InputsExpanded = true;
    bool _ResultExpanded = true;
};

// --------------------------------------------------------------------------------------------------------------------

/** Runtime material-input table with independent filter and highlight queries. */
class CKTEXTUREDEBUGGER_API SCkTextureDebugger_MaterialInputsPage final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkTextureDebugger_MaterialInputsPage) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto Set_Context(
        TOptional<FCkTextureDebugger_ComponentRow> InComponent,
        TOptional<FCkTextureDebugger_TextureHealthSelection> InSelectedTexture,
        TArray<int32> InExplicitSlotIndices) -> void;

    auto TryReload_Layout(const FString& InMarkup, const FString& InStylesheet) -> FCkUiLoadResult;
    auto Reload_LayoutFiles(const FString& InMarkupPath, const FString& InStylesheetPath) -> FCkUiLoadResult;
    auto Poll_LayoutFiles() -> bool;
    auto Get_LayoutRevision() const -> int64;
    auto Get_LayoutError() const -> FText;
    auto Get_AuthoredTable() const -> TSharedPtr<SCkUiTable>;

public:
    struct FRow
    {
        FString StableKey;
        FString Parameter;
        FString Texture;
        FString TexturePath;
        FString Provenance;
        FString Detail;
        FString Slot;
        FString Variant;
        FString SearchText;
        int32 SlotIndex = INDEX_NONE;
        ECkTextureDebugger_MaterialTextureProvenance ProvenanceKind =
            ECkTextureDebugger_MaterialTextureProvenance::Unavailable;
        bool IsDimmed = false;
        bool IsHighlighted = false;
    };

private:
    auto Rebuild_Rows() -> void;
    auto Apply_Search() -> void;
    auto OnFilterTextChanged(const FString& InText) -> void;
    auto OnHighlightTextChanged(const FString& InText) -> void;
    auto Get_EmptyStateText() const -> FText;
    auto Publish_Rows() -> void;
    auto Tick_LayoutFiles(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType;

    TOptional<FCkTextureDebugger_ComponentRow> _Component;
    TOptional<FCkTextureDebugger_TextureHealthSelection> _SelectedTexture;
    TArray<int32> _ExplicitSlotIndices;
    TArray<TSharedPtr<FRow>> _AllRows;
    TArray<TSharedPtr<FRow>> _VisibleRows;
    TSharedPtr<FCkUiView> _LayoutView;
    TSharedPtr<FCkUiCollection> _UiCollection;
    FString _PublicationError;
    FString _FilterText;
    FString _HighlightText;
};

// --------------------------------------------------------------------------------------------------------------------

/** Per-slot material, shadow, lighting, decal, lightmap, and Nanite facts. */
class CKTEXTUREDEBUGGER_API SCkTextureDebugger_SurfaceLightingPage final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkTextureDebugger_SurfaceLightingPage) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto Set_Context(
        TOptional<FCkTextureDebugger_ComponentRow> InComponent,
        TOptional<FCkTextureDebugger_TextureHealthSelection> InSelectedTexture,
        TArray<int32> InExplicitSlotIndices) -> void;

    auto TryReload_Layout(const FString& InMarkup, const FString& InStylesheet) -> FCkUiLoadResult;
    auto Reload_LayoutFiles(const FString& InMarkupPath, const FString& InStylesheetPath) -> FCkUiLoadResult;
    auto Poll_LayoutFiles() -> bool;
    auto Get_LayoutRevision() const -> int64;
    auto Get_LayoutError() const -> FText;
    auto Get_AuthoredRepeat() const -> TSharedPtr<SCkUiRepeat>;

private:
    struct FSlotFacts
    {
        FString StableKey;
        int32 SlotIndex = INDEX_NONE;
        FString MaterialName;
        FString MaterialPath;
        FString BlendMode;
        FString ShadingModels;
        FString LightMapResolution;
        FString Nanite;
        bool HasMaterial = false;
        bool IsTwoSided = false;
        bool IsMasked = false;
        bool IsTranslucent = false;
        float OpacityMaskClipValue = 0.0f;
        bool CastsShadow = false;
        bool CastsDynamicShadow = false;
        bool CastsStaticShadow = false;
        bool CastsVolumetricTranslucentShadow = false;
        bool ReceivesDecals = false;
        bool HasStaticLighting = false;
        bool Expanded = true;
    };

    auto Rebuild_Facts() -> void;
    auto Publish_Facts() -> void;
    auto Toggle_Slot(const FString& InStableKey) -> void;
    auto Get_EmptyStateText() const -> FText;
    auto Tick_LayoutFiles(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType;

    TOptional<FCkTextureDebugger_ComponentRow> _Component;
    TOptional<FCkTextureDebugger_TextureHealthSelection> _SelectedTexture;
    TArray<int32> _ExplicitSlotIndices;
    TArray<TSharedPtr<FSlotFacts>> _SlotFacts;
    TSharedPtr<FCkUiView> _LayoutView;
    TSharedPtr<FCkUiCollection> _UiCollection;
    FString _PublicationError;
};
