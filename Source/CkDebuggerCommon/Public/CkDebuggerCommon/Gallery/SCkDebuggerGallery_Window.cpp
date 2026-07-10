#include "SCkDebuggerGallery_Window.h"

#include "CkDebuggerGallery_Registry.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

const FName SCkDebuggerGallery_Window::WindowId = FName(TEXT("DebuggerGallery"));

auto
	SCkDebuggerGallery_Window::
	Construct(const FArguments& InArgs)
	-> void
{
	Register_WithGate();

	_Sections = FCkDebuggerGallery_Registry::Get().CreateAll();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkStyle::BgRoot()))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				Build_TopBar()
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				+ SSplitter::Slot()
				.Value(0.2f)
				[
					Build_LeftRail()
				]

				+ SSplitter::Slot()
				.Value(0.8f)
				[
					Build_Body()
				]
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebuggerGallery_Window::
	Build_TopBar()
	-> TSharedRef<SWidget>
{
	return SNew(SBorder)
		.BorderImage(CkStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
		.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceS))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("CK Debugger \u2014 Widget Gallery")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeBody()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCkDebugger_RefreshControls)
					.WindowId(SCkDebuggerGallery_Window::WindowId)
			]
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebuggerGallery_Window::
	Build_LeftRail()
	-> TSharedRef<SWidget>
{
	auto List = SNew(SVerticalBox);

	List->AddSlot()
		.AutoHeight()
		.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("WIDGETS")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::PaneHeadingFontSize()))
			.ColorAndOpacity(FSlateColor(CkStyle::PaneHeadingColor()))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		];

	for (auto Index = 0; Index < _Sections.Num(); ++Index)
	{
		const auto& Section = _Sections[Index];
		const auto CapturedIndex = Index;

		List->AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ContentPadding(FMargin(CkStyle::SpaceL, CkStyle::SpaceS))
				.OnClicked_Lambda([this, CapturedIndex]() { return OnRailItemClicked(CapturedIndex); })
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text(Section->Get_Name())
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeBody()))
					.ColorAndOpacity(FSlateColor(CkStyle::Text()))
				]
			];
	}

	return SNew(SBorder)
		.BorderImage(CkStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
		.Padding(0.0f)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Vertical)

			+ SScrollBox::Slot()
			[
				List
			]
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebuggerGallery_Window::
	Build_Body()
	-> TSharedRef<SWidget>
{
	SAssignNew(_ScrollBox, SScrollBox)
		.Orientation(Orient_Vertical)
		.ScrollBarAlwaysVisible(true);

	_SectionPanels.Empty();
	_SectionPanels.Reserve(_Sections.Num());

	for (const auto& Section : _Sections)
	{
		// Optional description caption between title and body.
		auto Body = Section->Build_Widget();
		const auto Description = Section->Get_Description();

		auto Wrapped = TSharedPtr<SWidget>{};
		if (Description.IsEmpty())
		{
			Wrapped = Body;
		}
		else
		{
			Wrapped = SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
				[
					SNew(STextBlock)
					.Text(Description)
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkStyle::FontSizeSmall()))
					.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					Body
				];
		}

		const TSharedRef<SWidget> Panel = SNew(SCkDebug_InspectorPanel)
			.Title(Section->Get_Name())
			.Body()
			[
				SNew(SBox)
				.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
				[
					Wrapped.ToSharedRef()
				]
			];

		_SectionPanels.Add(Panel);

		_ScrollBox->AddSlot()
			.Padding(FMargin(CkStyle::SpaceM))
			[
				Panel
			];
	}

	return _ScrollBox.ToSharedRef();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebuggerGallery_Window::
	OnRailItemClicked(int32 InIndex)
	-> FReply
{
	if (_SectionPanels.IsValidIndex(InIndex) && _SectionPanels[InIndex].IsValid() && _ScrollBox.IsValid())
	{
		_ScrollBox->ScrollDescendantIntoView(_SectionPanels[InIndex].ToSharedRef(), true, EDescendantScrollDestination::TopOrLeft);
	}
	return FReply::Handled();
}

// ====================================================================================================================
