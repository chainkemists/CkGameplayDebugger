#include "CkGoapDebugger/Window/SCkGoapDebugger_InspectorGateway.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// ====================================================================================================================

namespace ck_goap_debugger_gateway_internal
{
    // Mirrors the standalone window's status-color lookup so the gateway dots
    // read the same way at-a-glance.
    static auto
    ColorForPlanStatus(
        ECk_GoapPlanStatus InStatus,
        ECk_EnableDisable  InEnable) -> FLinearColor
    {
        if (InEnable == ECk_EnableDisable::Disable)
        { return FCkGoapDebuggerStyle::Color_Text_Faint; }

        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:               return FCkGoapDebuggerStyle::Color_Status_PlanFound;
        case ECk_GoapPlanStatus::Planning:                return FCkGoapDebuggerStyle::Color_Status_Planning;
        case ECk_GoapPlanStatus::PlanFailed:              return FCkGoapDebuggerStyle::Color_Status_Failed;
        case ECk_GoapPlanStatus::CostThresholdReached:    return FCkGoapDebuggerStyle::Color_Status_Selected;
        case ECk_GoapPlanStatus::Idle:
        default:                                          return FCkGoapDebuggerStyle::Color_Text_Dim;
        }
    }

    static auto
    LabelForPlanStatus(
        ECk_GoapPlanStatus InStatus) -> FString
    {
        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:               return TEXT("PlanFound");
        case ECk_GoapPlanStatus::Planning:                return TEXT("Planning");
        case ECk_GoapPlanStatus::PlanFailed:              return TEXT("PlanFailed");
        case ECk_GoapPlanStatus::CostThresholdReached:    return TEXT("CostThreshold");
        case ECk_GoapPlanStatus::Idle:                    return TEXT("Idle");
        default:                                          return TEXT("(unknown)");
        }
    }

    // Compute the "headline" plan status for a Planner — used by the dot in
    // the Planner list. Picks the deepest active chain entry's status when
    // available; otherwise falls back to the root's status.
    static auto
    HeadlineStatus(
        const FCkGoapDebugger_ActionSetInfo& InAs) -> ECk_GoapPlanStatus
    {
        if (InAs.ActiveChainHandles.Num() > 0)
        {
            const auto& Leaf = InAs.ActiveChainHandles.Last();
            const auto* Info = InAs.Catalog.FindByPredicate(
                [&Leaf](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == Leaf; });
            if (Info != nullptr) { return Info->PlanStatus; }
        }

        if (ck::IsValid(InAs.RootActionHandle))
        {
            const auto* Root = InAs.Catalog.FindByPredicate(
                [&InAs](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == InAs.RootActionHandle; });
            if (Root != nullptr) { return Root->PlanStatus; }
        }

        return ECk_GoapPlanStatus::Idle;
    }

    static auto
    LeafActionInfo(
        const FCkGoapDebugger_ActionSetInfo& InAs) -> const FCkGoapDebugger_ActionInfo*
    {
        if (InAs.ActiveChainHandles.Num() == 0) { return nullptr; }
        const auto& LeafHandle = InAs.ActiveChainHandles.Last();
        return InAs.Catalog.FindByPredicate(
            [&LeafHandle](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == LeafHandle; });
    }

    // Small colored dot used in the Planner list rows + the leaf-status indicator.
    static auto
    MakeStatusDot(
        const FLinearColor& InColor,
        float InSize = 7.0f) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(InSize)
            .HeightOverride(InSize)
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(InColor)
                    .Padding(FMargin(0.0f))
                    [ SNew(SSpacer) ]
            ];
    }

    // Small rounded badge used for the "root" pill in the header.
    static auto
    MakeBadge(
        const FString& InText,
        const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(InColor.CopyWithNewOpacity(0.18f))
            .Padding(FMargin(6.0f, 1.0f))
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(InText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FSlateColor(InColor))
            ];
    }

    // Hash the bits of the snapshot the gateway actually renders. Keeps Tick
    // from re-emitting Slate every frame when nothing visible changed.
    static auto
    HashGatewayState(
        const FCkGoapDebugger_EntitySnapshot* InSnapshot) -> uint32
    {
        if (InSnapshot == nullptr) { return 0; }

        auto Hash = uint32{0};
        Hash = HashCombine(Hash, ::GetTypeHash(InSnapshot->EntityHandle));
        Hash = HashCombine(Hash, ::GetTypeHash(InSnapshot->ActionSets.Num()));

        for (const auto& As : InSnapshot->ActionSets)
        {
            Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(As.Handle)));
            Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(As.EnableToggle)));
            Hash = HashCombine(Hash, ::GetTypeHash(As.ActiveChainHandles.Num()));

            for (const auto& Action : As.Catalog)
            {
                Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(Action.PlanStatus)));
                Hash = HashCombine(Hash, ::GetTypeHash(Action.PlanClassNames.Num()));
            }
        }

        return Hash;
    }
} // namespace ck_goap_debugger_gateway_internal

