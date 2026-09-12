#pragma once

#include "InputCoreTypes.h"
#include "Templates/Function.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SWidget;
class SEditableTextBox;
struct FKeyEvent;

// ====================================================================================================================

/** Compact runtime drawer for entity-selection policy. It owns only preference editing and action
 * callbacks: all entity lifetime, selection state, and input routing remain in the subsystem. */
class CKENTITYDEBUGOVERLAY_API SCkDebugOverlay_SelectionPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebugOverlay_SelectionPanel)
        : _StatusText(FText::GetEmpty())
        , _ExplanationText(FText::GetEmpty())
    {}
        SLATE_EVENT(FSimpleDelegate, OnClose)
        SLATE_ARGUMENT(TFunction<void()>, OnSelect)
        SLATE_ARGUMENT(TFunction<void()>, OnPrevious)
        SLATE_ARGUMENT(TFunction<void()>, OnNext)
        SLATE_ARGUMENT(TFunction<void()>, OnFamily)
        SLATE_ATTRIBUTE(FText, StatusText)
        SLATE_ATTRIBUTE(FText, ExplanationText)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto SupportsKeyboardFocus() const -> bool override { return true; }
    virtual auto OnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
    virtual auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;

private:
    auto Build_PolicySection() -> TSharedRef<SWidget>;
    auto Build_TuningSection() -> TSharedRef<SWidget>;
    auto Build_PresentationSection() -> TSharedRef<SWidget>;
    auto Build_OverlaySection() -> TSharedRef<SWidget>;
    auto Build_AttributesSection() -> TSharedRef<SWidget>;
    auto Build_WorldTagsSection() -> TSharedRef<SWidget>;
    auto Build_InputSection() -> TSharedRef<SWidget>;
    auto Build_NamedPresetSection() -> TSharedRef<SWidget>;
    auto Make_Section(const FText& InTitle, const TSharedRef<SWidget>& InContent) const -> TSharedRef<SWidget>;
    auto Make_CycleRow(const FText& InLabel, TAttribute<FText> InValue, TFunction<void()> InOnClicked) const -> TSharedRef<SWidget>;
    auto Make_ToggleRow(const FText& InLabel, TAttribute<bool> InValue, TFunction<void(bool)> InOnChanged) const -> TSharedRef<SWidget>;
    auto Make_SliderRow(const FText& InLabel, TAttribute<float> InValue, float InMin, float InMax, TFunction<void(float)> InOnChanged) const -> TSharedRef<SWidget>;
    auto Make_IntegerRow(const FText& InLabel, TAttribute<int32> InValue, int32 InMin, int32 InMax, TFunction<void(int32)> InOnChanged) const -> TSharedRef<SWidget>;
    auto Make_ActionButton(const FText& InLabel, TFunction<void()> InAction) const -> TSharedRef<SWidget>;
    auto Mutate_Config(TFunction<void(struct FCk_DebugOverlay_SelectionConfig&)> InMutation, bool InSave = true) const -> void;
    auto Begin_BindingCapture(FName InBinding) -> FReply;
    auto Set_Binding(FName InBinding, const FKey& InKey) -> void;
    auto Get_BindingText(FName InBinding) const -> FText;
    auto Get_StatusText() const -> FText;

    FSimpleDelegate _OnClose;
    TFunction<void()> _OnSelect;
    TFunction<void()> _OnPrevious;
    TFunction<void()> _OnNext;
    TFunction<void()> _OnFamily;
    TAttribute<FText> _StatusText;
    TAttribute<FText> _ExplanationText;
    FName _BindingToCapture;
    TSharedPtr<SEditableTextBox> _PresetNameText;
    FString _PanelError;
};

// ====================================================================================================================
