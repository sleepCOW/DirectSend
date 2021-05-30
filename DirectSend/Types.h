#pragma once

#include <iostream>
#include <array>
#include <list>
#include <string>
#include <string_view>

using SizeT = std::size_t;

template<typename Type, SizeT Size>
using Array = std::array<Type, Size>;

template<typename Key, typename Value>
using Pair = std::pair<Key, Value>;

template<typename Type>
using List = std::list<Type>;

using String = std::string;
using StringView = std::string_view;

using OStream = std::ostream;