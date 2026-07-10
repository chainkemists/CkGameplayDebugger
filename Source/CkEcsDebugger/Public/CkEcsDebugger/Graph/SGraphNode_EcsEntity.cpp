#include "SGraphNode_EcsEntity.h"

#include "CkEcsDebugNode_Entity.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"

#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_EcsEntity::
    Construct(
        const FArguments& InArgs,
        UCkEcsDebugNode_Entity* InNode)
    -> void
{
    _EntityNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_EcsEntity::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    const auto DisplayName = _EntityNode ? _EntityNode->Get_DisplayName() : TEXT("Unknown");
    const auto bIsCenter   = _EntityNode ? _EntityNode->Get_IsCenterNode() : false;

    const auto Variant = bIsCenter
        ? ECkDebug_NodePillVariant::InPlan
        : ECkDebug_NodePillVariant::Inactive;

    // ---- Edge-type border override (static per rebuild) ----
    auto BorderOverride = FLinearColor::Transparent;
    if (_EntityNode && NOT bIsCenter)
    {
        switch (_EntityNode->Get_EdgeType())
        {
        case ECkEcsDebugEdgeType::LifetimeOwner:     BorderOverride = CkStyle::Relationship(); break;
        case ECkEcsDebugEdgeType::ContextOwner:      BorderOverride = CkStyle::Reference();    break;
        case ECkEcsDebugEdgeType::LifetimeDependent: BorderOverride = CkStyle::Transform();    break;
        default: break;
        }
    }

    // ---- Accent strip color (static per rebuild) ----
    const auto AccentColor = _EntityNode ? _EntityNode->Get_AccentColor() : FLinearColor::White;

    // ---- Outer dim wrapper: the pill's own content stays fully opaque, this
    //      multiplies by Get_DimMultiplier() per-frame so filter changes fade
    //      the whole node without a graph rebuild.
    auto DimAttr = TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateSP(
        this, &SGraphNode_EcsEntity::Get_DimTint));

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        [
            SNew(SOverlay)

            // Pin overlay — fills entire node for connection geometry.
            // HitTestInvisible so pins don't eat clicks.
            + SOverlay::Slot()
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Fill)
                [
                    SAssignNew(RightNodeBox, SVerticalBox)
                        .Visibility(EVisibility::HitTestInvisible)
                ]

            // The pill — wrapped in a transparent SBorder whose ColorAndOpacity
            // drives the per-frame dim effect for inspector filter matches.
            + SOverlay::Slot()
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Fill)
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
                    .Padding(0.0f)
                    .ColorAndOpacity(DimAttr)
                    [
                        SNew(SCkDebug_NodePill)
                        .Variant(Variant)
                        .StepIndex(-1)
                        .ShowCost(false)
                        .Title(FText::FromString(DisplayName))
                        .AccentColor(AccentColor)
                        .BorderColorOverride(BorderOverride)
                    ]
                ]
        ];

    // Mark the node volatile so Slate re-polls TAttribute callbacks every paint
    // instead of caching them — required for the inspector-filter dim path.
    constexpr auto ForceVolatileWidget = true;
    ForceVolatile(ForceVolatileWidget);

    CreatePinWidgets();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_EcsEntity::
    CreatePinWidgets()
    -> void
{
    if (NOT _EntityNode)
    { return; }

    for (auto* Pin : _EntityNode->Pins)
    {
        if (NOT Pin->bHidden)
        {
            auto NewPin = SNew(SGraphPin, Pin);
            AddPin(NewPin);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_EcsEntity::
    AddPin(
        const TSharedRef<SGraphPin>& PinToAdd)
    -> void
{
    PinToAdd->SetOwner(SharedThis(this));
    PinToAdd->SetRenderOpacity(0.0f);
    PinToAdd->SetVisibility(EVisibility::HitTestInvisible);

    if (RightNodeBox.IsValid())
    {
        RightNodeBox->AddSlot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            .FillHeight(1.0f)
            [
                PinToAdd
            ];
    }

    if (PinToAdd->GetDirection() == EGPD_Input)
    {
        InputPins.Add(PinToAdd);
    }
    else
    {
        OutputPins.Add(PinToAdd);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_EcsEntity::
    OnMouseButtonDoubleClick(
        const FGeometry& InMyGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && _EntityNode)
    {
        OnDoubleClicked.ExecuteIfBound(_EntityNode);
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_EcsEntity::
    Get_DimMultiplier() const
    -> float
{
    constexpr auto FullAlpha = 1.0f;
    constexpr auto DimAlpha  = 0.30f;

    if (NOT _EntityNode)
    { return FullAlpha; }

    return _EntityNode->Get_IsFilterMatch() ? FullAlpha : DimAlpha;
}

auto
    SGraphNode_EcsEntity::
    Get_DimTint() const
    -> FLinearColor
{
    return FLinearColor(1.0f, 1.0f, 1.0f, Get_DimMultiplier());
}

// --------------------------------------------------------------------------------------------------------------------
