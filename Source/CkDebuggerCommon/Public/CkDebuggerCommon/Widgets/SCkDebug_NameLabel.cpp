#include "SCkDebug_NameLabel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
    SCkDebug_NameLabel::
    Get_ShortName(
        const FString& InFullName,
        int32 InDepth)
    -> FString
{
    auto Name = InFullName;
    if (Name.StartsWith(TEXT("Default__"))) { Name = Name.RightChop(9); }
    if (Name.EndsWith(TEXT("_C"))) { Name = Name.LeftChop(2); }

    TArray<FString> Segments;
    static const TCHAR* Delims[] = { TEXT("_"), TEXT(".") };
    Name.ParseIntoArray(Segments, Delims, 2, true);

    if (InDepth <= 0 || InDepth >= Segments.Num())
    { return FString::Join(Segments, TEXT(".")); }

    auto Result = FString{};
    for (auto i = Segments.Num() - InDepth; i < Segments.Num(); ++i)
    {
        if (Result.Len() > 0) { Result += TEXT("."); }
        Result += Segments[i];
    }
    return Result;
}

auto
    SCkDebug_NameLabel::
    Get_SegmentCount(
        const FString& InFullName)
    -> int32
{
    auto Name = InFullName;
    if (Name.StartsWith(TEXT("Default__"))) { Name = Name.RightChop(9); }
    if (Name.EndsWith(TEXT("_C"))) { Name = Name.LeftChop(2); }

    TArray<FString> Segments;
    static const TCHAR* Delims[] = { TEXT("_"), TEXT(".") };
    Name.ParseIntoArray(Segments, Delims, 2, true);
    return FMath::Max(1, Segments.Num());
}

// ====================================================================================================================

auto
    SCkDebug_NameLabel::
    Construct(const FArguments& InArgs)
    -> void
{
    _FullName  = InArgs._FullName;
    _Prefix    = InArgs._Prefix;
    _NameDepth = InArgs._NameDepth;

    const auto Font = InArgs._Font.IsSet()
        ? InArgs._Font
        : TAttribute<FSlateFontInfo>(CkStyle::RegularFont(CkStyle::FontSizeSmall()));

    // True whenever the depth-shortened form differs from the full name — the
    // expand/contract affordance only exists when there is something to expand.
    const auto IsShortened = [this]() -> bool
    {
        return Get_ShortName(_FullName, _NameDepth.Get(1)) != _FullName;
    };

    ChildSlot
    [
        SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text_Lambda([this]() -> FText
                        {
                            const auto Name = _Expanded
                                ? _FullName
                                : Get_ShortName(_FullName, _NameDepth.Get(1));
                            return FText::FromString(_Prefix + Name);
                        })
                        .ToolTipText(FText::FromString(_FullName))
                        .Font(Font)
                        .ColorAndOpacity(InArgs._ColorAndOpacity)
                ]

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin{CkStyle::SpaceS, 0.0f, 0.0f, 0.0f})
                [
                    SNew(SButton)
                        .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                        .ContentPadding(FMargin{2.0f, 0.0f})
                        .Visibility_Lambda([this, IsShortened]() -> EVisibility
                        {
                            return IsShortened() ? EVisibility::Visible : EVisibility::Collapsed;
                        })
                        .ToolTipText_Lambda([this]() -> FText
                        {
                            return FText::FromString(_Expanded
                                ? TEXT("Contract to the short name")
                                : TEXT("Expand to the full name"));
                        })
                        .OnClicked_Lambda([this]() -> FReply
                        {
                            _Expanded = NOT _Expanded;
                            return FReply::Handled();
                        })
                        [
                            SNew(STextBlock)
                                .Text_Lambda([this]() -> FText
                                {
                                    return FText::FromString(_Expanded ? TEXT("\x00AB") : TEXT("\x00BB"));   // « / »
                                })
                                .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                        ]
                ]
    ];
}

// ====================================================================================================================
