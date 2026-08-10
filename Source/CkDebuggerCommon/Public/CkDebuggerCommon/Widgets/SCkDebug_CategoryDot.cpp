#include "SCkDebug_CategoryDot.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

// ====================================================================================================================

auto
	SCkDebug_CategoryDot::
	Construct(const FArguments& InArgs)
	-> void
{
	const auto Color = InArgs._Color;
	const auto Size = InArgs._Diameter;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(Size)
		.HeightOverride(Size)
		[
			// Pill brush, not GenericWhiteBox: the rounded-box shader clamps its corner radius to
			// half the smaller side, so a square box with the pill radius paints a true circle at
			// any diameter. GenericWhiteBox painted these as squares.
			SNew(SImage)
			.Image(CkStyle::GetRoundedBrush_Pill())
			.ColorAndOpacity(FSlateColor(Color))
		]
	];
}

// ====================================================================================================================
