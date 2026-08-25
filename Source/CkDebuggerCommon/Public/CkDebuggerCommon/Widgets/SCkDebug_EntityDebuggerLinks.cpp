#include "SCkDebug_EntityDebuggerLinks.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_EntityDebuggerLinks::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _ExcludeTabId = InArgs._ExcludeTabId;

    _RouteChangedHandle = FCkDebug_EntityTargetRegistry::Get().Get_OnChanged().AddSP(
        SharedThis(this), &SCkDebug_EntityDebuggerLinks::Rebuild);
    _ToolChangedHandle = FCkDebuggerToolRegistry::Get().Get_OnChanged().AddSP(
        SharedThis(this), &SCkDebug_EntityDebuggerLinks::Rebuild);

    ChildSlot
    [
        SAssignNew(_Root, SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor(CkStyle::Bg2())
            .Padding(FMargin{CkStyle::SpaceS})
            [
                SAssignNew(_Links, SHorizontalBox)
            ]
    ];

    Rebuild();
}

SCkDebug_EntityDebuggerLinks::~SCkDebug_EntityDebuggerLinks()
{
    if (_RouteChangedHandle.IsValid())
    { FCkDebug_EntityTargetRegistry::Get().Get_OnChanged().Remove(_RouteChangedHandle); }

    if (_ToolChangedHandle.IsValid())
    { FCkDebuggerToolRegistry::Get().Get_OnChanged().Remove(_ToolChangedHandle); }
}

auto SCkDebug_EntityDebuggerLinks::Tick(
    const FGeometry& InAllottedGeometry,
    double           InCurrentTime,
    float            InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (_Entity.Get() == _BuiltForEntity)
    { return; }

    Rebuild();
}

auto SCkDebug_EntityDebuggerLinks::Rebuild() -> void
{
    if (NOT _Links.IsValid())
    { return; }

    _Links->ClearChildren();

    const auto Entity = _Entity.Get();
    _BuiltForEntity = Entity;
    auto TargetableTabs = TSet<FName>{};
    for (const auto TabId : FCkDebug_EntityTargetRegistry::Get().Get_TargetableTabs(Entity))
    { TargetableTabs.Add(TabId); }
    if (TargetableTabs.IsEmpty())
    {
        if (_Root.IsValid()) { _Root->SetVisibility(EVisibility::Collapsed); }
        return;
    }

    auto AddedCount = 0;
    for (const auto& Tool : FCkDebuggerToolRegistry::Get().Get_Tools())
    {
        if (Tool.Get_TabId() == _ExcludeTabId || NOT TargetableTabs.Contains(Tool.Get_TabId()))
        { continue; }

        if (AddedCount == 0)
        {
            _Links->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceS, 0.0f})
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("OPEN IN")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeMicro()))
                        .ColorAndOpacity(CkStyle::TextMute())
                ];
        }

        _Links->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceXS, 0.0f})
            [
                SNew(SButton)
                    .Text(Tool.Get_DisplayName())
                    .ToolTipText(FText::Format(
                        FText::FromString(TEXT("Open {0} and target this entity")),
                        Tool.Get_DisplayName()))
                    .OnClicked(this, &SCkDebug_EntityDebuggerLinks::OnOpenInClicked, Tool.Get_TabId())
            ];

        ++AddedCount;
    }

    if (_Root.IsValid())
    { _Root->SetVisibility(AddedCount > 0 ? EVisibility::Visible : EVisibility::Collapsed); }
}

auto SCkDebug_EntityDebuggerLinks::OnOpenInClicked(FName InTabId) -> FReply
{
    FCkDebug_EntityTargetRegistry::Get().TryOpenAndTarget(InTabId, _Entity.Get());
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------