// ====================================================================================================================
// CONSTRUCT
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Entity = InArgs._Entity;

    ChildSlot
    [
        SAssignNew(_ContentBox, SVerticalBox)
    ];

    Rebuild();
}

// ====================================================================================================================
// SELECTION
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Set_Entity(
        const FCk_Handle& InEntity)
    -> void
{
    if (_Entity == InEntity) { return; }
    _Entity        = InEntity;
    _HasBuilt      = false;
    _LastBuiltHash = 0;
    Rebuild();
}

// ====================================================================================================================
// TICK — re-pull snapshots and rebuild if the gateway-visible state changed.
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    using namespace ck_goap_debugger_gateway_internal;

    // Honour the per-window refresh gate. The gateway lives inside the
    // EcsDebugger entity inspector, so it consults that window's gate id —
    // matching SCkDebuggerWindow_Main::WindowId. Without this the collect below
    // runs every editor tick even when the user has set "OnlyWhenVisible" or a
    // Hz cap on the EcsDebugger.
    static const auto EcsDebuggerWindowId = FName(TEXT("EcsDebugger"));
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(EcsDebuggerWindowId))
    { return; }

    if (ck::Is_NOT_Valid(_Entity)) { return; }

    auto* World = Resolve_World();
    if (World == nullptr) { return; }

    // Deep tier for THIS entity only. This used to pull the all-agents batch
    // and discard everything but its own slice — the dominant cost of having
    // the gateway open in a populated world.
    const auto MySnapshotOpt = FCkGoapDebugger_DataCollector::CollectFull(World, _Entity);
    const auto* MySnapshot = MySnapshotOpt.GetPtrOrNull();

    const auto NewHash = HashGatewayState(MySnapshot);
    if (_HasBuilt && NewHash == _LastBuiltHash) { return; }

    _LastBuiltHash = NewHash;
    _HasBuilt      = true;
    Rebuild();
}

// ====================================================================================================================
// REBUILD
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Rebuild()
    -> void
{
    if (NOT _ContentBox.IsValid()) { return; }
    _ContentBox->ClearChildren();

    if (ck::Is_NOT_Valid(_Entity))
    {
        _ContentBox->AddSlot().AutoHeight() [ Build_EmptyStub() ];
        return;
    }

    auto* World = Resolve_World();
    if (World == nullptr)
    {
        _ContentBox->AddSlot().AutoHeight() [ Build_EmptyStub() ];
        return;
    }

    const auto MySnapshotOpt = FCkGoapDebugger_DataCollector::CollectFull(World, _Entity);
    const auto* MySnapshot = MySnapshotOpt.GetPtrOrNull();

    if (MySnapshot == nullptr)
    {
        // Selected entity has no Goap root — render a collapsed stub so the
        // inspector slot has known height.
        _ContentBox->AddSlot().AutoHeight() [ Build_EmptyStub() ];
        return;
    }

    _ContentBox->AddSlot()
        .AutoHeight()
        [ Build_Header(*MySnapshot) ];

    _ContentBox->AddSlot()
        .AutoHeight()
        [ Build_ActionSetList(*MySnapshot) ];

    if (const auto* DisplayAs = Pick_DisplayActionSet(*MySnapshot))
    {
        _ContentBox->AddSlot()
            .AutoHeight()
            [ Build_ActiveChain(*DisplayAs) ];

        using namespace ck_goap_debugger_gateway_internal;
        if (const auto* Leaf = LeafActionInfo(*DisplayAs))
        {
            _ContentBox->AddSlot()
                .AutoHeight()
                [ Build_LeafAction(*Leaf) ];

            _ContentBox->AddSlot()
                .AutoHeight()
                [ Build_PlanPreview(*Leaf) ];
        }
    }
}

