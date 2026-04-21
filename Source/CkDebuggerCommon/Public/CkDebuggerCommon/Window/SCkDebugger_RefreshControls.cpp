#include "SCkDebugger_RefreshControls.h"

#include "CkDebuggerRefreshGate.h"

#include "CkDebuggerCommon/Settings/CkDebuggerWindowSettings.h"
#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace
{
	auto ModeLabel(ECkDebugger_RefreshMode InMode) -> FText
	{
		switch (InMode)
		{
			case ECkDebugger_RefreshMode::AlwaysRefresh:   return FText::FromString(TEXT("Always Refresh"));
			case ECkDebugger_RefreshMode::OnlyWhenVisible: return FText::FromString(TEXT("Only When Visible"));
			case ECkDebugger_RefreshMode::UseGlobal:
			default:                                       return FText::FromString(TEXT("Use Global"));
		}
	}

	auto RateLabel(ECkDebugger_RefreshRateCap InRate) -> FText
	{
		switch (InRate)
		{
			case ECkDebugger_RefreshRateCap::Unlimited: return FText::FromString(TEXT("Unlimited"));
			case ECkDebugger_RefreshRateCap::Hz60:      return FText::FromString(TEXT("60 Hz"));
			case ECkDebugger_RefreshRateCap::Hz30:      return FText::FromString(TEXT("30 Hz"));
			case ECkDebugger_RefreshRateCap::Hz15:      return FText::FromString(TEXT("15 Hz"));
			case ECkDebugger_RefreshRateCap::Hz5:       return FText::FromString(TEXT("5 Hz"));
			case ECkDebugger_RefreshRateCap::UseGlobal:
			default:                                    return FText::FromString(TEXT("Use Global"));
		}
	}

	auto SaveSettings() -> void
	{
		if (auto* Settings = GetMutableDefault<UCkDebuggerWindowSettings>())
		{
			Settings->SaveConfig();
		}
	}

	auto Get_CurrentMode(FName InWindowId) -> ECkDebugger_RefreshMode
	{
		const auto* Settings = GetDefault<UCkDebuggerWindowSettings>();
		if (Settings == nullptr) { return ECkDebugger_RefreshMode::UseGlobal; }
		return Settings->RefreshMode_PerWindow.Contains(InWindowId)
			? Settings->RefreshMode_PerWindow[InWindowId]
			: ECkDebugger_RefreshMode::UseGlobal;
	}

	auto Get_CurrentRate(FName InWindowId) -> ECkDebugger_RefreshRateCap
	{
		const auto* Settings = GetDefault<UCkDebuggerWindowSettings>();
		if (Settings == nullptr) { return ECkDebugger_RefreshRateCap::UseGlobal; }
		return Settings->RefreshRateCap_PerWindow.Contains(InWindowId)
			? Settings->RefreshRateCap_PerWindow[InWindowId]
			: ECkDebugger_RefreshRateCap::UseGlobal;
	}

	auto Set_CurrentMode(FName InWindowId, ECkDebugger_RefreshMode InMode) -> void
	{
		auto* Settings = GetMutableDefault<UCkDebuggerWindowSettings>();
		if (Settings == nullptr) { return; }

		if (InMode == ECkDebugger_RefreshMode::UseGlobal)
		{
			Settings->RefreshMode_PerWindow.Remove(InWindowId);
		}
		else
		{
			Settings->RefreshMode_PerWindow.Add(InWindowId, InMode);
		}
		SaveSettings();
	}

	auto Set_CurrentRate(FName InWindowId, ECkDebugger_RefreshRateCap InRate) -> void
	{
		auto* Settings = GetMutableDefault<UCkDebuggerWindowSettings>();
		if (Settings == nullptr) { return; }

		if (InRate == ECkDebugger_RefreshRateCap::UseGlobal)
		{
			Settings->RefreshRateCap_PerWindow.Remove(InWindowId);
		}
		else
		{
			Settings->RefreshRateCap_PerWindow.Add(InWindowId, InRate);
		}
		SaveSettings();
	}
}

