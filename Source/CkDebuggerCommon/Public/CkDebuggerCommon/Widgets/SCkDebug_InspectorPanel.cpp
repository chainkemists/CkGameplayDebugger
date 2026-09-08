#include "SCkDebug_InspectorPanel.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"

#include "CkSlateLayout/CkFlexLayoutTypes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

namespace ck_debug_inspector_panel
{
	// The expand/collapse chevron. 12 is the size this header has always drawn — four less than the
	// IconSize axis' 16 default — so it rides the axis as a DELTA and stays 12 under Classic.
	constexpr auto ChevronSizeBase = 12.0f;

	auto Get_ChevronSize() -> TOptional<FVector2D>
	{
		const auto Size = ck::debug_axes::Apply_IconSize(ChevronSizeBase);
		return TOptional<FVector2D>{FVector2D{Size, Size}};
	}
}

// ====================================================================================================================

auto
	SCkDebug_InspectorPanel::
	Construct(const FArguments& InArgs)
	-> void
{
	_IsExpanded = InArgs._StartExpanded;
	_OnToggled = InArgs._OnToggled;
	_Title = InArgs._Title;
	_CountText = InArgs._CountText;
	_StatusPillText = InArgs._StatusPillText;
	_StatusPillTone = InArgs._StatusPillTone;
	_IconBrush = InArgs._IconBrush;
	_IconColor = InArgs._IconColor;
	_Body = InArgs._Body.Widget;

	SAssignNew(_HeaderRow, SHorizontalBox);

	SAssignNew(_HeaderButton, SButton)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ContentPadding(FMargin(0.0f))
		.OnClicked(this, &SCkDebug_InspectorPanel::OnHeaderClicked)
		[
			// The header strip is a depth-1 surface; the body deliberately keeps the engine's
			// NoBorder so editor chrome shows through, which no elevation option overrides.
			SNew(SBorder)
			.BorderImage_Lambda([]{ return ck::debug_axes::Get_SurfaceBrush(1); })
			.BorderBackgroundColor_Lambda([]{ return FSlateColor{ck::debug_axes::Get_SurfaceTint(1)}; })
			.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
			[
				_HeaderRow.ToSharedRef()
			]
		];

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			_HeaderButton.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			// Body uses the NoBorder brush so the editor panel background
			// shows through — layered tints on top of editor chrome read as
			// washed-out gray.
			SAssignNew(_BodyBorder, SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
			.Padding(FMargin(0.0f))
			.Visibility(_IsExpanded ? EVisibility::Visible : EVisibility::Collapsed)
			[
				_Body.ToSharedRef()
			]
		]
	];

	RebuildHeader();
	AddMetadata(MakeShared<FCkFlexMeasureMetaData>(
		[WeakPanel = TWeakPtr<SCkDebug_InspectorPanel>(SharedThis(this))](const FCkFlexMeasureArgs& Args) -> FVector2D
		{
			const TSharedPtr<SCkDebug_InspectorPanel> Panel = WeakPanel.Pin();
			if (!Panel.IsValid() || !IsValid_CkFlexMeasureArgs(Args)) { return FVector2D::ZeroVector; }
			const FVector2D Header = Panel->_HeaderButton.IsValid() ? Panel->_HeaderButton->GetDesiredSize() : FVector2D::ZeroVector;
			FVector2D Body = FVector2D::ZeroVector;
			if (Panel->_IsExpanded && Panel->_Body.IsValid())
			{
				const TSharedPtr<FCkFlexMeasureMetaData> Measure = Panel->_Body->GetMetaData<FCkFlexMeasureMetaData>();
				auto BodyArgs = Args;
				BodyArgs.AvailableHeight = YGUndefined;
				BodyArgs.HeightMode = YGMeasureModeUndefined;
				Body = Measure.IsValid() ? Measure->Measure(BodyArgs) : Panel->_Body->GetDesiredSize();
			}
			FVector2D Result(FMath::Max<double>(Header.X, Body.X), Header.Y + Body.Y);
			if (Args.WidthMode == YGMeasureModeExactly) { Result.X = Args.AvailableWidth; }
			else if (Args.WidthMode == YGMeasureModeAtMost) { Result.X = FMath::Min<double>(Result.X, Args.AvailableWidth); }
			if (Args.HeightMode == YGMeasureModeExactly) { Result.Y = Args.AvailableHeight; }
			else if (Args.HeightMode == YGMeasureModeAtMost) { Result.Y = FMath::Min<double>(Result.Y, Args.AvailableHeight); }
			return Result;
		},
		[WeakPanel = TWeakPtr<SCkDebug_InspectorPanel>(SharedThis(this))](const float Width, const float Height)
		{
			const TSharedPtr<SCkDebug_InspectorPanel> Panel = WeakPanel.Pin();
			if (!Panel.IsValid() || !Panel->_IsExpanded || !Panel->_Body.IsValid()) { return; }
			if (const TSharedPtr<FCkFlexMeasureMetaData> Measure = Panel->_Body->GetMetaData<FCkFlexMeasureMetaData>(); Measure.IsValid())
			{
				const float HeaderHeight = Panel->_HeaderButton.IsValid() ? Panel->_HeaderButton->GetDesiredSize().Y : 0.0f;
				Measure->NotifyArranged(Width, FMath::Max(0.0f, Height - HeaderHeight));
			}
		}));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebug_InspectorPanel::
	RebuildHeader()
	-> void
{
	if (!_HeaderRow.IsValid()) { return; }
	_HeaderRow->ClearChildren();

	if (_IconBrush != nullptr)
	{
		_HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
			[
				SNew(SCkDebug_Icon)
				.Brush(_IconBrush)
				.Meaning_Lambda([WeakPanel = TWeakPtr<SCkDebug_InspectorPanel>(SharedThis(this))]()
				{
					const TSharedPtr<SCkDebug_InspectorPanel> Panel = WeakPanel.Pin();
					return Panel.IsValid() ? Panel->_Title.Get(FText::GetEmpty()) : FText::GetEmpty();
				})
				.ColorAndOpacity(FSlateColor(_IconColor))
				.Size(FVector2D(14.0f, 14.0f))
			];
	}

	_HeaderRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(_TitleText, STextBlock)
			.Text(_Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::PaneHeadingFontSize()))
			.ColorAndOpacity(FSlateColor(CkStyle::PaneHeadingColor()))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		];

	if (!_CountText.IsEmpty())
	{
		_HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(_CountBadge, STextBlock)
				.Text(_CountText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
			];
	}

	if (!_StatusPillText.IsEmpty())
	{
		_HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(_StatusPill, SCkDebug_StatusPill)
				.Text(_StatusPillText)
				.Tone(_StatusPillTone)
			];
	}

	_HeaderRow->AddSlot().FillWidth(1.0f);

	// Engine SVG chevrons \u2014 the old text glyphs (\u25BE/\u25B8) render as boxes in
	// the editor font on some systems.
	_HeaderRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(_ChevronIcon, SImage)
			.Image(FAppStyle::GetBrush(_IsExpanded ? TEXT("Icons.ChevronDown") : TEXT("Icons.ChevronRight")))
			.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
			.DesiredSizeOverride_Static(&ck_debug_inspector_panel::Get_ChevronSize)
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebug_InspectorPanel::
	Set_Expanded(bool InExpanded)
	-> void
{
	if (_IsExpanded == InExpanded) { return; }
	_IsExpanded = InExpanded;
	if (_BodyBorder.IsValid())
	{
		_BodyBorder->SetVisibility(_IsExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (_ChevronIcon.IsValid())
	{
		_ChevronIcon->SetImage(FAppStyle::GetBrush(_IsExpanded ? TEXT("Icons.ChevronDown") : TEXT("Icons.ChevronRight")));
	}
	_OnToggled.ExecuteIfBound(_IsExpanded);
}

auto
	SCkDebug_InspectorPanel::
	Set_Title(TAttribute<FText> InTitle)
	-> void
{
	_Title = MoveTemp(InTitle);
	if (_TitleText.IsValid()) { _TitleText->SetText(_Title); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebug_InspectorPanel::
	Set_CountText(const FText& InText)
	-> void
{
	_CountText = InText;
	if (_CountBadge.IsValid()) { _CountBadge->SetText(InText); }
	else { RebuildHeader(); }
}

auto
	SCkDebug_InspectorPanel::
	Set_StatusPill(const FText& InText, ECk_Tone InTone)
	-> void
{
	_StatusPillText = InText;
	_StatusPillTone = InTone;
	RebuildHeader();
}

auto
	SCkDebug_InspectorPanel::
	OnHeaderClicked()
	-> FReply
{
	Set_Expanded(!_IsExpanded);
	return FReply::Handled();
}

// ====================================================================================================================