// ====================================================================================================================
// SECTIONS
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Build_Header(
        const FCkGoapDebugger_EntitySnapshot& InSnapshot)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_gateway_internal;

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Surface)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("GOAP")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        MakeBadge(TEXT("root"), FCkGoapDebuggerStyle::Color_Status_PlanFound)
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Open in Goap Debugger >")))
                            .ToolTipText(FText::FromString(TEXT("Open the standalone Goap Debugger window focused on this entity")))
                            .OnClicked(this, &SCkGoapDebugger_InspectorGateway::OnClicked_OpenInGoapDebugger)
                    ]
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_ActionSetList(
        const FCkGoapDebugger_EntitySnapshot& InSnapshot)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_gateway_internal;

    auto Box = SNew(SVerticalBox);

    // Section header
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(FString::Printf(TEXT("PLANNERS - %d"), InSnapshot.ActionSets.Num())))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
        ];

    for (const auto& As : InSnapshot.ActionSets)
    {
        const auto Status   = HeadlineStatus(As);
        const auto DotColor = ColorForPlanStatus(Status, As.EnableToggle);

        auto TagStr = As.ActionSetTag.IsValid()
            ? As.ActionSetTag.ToString()
            : FString{};

        auto StatusStr = As.EnableToggle == ECk_EnableDisable::Disable
            ? FString(TEXT("Disabled"))
            : FString::Printf(TEXT("%s - %d active"),
                  *LabelForPlanStatus(Status),
                  As.ActiveChainHandles.Num());

        const auto NameColor = As.EnableToggle == ECk_EnableDisable::Disable
            ? FCkGoapDebuggerStyle::Color_Text_Dim
            : FCkGoapDebuggerStyle::Color_Text_Primary;

        Box->AddSlot()
            .AutoHeight()
            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
            [
                SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                        [
                            MakeStatusDot(DotColor)
                        ]

                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                                    [
                                        SNew(SCkDebug_SelectableLabel)
                                            .Text(FText::FromString(As.DebugName))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                            .ColorAndOpacity(FSlateColor(NameColor))
                                    ]
                                + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SCkDebug_SelectableLabel)
                                            .Text(FText::FromString(TagStr))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                                    ]
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(StatusStr))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                        ]
            ];
    }

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Panel)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            Box
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_ActiveChain(
        const FCkGoapDebugger_ActionSetInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    auto HeaderText = FString::Printf(TEXT("ACTIVE CHAIN - %s"), *InPlanner.DebugName);

    // Compose the breadcrumb. Leaf gets highlighted in amber per the mockup.
    auto BreadcrumbBox = SNew(SHorizontalBox);
    auto TagChain      = FString{};

    const auto NumEntries = InPlanner.ActiveChainHandles.Num();
    for (auto i = 0; i < NumEntries; ++i)
    {
        const auto& Handle = InPlanner.ActiveChainHandles[i];
        const auto* Info = InPlanner.Catalog.FindByPredicate(
            [&Handle](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == Handle; });

        auto ClassName = (Info != nullptr) ? Info->ClassName : FString(TEXT("(?)"));
        const bool IsLeaf = (i == NumEntries - 1);

        if (i > 0)
        {
            BreadcrumbBox->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT(">")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Ghost))
                ];
        }

        const auto Color = IsLeaf
            ? FCkGoapDebuggerStyle::Color_Status_Selected
            : FCkGoapDebuggerStyle::Color_Text_Secondary;

        BreadcrumbBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(ClassName))
                    .Font(IsLeaf
                        ? FCoreStyle::GetDefaultFontStyle("Bold", 10)
                        : FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .ColorAndOpacity(FSlateColor(Color))
            ];

        if (Info != nullptr && Info->ActionTag.IsValid())
        {
            if (NOT TagChain.IsEmpty()) { TagChain.Append(TEXT(" > ")); }
            TagChain.Append(Info->ActionTag.ToString());
        }
    }

    if (NumEntries == 0)
    {
        BreadcrumbBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(TEXT("(no active chain)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
            ];
    }

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Panel)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(HeaderText))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Root)
                            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
                            [
                                BreadcrumbBox
                            ]
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(FString::Printf(TEXT("Action tag chain: %s"),
                                TagChain.IsEmpty() ? TEXT("(none)") : *TagChain)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                    ]
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_LeafAction(
        const FCkGoapDebugger_ActionInfo& InLeaf)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_gateway_internal;

    auto MakeRow = [](const FString& InLabel, const FString& InValue, const FLinearColor& InValueColor) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(InLabel))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(InValue))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        .ColorAndOpacity(FSlateColor(InValueColor))
                ];
    };

    const auto StatusColor = ColorForPlanStatus(InLeaf.PlanStatus, ECk_EnableDisable::Enable);
    const auto CostStr     = FString::Printf(TEXT("%.0f"), InLeaf.PlanCost);
    const auto LenStr      = FString::Printf(TEXT("%d action%s"),
        InLeaf.PlanClassNames.Num(),
        InLeaf.PlanClassNames.Num() == 1 ? TEXT("") : TEXT("s"));

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Panel)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(FString::Printf(TEXT("LEAF ACTION - %s"), *InLeaf.ClassName)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
                    [
                        MakeRow(TEXT("Status"), LabelForPlanStatus(InLeaf.PlanStatus), StatusColor)
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
                    [
                        MakeRow(TEXT("Cost"), CostStr, FCkGoapDebuggerStyle::Color_Status_Selected)
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
                    [
                        MakeRow(TEXT("Plan length"), LenStr, FCkGoapDebuggerStyle::Color_Text_Primary)
                    ]
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_PlanPreview(
        const FCkGoapDebugger_ActionInfo& InLeaf)
    -> TSharedRef<SWidget>
{
    const auto Total   = InLeaf.PlanClassNames.Num();
    const auto Preview = FMath::Min(3, Total);

    auto HeaderText = (Total <= Preview)
        ? FString::Printf(TEXT("PLAN (%d)"), Total)
        : FString::Printf(TEXT("PLAN (first %d of %d)"), Preview, Total);

    auto List = SNew(SVerticalBox);

    for (auto i = 0; i < Preview; ++i)
    {
        const auto& Name = InLeaf.PlanClassNames[i];

        List->AddSlot()
            .AutoHeight()
            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(FString::Printf(TEXT("%d."), i + 1)))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_PlanningBdr))
                        ]
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(Name))
                                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                        ]
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            // We don't have per-plan-entry cost in PlanClassNames; surface a
                            // single "(leaf)" hint since plan entries are always leaves of
                            // the active chain.
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(TEXT("(leaf)")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                        ]
            ];
    }

    if (Total == 0)
    {
        List->AddSlot()
            .AutoHeight()
            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(TEXT("(no plan)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
            ];
    }

    if (Total > Preview)
    {
        List->AddSlot()
            .AutoHeight()
            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(FString::Printf(TEXT("+ %d more - open the Window to see all"),
                        Total - Preview)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
    }

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Panel)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(HeaderText))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        List
                    ]
        ];
}

