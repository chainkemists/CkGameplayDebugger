#include "SGraphNode_SmEntry.h"

#include "CkSmDebugNode_Entry.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"
#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmEntry::
    Construct(
        const FArguments& InArgs,
        UCkSmDebugNode_Entry* InNode)
    -> void
{
    _EntryNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_SmEntry::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    auto TextColor = FCkSmDebuggerStyle::Color_Sm_EntryText;

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        [
            SNew(SOverlay)

            // Pin overlay — fills entire entry widget for connection geometry
            + SOverlay::Slot()
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Fill)
                [
                    SAssignNew(RightNodeBox, SVerticalBox)
                ]

            // Content — "Entry ▶"
            + SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Entry")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                .ColorAndOpacity(TextColor)
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("\x25B6")))  // ▶
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                                .ColorAndOpacity(TextColor)
                        ]
                ]
        ];

    CreatePinWidgets();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmEntry::
    CreatePinWidgets()
    -> void
{
    if (NOT GraphNode) { return; }

    for (auto* Pin : GraphNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && NOT Pin->bHidden)
        {
            auto PinWidget = SNew(SGraphPin, Pin);
            PinWidget->SetIsEditable(false);
            PinWidget->SetVisibility(EVisibility::HitTestInvisible);
            AddPin(PinWidget);
            break;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmEntry::
    AddPin(
        const TSharedRef<SGraphPin>& PinToAdd)
    -> void
{
    PinToAdd->SetOwner(SharedThis(this));

    RightNodeBox->AddSlot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .FillHeight(1.0f)
        [
            PinToAdd
        ];

    OutputPins.Add(PinToAdd);
}

// --------------------------------------------------------------------------------------------------------------------
