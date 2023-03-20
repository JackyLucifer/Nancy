#pragma once
#include <type_traits>
#include <functional>

namespace nc::details {

template <typename F, typename... Args>
using is_runnable = std::is_constructible<std::function<void(Args...)>, typename std::remove_reference<F>::type>;

} // namespace nc::net
