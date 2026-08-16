#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"
#include "CkGallery_SectionUtils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"

using ck::gallery::Caption;

namespace ck_gallery_icon_toggles
{
auto MakeAction(
    FName InId,
    FName InIconId,
    const TCHAR* InLabel,
    const TCHAR* InToolTip,
    bool InInitialState,
    bool InIsEnabled = true)
    -> FCkDebug_IconToggleAction
{
    auto State = MakeShared<bool>(InInitialState);
    return FCkDebug_IconToggleAction{
        InId,
        InIconId,
        FText::FromString(InLabel),
        FText::FromString(InToolTip),
        TAttribute<bool>::CreateLambda([State]() { return *State; }),
        FOnCkDebug_IconToggleChanged::CreateLambda([State](bool InState) { *State = InState; }),
        TAttribute<bool>{InIsEnabled}};
}

auto MakeToolbarActions() -> TArray<FCkDebug_IconToggleAction>
{
    return {
        MakeAction(TEXT("Overlay"), TEXT("World"), TEXT("Overlay"), TEXT("Show the world overlay"), true),
        MakeAction(TEXT("Grid"), TEXT("Grid"), TEXT("Grid"), TEXT("Show grid lines"), true),
        MakeAction(TEXT("Candidates"), TEXT("People"), TEXT("Candidates"), TEXT("Show candidate markers"), true),
        MakeAction(TEXT("Best"), TEXT("Target"), TEXT("Best result"), TEXT("Highlight the best result"), true),
        MakeAction(TEXT("Failed"), TEXT("Severity_Error"), TEXT("Failed"), TEXT("Show failed results"), false),
        MakeAction(TEXT("Querier"), TEXT("Person"), TEXT("Querier"), TEXT("Show the querier marker"), true),
        MakeAction(TEXT("Line"), TEXT("Rail"), TEXT("Best line"), TEXT("Draw a line to the best result"), true),
        MakeAction(TEXT("Bounds"), TEXT("Cube"), TEXT("Bounds"), TEXT("Show result bounds"), false),
    };
}
} // namespace ck_gallery_icon_toggles

// ====================================================================================================================

class FCkGallery_IconToggles : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Icon Toggles")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("Canonical boolean action and always-visible direct toolbar."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 5; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        using namespace ck_gallery_icon_toggles;

        const auto OnState = MakeAction(
            TEXT("OnExample"), TEXT("Grid"), TEXT("Grid"), TEXT("Checked state"), true);
        const auto OffState = MakeAction(
            TEXT("OffExample"), TEXT("World"), TEXT("Overlay"), TEXT("Unchecked state"), false);
        const auto DisabledState = MakeAction(
            TEXT("DisabledExample"), TEXT("Target"), TEXT("Best result"), TEXT("Disabled state"), true, false);

        return SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                Caption(TEXT("Individual states — checked · unchecked · disabled"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_IconToggle)
                    .IconId(OnState.IconId)
                    .Label(OnState.Label)
                    .ToolTip(OnState.ToolTip)
                    .IsOn(OnState.IsOn)
                    .OnStateChanged(OnState.OnStateChanged)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_IconToggle)
                    .IconId(OffState.IconId)
                    .Label(OffState.Label)
                    .ToolTip(OffState.ToolTip)
                    .IsOn(OffState.IsOn)
                    .OnStateChanged(OffState.OnStateChanged)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SCkDebug_IconToggle)
                    .IconId(DisabledState.IconId)
                    .Label(DisabledState.Label)
                    .ToolTip(DisabledState.ToolTip)
                    .IsOn(DisabledState.IsOn)
                    .OnStateChanged(DisabledState.OnStateChanged)
                    .IsEnabled(DisabledState.IsEnabled)
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                Caption(TEXT("Toolbar — all eight actions remain directly visible"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
            [
                SNew(SBox)
                .WidthOverride(620.0f)
                [
                    SNew(SScrollBox)
                    .Orientation(Orient_Horizontal)
                    .ScrollBarVisibility(EVisibility::Collapsed)
                    .ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)

                    + SScrollBox::Slot()
                    [
                        SNew(SCkDebug_IconToolbar)
                        .Actions(MakeToolbarActions())
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                Caption(TEXT("Narrow container — direct actions stay on one scrollable line"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .WidthOverride(280.0f)
                [
                    SNew(SScrollBox)
                    .Orientation(Orient_Horizontal)
                    .ScrollBarVisibility(EVisibility::Collapsed)
                    .ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)

                    + SScrollBox::Slot()
                    [
                        SNew(SCkDebug_IconToolbar)
                        .Actions(MakeToolbarActions())
                    ]
                ]
            ];
    }
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_IconToggles)

// ====================================================================================================================
