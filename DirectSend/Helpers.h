#pragma once

#include <exception>

#include "Types.h"

/**
 * Compile time map with the std::map like functionality
 */
template<typename Key, typename Value, SizeT Size>
struct ConstexprMap
{
	Array<Pair<Key, Value>, Size> Data;

	constexpr Value At(const Key& InKey) const
	{
		const auto itr = std::find_if(Data.begin(), Data.end(), [&InKey](const auto& InValue) { return InValue.first == InKey; });

		if (itr != Data.end())
		{
			return itr->second;
		}
		else
		{
			throw std::range_error("Not found");
		}
	}
};

namespace CMD
{
	OStream& Print();
	OStream& PrintError();
}