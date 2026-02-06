#include "ArgusFragmentTypeMap.h"

namespace Argus
{

auto FFragmentTypeMap::Get() -> FFragmentTypeMap&
{
    static FFragmentTypeMap Instance;
    return Instance;
}

auto FFragmentTypeMap::GetAllEntries() const -> const TMap<entt::id_type, const UScriptStruct*>&
{
    return Entries;
}

auto FFragmentTypeMap::Num() const -> int32
{
    return Entries.Num();
}

} // namespace Argus
