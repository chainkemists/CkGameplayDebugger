#include "SCkDebug_CategoryDot.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

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
			SNew(SImage)
			.Image(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
			.ColorAndOpacity(FSlateColor(Color))
		]
	];
}

// ====================================================================================================================
