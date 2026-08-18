#include "SCkDebug_ViewportPickerControls.h"

#include "CkDebuggerCommon/Markers/CkDebug_EntityMarkers.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "HAL/IConsoleManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_viewport_picker_controls
{
    auto Get_BodyFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody());
    }

    auto Get_TinyLabelFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall());
    }

    auto Get_GlyphFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody());
    }

    auto Make_RowLabel(const TCHAR* InLabel) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(110.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(InLabel))
                .Font_Static(&Get_BodyFont)
                .ColorAndOpacity(FSlateColor(CkStyle::Text()))
            ];
    }
}

// =====================================================================================================================

auto
    SCkDebug_ViewportPickerControls::
    Construct(
        const FArguments& InArgs) -> void
{
    _Picker              = InArgs._Picker;
    _PickTooltip         = InArgs._PickTooltip;
    _ExtraSettingsContent = InArgs._ExtraSettingsContent.Widget;

    ChildSlot
    [
        SNew(SHorizontalBox)

        // ---- Pick toggle button ----
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
        [
            SNew(SButton)
            .ButtonColorAndOpacity_Lambda([this]() -> FLinearColor
            {
                return _Picker.IsValid() && _Picker->IsActive()
                    ? CkStyle::Selection()
                    : CkStyle::Bg2();
            })
            .ForegroundColor_Lambda([this]() -> FSlateColor
            {
                return _Picker.IsValid() && _Picker->IsActive()
                    ? FSlateColor(CkStyle::TextStrong())
                    : FSlateColor(CkStyle::TextDim());
            })
            .IsEnabled_Lambda([this]() -> bool
            {
                if (NOT _Picker.IsValid())
                { return false; }

                if (_Picker->IsActive())
                { return true; }

                return _Picker->CanActivate();
            })
            .ToolTipText_Lambda([this]() -> FText
            {
                if (NOT _Picker.IsValid())
                { return FText::GetEmpty(); }

                if (_Picker->IsActive())
                {
                    return FText::FromString(TEXT("Exit pick mode (Esc)"));
                }

                if (NOT _Picker->CanActivate())
                {
                    return FText::FromString(TEXT(
                        "Pick mode unavailable — select a running PIE or Game world first.\n"
                        "(Simulate-in-Editor is not supported.)"));
                }

                if (NOT _PickTooltip.IsEmpty())
                { return _PickTooltip; }

                return FText::FromString(TEXT(
                    "Enter pick mode: click an entity in the viewport to select it in the debugger."));
            })
            .OnClicked_Lambda([this]() -> FReply
            {
                if (_Picker.IsValid())
                {
                    _Picker->Toggle();
                }
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    return _Picker.IsValid() && _Picker->IsActive()
                        ? FText::FromString(TEXT("Picking..."))
                        : FText::FromString(TEXT("Pick"));
                })
                .Font_Static(&ck_debug_viewport_picker_controls::Get_BodyFont)
            ]
        ]

        // ---- Gear → picker settings popover ----
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SAssignNew(_SettingsAnchor, SMenuAnchor)
            .Placement(MenuPlacement_BelowAnchor)
            .OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
            {
                return Build_SettingsPopover();
            })
            [
                SNew(SButton)
                .ButtonColorAndOpacity(CkStyle::Bg2())
                .ToolTipText(FText::FromString(TEXT("Viewport picker settings")))
                .OnClicked_Lambda([this]() -> FReply
                {
                    if (_SettingsAnchor.IsValid())
                    {
                        _SettingsAnchor->SetIsOpen(NOT _SettingsAnchor->IsOpen());
                    }
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("⚙"))) // gear glyph
                    .Font_Static(&ck_debug_viewport_picker_controls::Get_GlyphFont)
                ]
            ]
        ]
    ];
}

// =====================================================================================================================

