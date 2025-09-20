#include "CkDebuggerSidebar.h"

#include "CkSlateDebugger/CkSlateDebuggerStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCkDebuggerSidebar"

void SCkDebuggerSidebar::Construct(const FArguments& InArgs)
{
    OnViewSelected = InArgs._OnViewSelected;

    BuildNavigation();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkSlateDebuggerStyle::Get().GetBrush("CkDebugger.Sidebar"))
        .Padding(0)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SAssignNew(NavigationBox, SVerticalBox)
            ]
        ]
    ];

    // Build categories
    TMap<FName, TArray<FCkDebuggerNavItem>> CategorizedItems;
    for (const auto& Item : NavigationItems)
    {
        CategorizedItems.FindOrAdd(Item.Category).Add(Item);
    }

    // Core category
    if (auto* CoreItems = CategorizedItems.Find("Core"))
    {
        NavigationBox->AddSlot()
        .AutoHeight()
        [
            CreateNavCategory(LOCTEXT("Core", "Core"), *CoreItems)
        ];
    }

    // Gameplay category
    if (auto* GameplayItems = CategorizedItems.Find("Gameplay"))
    {
        NavigationBox->AddSlot()
        .AutoHeight()
        [
            CreateNavCategory(LOCTEXT("Gameplay", "Gameplay"), *GameplayItems)
        ];
    }

    // Physics category
    if (auto* PhysicsItems = CategorizedItems.Find("Physics"))
    {
        NavigationBox->AddSlot()
        .AutoHeight()
        [
            CreateNavCategory(LOCTEXT("Physics", "Physics"), *PhysicsItems)
        ];
    }

    // Interaction category
    if (auto* InteractionItems = CategorizedItems.Find("Interaction"))
    {
        NavigationBox->AddSlot()
        .AutoHeight()
        [
            CreateNavCategory(LOCTEXT("Interaction", "Interaction"), *InteractionItems)
        ];
    }

    // Debug category
    if (auto* DebugItems = CategorizedItems.Find("Debug"))
    {
        NavigationBox->AddSlot()
        .AutoHeight()
        [
            CreateNavCategory(LOCTEXT("Debug", "Debug"), *DebugItems)
        ];
    }
}

auto SCkDebuggerSidebar::SetActiveView(const FName& ViewName) -> void
{
    ActiveViewName = ViewName;
}

auto SCkDebuggerSidebar::BuildNavigation() -> void
{
    // Core
    NavigationItems.Add({TEXT("EntitySelection"), LOCTEXT("EntitySelection", "Entity Selection"), TEXT("Core")});
    NavigationItems.Add({TEXT("EntityDetails"), LOCTEXT("EntityDetails", "Entity Details"), TEXT("Core")});
    NavigationItems.Add({TEXT("EntityCollections"), LOCTEXT("EntityCollections", "Collections"), TEXT("Core")});

    // Gameplay
    NavigationItems.Add({TEXT("Abilities"), LOCTEXT("Abilities", "Abilities"), TEXT("Gameplay")});
    NavigationItems.Add({TEXT("Attributes"), LOCTEXT("Attributes", "Attributes"), TEXT("Gameplay")});
    NavigationItems.Add({TEXT("Objectives"), LOCTEXT("Objectives", "Objectives"), TEXT("Gameplay")});
    NavigationItems.Add({TEXT("Timers"), LOCTEXT("Timers", "Timers"), TEXT("Gameplay")});

    // Physics
    NavigationItems.Add({TEXT("Overlaps"), LOCTEXT("Overlaps", "Overlaps"), TEXT("Physics")});
    NavigationItems.Add({TEXT("Probes"), LOCTEXT("Probes", "Probes"), TEXT("Physics")});

    // Interaction
    NavigationItems.Add({TEXT("InteractTargets"), LOCTEXT("InteractTargets", "Interact Targets"), TEXT("Interaction")});
    NavigationItems.Add({TEXT("InteractionResolvers"), LOCTEXT("InteractionResolvers", "Resolvers"), TEXT("Interaction")});

    // Debug
    NavigationItems.Add({TEXT("Performance"), LOCTEXT("Performance", "Performance"), TEXT("Debug")});
    NavigationItems.Add({TEXT("Console"), LOCTEXT("Console", "Console"), TEXT("Debug")});
}

auto SCkDebuggerSidebar::CreateNavCategory(const FText& CategoryName, const TArray<FCkDebuggerNavItem>& Items) -> TSharedRef<SWidget>
{
    TSharedRef<SVerticalBox> CategoryBox = SNew(SVerticalBox);

    // Category header
    CategoryBox->AddSlot()
    .AutoHeight()
    .Padding(12, 8, 12, 4)
    [
        SNew(STextBlock)
        .Text(CategoryName)
        .TextStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.CategoryHeader"))
    ];

    // Items
    for (const auto& Item : Items)
    {
        CategoryBox->AddSlot()
        .AutoHeight()
        [
            CreateNavItem(Item)
        ];
    }

    return CategoryBox;
}

auto SCkDebuggerSidebar::CreateNavItem(const FCkDebuggerNavItem& Item) -> TSharedRef<SWidget>
{
    return SNew(SButton)
    .ButtonStyle(FCoreStyle::Get(), "NoBorder")
    .ForegroundColor(FSlateColor::UseStyle())
    .OnClicked_Lambda([this, Item]()
    {
        if (OnViewSelected.IsBound())
        {
            OnViewSelected.Execute(Item.Name);
        }
        return FReply::Handled();
    })
    [
        SNew(SBorder)
        .BorderImage_Lambda([this, Item]()
        {
            if (IsViewActive(Item.Name))
            {
                return FCkSlateDebuggerStyle::Get().GetBrush("CkDebugger.Panel");
            }
            return FCoreStyle::Get().GetBrush("NoBorder");
        })
        .Padding(FMargin(20, 8))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0, 0, 8, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("•")))
                .ColorAndOpacity_Lambda([this, Item]()
                {
                    return IsViewActive(Item.Name) ?
                        FCkSlateDebuggerStyle::Get().GetColor("CkDebugger.Color.Entity") :
                        FLinearColor(0.5f, 0.5f, 0.5f);
                })
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(Item.DisplayName)
                .ColorAndOpacity_Lambda([this, Item]()
                {
                    return IsViewActive(Item.Name) ?
                        FLinearColor::White :
                        FLinearColor(0.8f, 0.8f, 0.8f);
                })
            ]
        ]
    ];
}

auto SCkDebuggerSidebar::IsViewActive(const FName& ViewName) const -> bool
{
    return ActiveViewName == ViewName;
}

#undef LOCTEXT_NAMESPACE