// ====================================================================================================================

auto
	SCkDebugger_RefreshControls::
	Construct(const FArguments& InArgs)
	-> void
{
	_WindowId = InArgs._WindowId;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, CkDebugStyle::SpaceS, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Refresh:")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, CkDebugStyle::SpaceS, 0.0f)
		[
			Build_ModeDropdown()
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			Build_RateDropdown()
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebugger_RefreshControls::
	Build_ModeDropdown()
	-> TSharedRef<SWidget>
{
	const auto WindowId = _WindowId;

	const auto MenuContentBuilder = FOnGetContent::CreateLambda([WindowId]()
	{
		auto Menu = FMenuBuilder(true, nullptr);

		const auto Choices = TArray<ECkDebugger_RefreshMode>{
			ECkDebugger_RefreshMode::UseGlobal,
			ECkDebugger_RefreshMode::AlwaysRefresh,
			ECkDebugger_RefreshMode::OnlyWhenVisible,
		};

		for (auto Choice : Choices)
		{
			auto Label = ModeLabel(Choice);
			if (Choice == ECkDebugger_RefreshMode::UseGlobal)
			{
				const auto Resolved = FCkDebuggerRefreshGate::Get_EffectiveMode(WindowId);
				// Only show the "resolved" hint when UseGlobal isn't already shadowing itself.
				if (Resolved != ECkDebugger_RefreshMode::UseGlobal)
				{
					Label = FText::FromString(FString::Printf(
						TEXT("Use Global  (currently: %s)"),
						*ModeLabel(Resolved).ToString()));
				}
			}

			Menu.AddMenuEntry(
				Label,
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([WindowId, Choice]()
					{
						Set_CurrentMode(WindowId, Choice);
					}),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([WindowId, Choice]()
					{
						return Get_CurrentMode(WindowId) == Choice;
					})
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		return Menu.MakeWidget();
	});

	return SNew(SComboButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(CkDebugStyle::SpaceS, 2.0f))
		.OnGetMenuContent(MenuContentBuilder)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([WindowId]() { return ModeLabel(Get_CurrentMode(WindowId)); })
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebugger_RefreshControls::
	Build_RateDropdown()
	-> TSharedRef<SWidget>
{
	const auto WindowId = _WindowId;

	const auto MenuContentBuilder = FOnGetContent::CreateLambda([WindowId]()
	{
		auto Menu = FMenuBuilder(true, nullptr);

		const auto Choices = TArray<ECkDebugger_RefreshRateCap>{
			ECkDebugger_RefreshRateCap::UseGlobal,
			ECkDebugger_RefreshRateCap::Unlimited,
			ECkDebugger_RefreshRateCap::Hz60,
			ECkDebugger_RefreshRateCap::Hz30,
			ECkDebugger_RefreshRateCap::Hz15,
			ECkDebugger_RefreshRateCap::Hz5,
		};

		for (auto Choice : Choices)
		{
			auto Label = RateLabel(Choice);
			if (Choice == ECkDebugger_RefreshRateCap::UseGlobal)
			{
				const auto Resolved = FCkDebuggerRefreshGate::Get_EffectiveRateCap(WindowId);
				if (Resolved != ECkDebugger_RefreshRateCap::UseGlobal)
				{
					Label = FText::FromString(FString::Printf(
						TEXT("Use Global  (currently: %s)"),
						*RateLabel(Resolved).ToString()));
				}
			}

			Menu.AddMenuEntry(
				Label,
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([WindowId, Choice]()
					{
						Set_CurrentRate(WindowId, Choice);
					}),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([WindowId, Choice]()
					{
						return Get_CurrentRate(WindowId) == Choice;
					})
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		return Menu.MakeWidget();
	});

	return SNew(SComboButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(CkDebugStyle::SpaceS, 2.0f))
		.OnGetMenuContent(MenuContentBuilder)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([WindowId]() { return RateLabel(Get_CurrentRate(WindowId)); })
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
		];
}

// ====================================================================================================================