auto
    SCkGoapDebugger_InspectorGateway::
    Build_EmptyStub()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Panel)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT("(no Goap root on selected entity)")))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
        ];
}

// ====================================================================================================================
// HELPERS
// ====================================================================================================================

auto
    SCkGoapDebugger_InspectorGateway::
    Resolve_World() const
    -> UWorld*
{
    auto FoundWorld = static_cast<UWorld*>(nullptr);

    if (GEngine == nullptr) { return nullptr; }

    for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
    {
        if (It->WorldType == EWorldType::PIE && ck::IsValid(It->World()) && It->World()->HasBegunPlay())
        {
            FoundWorld = It->World();
            break;
        }
    }

#if WITH_EDITOR
    if (FoundWorld == nullptr && GEditor)
    {
        auto& EditorCtx = GEditor->GetEditorWorldContext();
        if (ck::IsValid(EditorCtx.World()))
        { FoundWorld = EditorCtx.World(); }
    }
#endif

    return FoundWorld;
}

auto
    SCkGoapDebugger_InspectorGateway::
    Pick_DisplayActionSet(
        const FCkGoapDebugger_EntitySnapshot& InSnapshot) const
    -> const FCkGoapDebugger_ActionSetInfo*
{
    using namespace ck_goap_debugger_gateway_internal;

    if (InSnapshot.ActionSets.Num() == 0) { return nullptr; }

    // Priority: enabled + PlanFound > enabled + Planning > enabled + has chain > first.
    const FCkGoapDebugger_ActionSetInfo* Best = nullptr;
    auto BestRank = -1;

    for (const auto& As : InSnapshot.ActionSets)
    {
        if (As.EnableToggle == ECk_EnableDisable::Disable) { continue; }

        const auto Status = HeadlineStatus(As);
        auto Rank = 0;
        if (Status == ECk_GoapPlanStatus::PlanFound) { Rank = 3; }
        else if (Status == ECk_GoapPlanStatus::Planning) { Rank = 2; }
        else if (As.ActiveChainHandles.Num() > 0) { Rank = 1; }

        if (Rank > BestRank)
        {
            Best     = &As;
            BestRank = Rank;
        }
    }

    if (Best != nullptr) { return Best; }
    return &InSnapshot.ActionSets[0];
}

auto
    SCkGoapDebugger_InspectorGateway::
    OnClicked_OpenInGoapDebugger()
    -> FReply
{
    SCkGoapDebuggerWindow::OpenForEntity(_Entity);
    return FReply::Handled();
}

// ====================================================================================================================
