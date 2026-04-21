#pragma once

#include "CkDebuggerGallery_Section.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

class SScrollBox;
class SVerticalBox;

// ====================================================================================================================
// Left rail of section names + scrollable body of showcase panels. Every
// registered ICkDebuggerGallery_Section is instantiated once and wrapped in
// an SCkDebug_InspectorPanel.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebuggerGallery_Window : public SCkDebugger_WindowBase
{
public:
	static const FName WindowId;

	SLATE_BEGIN_ARGS(SCkDebuggerGallery_Window) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	virtual auto Get_WindowId() const -> FName override { return WindowId; }
	virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Widget Gallery")); }

private:
	auto Build_LeftRail() -> TSharedRef<SWidget>;
	auto Build_Body() -> TSharedRef<SWidget>;
	auto Build_TopBar() -> TSharedRef<SWidget>;
	auto OnRailItemClicked(int32 InIndex) -> FReply;

	TArray<TSharedRef<ICkDebuggerGallery_Section>> _Sections;
	TArray<TSharedPtr<class SWidget>> _SectionPanels;
	TSharedPtr<SScrollBox> _ScrollBox;
};

// ====================================================================================================================
