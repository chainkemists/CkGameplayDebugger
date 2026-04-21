#pragma once

#include "CkDebuggerGallery_Section.h"

// ====================================================================================================================
// Auto-registration registry for gallery sections. Each `.cpp` writes
// `CK_REGISTER_DEBUGGER_GALLERY_SECTION(FMySection)` at file scope; a static
// initializer adds a factory to the global list. The window pulls the list,
// sorts by priority, and instantiates each section once.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API FCkDebuggerGallery_Registry
{
public:
	using FFactory = TFunction<TSharedRef<ICkDebuggerGallery_Section>()>;

	static auto Get() -> FCkDebuggerGallery_Registry&;

	auto Register(FFactory InFactory) -> void;

	/** Instantiate every registered section, sorted by Get_SortPriority(). */
	auto CreateAll() const -> TArray<TSharedRef<ICkDebuggerGallery_Section>>;

private:
	TArray<FFactory> _Factories;
};

// --------------------------------------------------------------------------------------------------------------------

#define CK_REGISTER_DEBUGGER_GALLERY_SECTION(SectionType) \
	namespace \
	{ \
		struct FAutoReg_##SectionType \
		{ \
			FAutoReg_##SectionType() \
			{ \
				FCkDebuggerGallery_Registry::Get().Register([]() -> TSharedRef<ICkDebuggerGallery_Section> \
				{ \
					return MakeShared<SectionType>(); \
				}); \
			} \
		}; \
		static FAutoReg_##SectionType GAutoReg_##SectionType; \
	}

// ====================================================================================================================