auto
    SCkDebug_ViewportPickerControls::
    Build_SettingsPopover() -> TSharedRef<SWidget>
{
    using namespace ck_debug_viewport_picker_controls;

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Medium"))
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 2.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("VIEWPORT PICKER")))
                .Font_Static(&Get_TinyLabelFont)
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            ]

            // ---- Ignore Self toggle ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SCkDebug_IconToggle)
                .IconId(ECk_Icon::Orphaned)
                .Label(FText::FromString(TEXT("Ignore Self")))
                .ToolTip(FText::FromString(TEXT(
                    "Ignore entities that belong to the locally controlled pawn\n"
                    "(including attached actors and child ECS entities).\n"
                    "Useful to avoid picking your own first-person viewpoint entity.")))
                .IsOn_Lambda([this]() -> bool
                {
                    return _Picker.IsValid() && _Picker->Get_IgnoreLocalPawn();
                })
                .OnStateChanged_Lambda([this](bool InIsOn)
                {
                    if (_Picker.IsValid())
                    { _Picker->Set_IgnoreLocalPawn(InIsOn); }
                })
                .ShowLabel(true)
            ]

            // ---- Meshes First toggle ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SCkDebug_IconToggle)
                .IconId(ECk_Icon::SceneNode)
                .Label(FText::FromString(TEXT("Meshes First")))
                .ToolTip(FText::FromString(TEXT(
                    "Hide the diamond billboards of entities that are pickable by their\n"
                    "rendered geometry (ISM-instance- or actor-backed). Diamonds remain only\n"
                    "on meshless entities; everything stays pickable.")))
                .IsOn_Lambda([this]() -> bool
                {
                    return _Picker.IsValid() && _Picker->Get_MeshesFirst();
                })
                .OnStateChanged_Lambda([this](bool InIsOn)
                {
                    if (_Picker.IsValid())
                    { _Picker->Set_MeshesFirst(InIsOn); }
                })
                .ShowLabel(true)
            ]

            // ---- Cull Radius spinbox ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    Make_RowLabel(TEXT("Cull Radius:"))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(120.0f)
                    [
                        SNew(SSpinBox<float>)
                        .MinValue(100.0f)
                        .MaxValue(100000.0f)
                        .MinSliderValue(500.0f)
                        .MaxSliderValue(20000.0f)
                        .Delta(100.0f)
                        .Value_Lambda([this]() -> float
                        {
                            return _Picker.IsValid() ? _Picker->Get_CullRadius() : 0.0f;
                        })
                        .OnValueChanged_Lambda([this](float InValue)
                        {
                            if (_Picker.IsValid())
                            { _Picker->Set_CullRadius(InValue); }
                        })
                        .ToolTipText(FText::FromString(TEXT(
                            "Maximum distance (cm) from the camera at which entities are\n"
                            "drawn and considered for picking. Lower values reduce clutter\n"
                            "in large worlds.")))
                    ]
                ]
            ]

            // ---- Billboard Size spinbox ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    Make_RowLabel(TEXT("Billboard Size:"))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(120.0f)
                    [
                        SNew(SSpinBox<float>)
                        .MinValue(8.0f)
                        .MaxValue(128.0f)
                        .MinSliderValue(8.0f)
                        .MaxSliderValue(128.0f)
                        .Delta(1.0f)
                        .Value_Lambda([this]() -> float
                        {
                            return _Picker.IsValid() ? _Picker->Get_BillboardSize() : 0.0f;
                        })
                        .OnValueChanged_Lambda([this](float InValue)
                        {
                            if (_Picker.IsValid())
                            { _Picker->Set_BillboardSize(InValue); }
                        })
                        .ToolTipText(FText::FromString(TEXT(
                            "Size (pixels) of each entity billboard. Billboards keep the same\n"
                            "screen size regardless of distance to the camera.")))
                    ]
                ]
            ]

            // ---- Max Depth spinbox (shared cvar — linked to the On-Screen Overlay) ----
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    Make_RowLabel(TEXT("Max Depth:"))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(120.0f)
                    [
                        SNew(SSpinBox<int32>)
                        .MinValue(-1)
                        .MaxValue(16)
                        .Delta(1)
                        .Value_Lambda([]() -> int32
                        {
                            const auto* CVar = IConsoleManager::Get().FindConsoleVariable(
                                ck::DebugMarkers::Get_MaxDepthCVarName());
                            return CVar != nullptr ? CVar->GetInt() : 0;
                        })
                        .OnValueChanged_Lambda([](int32 InValue)
                        {
                            if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(
                                ck::DebugMarkers::Get_MaxDepthCVarName()))
                            {
                                CVar->Set(InValue, ECVF_SetByConsole);
                            }
                        })
                        .ToolTipText(FText::FromString(TEXT(
                            "Max hierarchy depth of previewed/pickable entities\n"
                            "(ck.Debug.EntityMarkers.MaxDepth — shared with the On-Screen Overlay).\n"
                            "-1 = unlimited, 0 = top-level entities only, N = up to N levels deep.\n"
                            "Not applied when this picker has a target filter — the filter is the gate.")))
                    ]
                ]
            ]

            // ---- Host-specific rows ----
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                _ExtraSettingsContent.IsValid()
                    ? _ExtraSettingsContent.ToSharedRef()
                    : SNullWidget::NullWidget
            ]
        ];
}

// =====================================================================================================================
