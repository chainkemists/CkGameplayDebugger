#include "CkSlateDebuggerWindow.h"
#include "CkSlateDebuggerStyle.h"
#include "Widgets/CkDebuggerSidebar.h"
#include "Widgets/CkDebuggerToolbar.h"
#include "Views/CkDebuggerViewBase.h"
#include "Views/CkDebuggerEntitySelectionView.h"
#include "Views/CkDebuggerEntityDetailsView.h"
#include "Views/CkDebuggerAbilitiesView.h"
#include "Views/CkDebuggerAttributesView.h"
#include "Views/CkDebuggerTimersView.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "SCkSlateDebuggerWindow"

void SCkSlateDebuggerWindow::Construct(const FArguments& InArgs)
{
    CreateViews();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkSlateDebuggerStyle::Get().GetBrush("CkDebugger.Background"))
        .Padding(0)
        [
            SNew(SVerticalBox)

            // World selector bar
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(8, 4)
            [
                BuildWorldSelector()
            ]

            // Main content
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)

                // Sidebar
                + SSplitter::Slot()
                .Value(0.2f)
                [
                    SNew(SBox)
                    .WidthOverride(220.0f)
                    [
                        SAssignNew(Sidebar, SCkDebuggerSidebar)
                        .OnViewSelected_Lambda([this](const FName& ViewName)
                        {
                            SetActiveView(ViewName);
                        })
                    ]
                ]

                // Content area
                + SSplitter::Slot()
                .Value(0.8f)
                [
                    SNew(SVerticalBox)

                    // Toolbar
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(Toolbar, SCkDebuggerToolbar)
                        .DebuggerWindow(SharedThis(this))
                    ]

                    // View container
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .Padding(4)
                    [
                        SAssignNew(ViewContainer, SBorder)
                        .BorderImage(FCkSlateDebuggerStyle::Get().GetBrush("CkDebugger.Panel"))
                        .Padding(8)
                    ]
                ]
            ]

            // Status bar
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FCkSlateDebuggerStyle::Get().GetBrush("CkDebugger.Panel"))
                .Padding(FMargin(8, 2))
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4, 0)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]()
                        {
                            return FText::Format(LOCTEXT("EntitiesCount", "Entities: {0}"),
                                FText::AsNumber(SelectedEntities.Num()));
                        })
                        .TextStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Small"))
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4, 0)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]()
                        {
                            if (ck::IsValid(SelectedWorld))
                            {
                                return FText::FromString(TEXT("Connected"));
                            }
                            return FText::FromString(TEXT("Disconnected"));
                        })
                        .ColorAndOpacity_Lambda([this]()
                        {
                            return ck::IsValid(SelectedWorld) ? FLinearColor::Green : FLinearColor::Red;
                        })
                        .TextStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Small"))
                    ]
                ]
            ]
        ]
    ];

    // Set initial world
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (ck::IsValid(Context.World()))
            {
                SetSelectedWorld(Context.World());
                break;
            }
        }
    }

    // Set initial view
    SetActiveView(TEXT("EntitySelection"));
}

void SCkSlateDebuggerWindow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (InCurrentTime - LastUpdateTime > UpdateInterval)
    {
        LastUpdateTime = InCurrentTime;

        if (ActiveView.IsValid())
        {
            ActiveView->UpdateView();
        }
    }
}

auto SCkSlateDebuggerWindow::GetSelectedWorld() const -> UWorld*
{
    return SelectedWorld.Get();
}

auto SCkSlateDebuggerWindow::SetSelectedWorld(UWorld* InWorld) -> void
{
    if (SelectedWorld == InWorld)
    { return; }

    SelectedWorld = InWorld;
    OnWorldChanged();
}

auto SCkSlateDebuggerWindow::GetSelectedEntities() const -> const TArray<FCk_Handle>&
{
    return SelectedEntities;
}

auto SCkSlateDebuggerWindow::SetSelectedEntities(const TArray<FCk_Handle>& InEntities) -> void
{
    PreviousSelectedEntities = SelectedEntities;
    SelectedEntities = InEntities;
    OnSelectionChanged();
}

auto SCkSlateDebuggerWindow::AddSelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (SelectedEntities.Contains(InEntity))
    { return; }

    PreviousSelectedEntities = SelectedEntities;
    SelectedEntities.Add(InEntity);
    OnSelectionChanged();
}

auto SCkSlateDebuggerWindow::RemoveSelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (NOT SelectedEntities.Contains(InEntity))
    { return; }

    PreviousSelectedEntities = SelectedEntities;
    SelectedEntities.Remove(InEntity);
    OnSelectionChanged();
}

auto SCkSlateDebuggerWindow::ClearSelectedEntities() -> void
{
    if (SelectedEntities.IsEmpty())
    { return; }

    PreviousSelectedEntities = SelectedEntities;
    SelectedEntities.Empty();
    OnSelectionChanged();
}

auto SCkSlateDebuggerWindow::SetActiveView(const FName& ViewName) -> void
{
    if (ActiveViewName == ViewName)
    { return; }

    auto* View = Views.Find(ViewName);
    if (NOT View)
    { return; }

    ActiveViewName = ViewName;
    ActiveView = *View;

    if (ViewContainer.IsValid())
    {
        ViewContainer->SetContent(ActiveView.ToSharedRef());
    }

    if (Sidebar.IsValid())
    {
        Sidebar->SetActiveView(ViewName);
    }
}

auto SCkSlateDebuggerWindow::GetActiveView() const -> TSharedPtr<SCkDebuggerViewBase>
{
    return ActiveView;
}

