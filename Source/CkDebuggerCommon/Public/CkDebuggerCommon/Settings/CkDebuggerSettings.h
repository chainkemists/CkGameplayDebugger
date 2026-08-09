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
	// DISPLAY NAMES
	// ----------------------------------------------------------------------------------------------------------------

	UPROPERTY(config, EditAnywhere, Category = "Display Names",
		meta = (DisplayName = "Entity Name Strip Patterns",
			ToolTip = "Substrings removed from entity display names on every debugger surface (entity pills, ECS tree rows, agent lists, overlay plates). Applied BEFORE the built-in Default__/_C/_0 cleanup, so full prefixes like 'Default__Ck_EntityScript_GoapGym_' work as written."))
	TArray<FString> EntityNameStripPatterns;

	// ----------------------------------------------------------------------------------------------------------------
	// HIERARCHY
	// ----------------------------------------------------------------------------------------------------------------

	UPROPERTY(config, EditAnywhere, Category = "Hierarchy",
		meta = (DisplayName = "Actor Relay Depth Transparency",
			ToolTip = "Treat entities owned by an ActorRelay as if the relay were not there. The ECS entity tree hoists relay children up to top level (revealable per-relay), and the on-screen overlay's depth counting skips relay hops, so relay-owned entities stay focusable and markered at the default ck.Debug.EntityMarkers.MaxDepth. Turn off to see literal lifetime nesting."))
	bool bActorRelayDepthTransparency = true;

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
