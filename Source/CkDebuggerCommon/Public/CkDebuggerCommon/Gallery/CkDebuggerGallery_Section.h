#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

// ====================================================================================================================
// One showcase inside the widget gallery. Implementations are registered
// via CK_REGISTER_DEBUGGER_GALLERY_SECTION; the gallery window queries the
// registry at construct time and builds one SCkDebug_InspectorPanel per
// section, invoking Build_Widget() to populate the body.
//
// Keep sections small and focused. A section should demonstrate EVERY
// variant / state / interactive capability of exactly one widget (or one
// family of related widgets). Add examples liberally — the whole point of
// the gallery is to show edge cases next to the main-line case.
// ====================================================================================================================

class ICkDebuggerGallery_Section
{
public:
	virtual ~ICkDebuggerGallery_Section() = default;

	/** Section title shown in the left rail + panel header. */
	virtual auto Get_Name() const -> FText = 0;

	/** Short caption under the title explaining what the section demonstrates. Optional. */
	virtual auto Get_Description() const -> FText { return FText::GetEmpty(); }

	/** Sort order — lower values sort higher in the left rail. */
	virtual auto Get_SortPriority() const -> int32 { return 100; }

	/** Build the body widget. Called once when the gallery is constructed. */
	virtual auto Build_Widget() -> TSharedRef<SWidget> = 0;
};

// ====================================================================================================================
