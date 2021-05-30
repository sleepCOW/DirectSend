#pragma once

#include <utility>
#include <array>
#include <exception>

/**
 * Compile time map with the std::map like functionality
 */
template<typename Key, typename Value, std::size_t Size>
struct ConstexprMap
{
	std::array<std::pair<Key, Value>, Size> Data;

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