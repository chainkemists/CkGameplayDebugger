#include "CkDebuggerGallery_Registry.h"

// ====================================================================================================================

auto FCkDebuggerGallery_Registry::Get() -> FCkDebuggerGallery_Registry&
{
	static FCkDebuggerGallery_Registry Instance;
	return Instance;
}

auto FCkDebuggerGallery_Registry::Register(FFactory InFactory) -> void
{
	_Factories.Add(MoveTemp(InFactory));
}

auto FCkDebuggerGallery_Registry::CreateAll() const -> TArray<TSharedRef<ICkDebuggerGallery_Section>>
{
	auto Sections = TArray<TSharedRef<ICkDebuggerGallery_Section>>{};
	Sections.Reserve(_Factories.Num());

	for (const auto& Factory : _Factories)
	{
		Sections.Add(Factory());
	}

	Sections.Sort([](const TSharedRef<ICkDebuggerGallery_Section>& A,
	                 const TSharedRef<ICkDebuggerGallery_Section>& B)
	{
		return A->Get_SortPriority() < B->Get_SortPriority();
	});

	return Sections;
}

// ====================================================================================================================
