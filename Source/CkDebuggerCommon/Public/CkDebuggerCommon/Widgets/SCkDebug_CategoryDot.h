#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Small colored circle used as a category swatch throughout the debug UIs.
// No state, no data binding — give it a color and a diameter.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_CategoryDot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_CategoryDot)
		: _Color(FLinearColor::White)
		, _Diameter(8.0f)
	{}
		SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_ARGUMENT(float, Diameter)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