auto SCkSlateDebuggerWindow::RefreshCurrentView() -> void
{
    if (ActiveView.IsValid())
    {
        ActiveView->RefreshView();
    }
}

auto SCkSlateDebuggerWindow::RefreshWorldOptions() -> void
{
    WorldOptions.Empty();

    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (ck::IsValid(Context.World()))
            {
                WorldOptions.Add(Context.World());
            }
        }
    }

    if (WorldComboBox.IsValid())
    {
        WorldComboBox->RefreshOptions();
    }
}

auto SCkSlateDebuggerWindow::CreateViews() -> void
{
    RegisterView(TEXT("EntitySelection"), SNew(SCkDebuggerEntitySelectionView).DebuggerWindow(SharedThis(this)));
    //RegisterView(TEXT("EntityDetails"), SNew(SCkDebuggerEntityDetailsView).DebuggerWindow(SharedThis(this)));
    RegisterView(TEXT("Abilities"), SNew(SCkDebuggerAbilitiesView).DebuggerWindow(SharedThis(this)));
    //RegisterView(TEXT("Attributes"), SNew(SCkDebuggerAttributesView).DebuggerWindow(SharedThis(this)));
    //RegisterView(TEXT("Timers"), SNew(SCkDebuggerTimersView).DebuggerWindow(SharedThis(this)));
}

auto SCkSlateDebuggerWindow::RegisterView(const FName& ViewName, TSharedRef<SCkDebuggerViewBase> View) -> void
{
    Views.Add(ViewName, View);
}

auto SCkSlateDebuggerWindow::OnWorldChanged() -> void
{
    ClearSelectedEntities();

    for (const auto& [Name, View] : Views)
    {
        View->OnWorldChanged(SelectedWorld.Get());
    }

    RefreshCurrentView();
}

auto SCkSlateDebuggerWindow::OnSelectionChanged() -> void
{
    for (const auto& [Name, View] : Views)
    {
        View->OnSelectionChanged(SelectedEntities);
    }
}

auto SCkSlateDebuggerWindow::BuildWorldSelector() -> TSharedRef<SWidget>
{
    // Populate world options initially
    RefreshWorldOptions();

    return SNew(SHorizontalBox)

    + SHorizontalBox::Slot()
    .AutoWidth()
    .VAlign(VAlign_Center)
    .Padding(4, 0)
    [
        SNew(STextBlock)
        .Text(LOCTEXT("WorldLabel", "World:"))
        .TextStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold"))
    ]

    + SHorizontalBox::Slot()
    .AutoWidth()
    .Padding(4, 0)
    [
        SAssignNew(WorldComboBox, SComboBox<TWeakObjectPtr<UWorld>>)
        .OptionsSource(&WorldOptions)
        .OnGenerateWidget_Lambda([](TWeakObjectPtr<UWorld> World) -> TSharedRef<SWidget>
        {
            if (auto* ValidWorld = World.Get())
            {
                FString NetModeString;
                switch (ValidWorld->GetNetMode())
                {
                    case NM_Standalone:
                        NetModeString = TEXT("Standalone");
                        break;
                    case NM_DedicatedServer:
                        NetModeString = TEXT("Dedicated Server");
                        break;
                    case NM_ListenServer:
                        NetModeString = TEXT("Listen Server");
                        break;
                    case NM_Client:
                        NetModeString = TEXT("Client");
                        break;
                    default:
                        NetModeString = TEXT("Unknown");
                        break;
                }

                return SNew(STextBlock)
                    .Text(FText::Format(LOCTEXT("WorldFormat", "{0} ({1})"),
                        FText::FromString(ValidWorld->GetName()),
                        FText::FromString(NetModeString)));
            }
            return SNullWidget::NullWidget;
        })
        .OnSelectionChanged_Lambda([this](TWeakObjectPtr<UWorld> World, ESelectInfo::Type)
        {
            SetSelectedWorld(World.Get());
        })
        .Content()
        [
            SNew(STextBlock)
            .Text(this, &SCkSlateDebuggerWindow::GetWorldSelectorText)
        ]
    ]

    // Add refresh button
    + SHorizontalBox::Slot()
    .AutoWidth()
    .Padding(4, 0)
    [
        SNew(SButton)
        .ToolTipText(LOCTEXT("RefreshWorldsTip", "Refresh available worlds"))
        .ButtonStyle(FCoreStyle::Get(), "FlatButton")
        .ContentPadding(FMargin(2))
        .OnClicked_Lambda([this]() -> FReply
        {
            RefreshWorldOptions();
            return FReply::Handled();
        })
        [
            SNew(SImage)
            .Image(FCoreStyle::Get().GetBrush("Icons.Refresh"))
        ]
    ];
}

auto SCkSlateDebuggerWindow::GetWorldSelectorText() const -> FText
{
    if (auto* World = SelectedWorld.Get())
    {
        FString NetModeString;
        switch (World->GetNetMode())
        {
            case NM_Standalone:
                NetModeString = TEXT("Standalone");
                break;
            case NM_DedicatedServer:
                NetModeString = TEXT("Dedicated Server");
                break;
            case NM_ListenServer:
                NetModeString = TEXT("Listen Server");
                break;
            case NM_Client:
                NetModeString = TEXT("Client");
                break;
            default:
                NetModeString = TEXT("Unknown");
                break;
        }

        return FText::Format(LOCTEXT("WorldFormat", "{0} ({1})"),
            FText::FromString(World->GetName()),
            FText::FromString(NetModeString));
    }
    return LOCTEXT("NoWorld", "No World Selected");
}

#undef LOCTEXT_NAMESPACE