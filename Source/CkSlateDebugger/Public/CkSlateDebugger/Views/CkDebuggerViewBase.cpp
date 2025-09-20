#include "CkDebuggerViewBase.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkSlateDebugger/CkSlateDebuggerWindow.h"

void SCkDebuggerViewBase::Construct(const FArguments& InArgs)
{
    DebuggerWindow = InArgs._DebuggerWindow;
}

auto SCkDebuggerViewBase::OnWorldChanged(UWorld* NewWorld) -> void
{
    CachedWorld = NewWorld;
}

auto SCkDebuggerViewBase::OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void
{
    CachedSelection = NewSelection;
}

auto SCkDebuggerViewBase::GetSelectedWorld() const -> UWorld*
{
    if (auto Window = DebuggerWindow.Pin())
    {
        return Window->GetSelectedWorld();
    }
    return CachedWorld.Get();
}

auto SCkDebuggerViewBase::GetSelectedEntities() const -> TArray<FCk_Handle>
{
    if (auto Window = DebuggerWindow.Pin())
    {
        return Window->GetSelectedEntities();
    }
    return CachedSelection;
}

auto SCkDebuggerViewBase::GetPrimarySelectedEntity() const -> FCk_Handle
{
    auto Entities = GetSelectedEntities();
    return Entities.Num() > 0 ? Entities[0] : FCk_Handle{};
}

auto SCkDebuggerViewBase::GetDebuggerWindow() const -> TSharedPtr<SCkSlateDebuggerWindow>
{
    return DebuggerWindow.Pin();
}