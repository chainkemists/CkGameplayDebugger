#pragma once

#include "CkDebuggerCommon/Graph/CkDebugNodeTheme.h"

#include "Engine/DeveloperSettings.h"

#include "CkDebuggerSettings.generated.h"

// ====================================================================================================================

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Ck Debugger"))
class CKDEBUGGERCOMMON_API UCkDebuggerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCkDebuggerSettings()
	{
		CategoryName = TEXT("Ck");
		SectionName = TEXT("Debugger");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// THEME
	// ----------------------------------------------------------------------------------------------------------------

	UPROPERTY(config, EditAnywhere, Category = "Theme",
		meta = (DisplayName = "Node Theme Style",
			ToolTip = "Visual style for debug graph nodes. Affects GOAP, SM, and future debuggers."))
	ECkDebugNodeThemeStyle NodeThemeStyle = ECkDebugNodeThemeStyle::Flat;

	// ----------------------------------------------------------------------------------------------------------------
	// ACCESSORS
	// ----------------------------------------------------------------------------------------------------------------

	static auto Get() -> const UCkDebuggerSettings*
	{
		return GetDefault<UCkDebuggerSettings>();
	}

	static auto GetTheme() -> FCkDebugNodeTheme
	{
		return FCkDebugNodeTheme::GetTheme(Get()->NodeThemeStyle);
	}
};

// ====================================================================================================================
