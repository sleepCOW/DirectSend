#pragma once

#include <iostream>
#include <array>
#include <list>
#include <string>
#include <string_view>
#include <thread>
#include <mutex>
#include <chrono>

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

using TimePoint = std::chrono::steady_clock::time_point;

using Mutex = std::mutex;

template <class Mutex>
using LockGuard = std::lock_guard<Mutex>;

using Thread = std::thread;

using namespace std::literals;