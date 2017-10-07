#pragma once

#include <tuple>

namespace lsmp {
    namespace _detail {
        template<int...> struct seq {};
        template<int N, int... S> struct gens : gens<N-1, N-1, S...> {};
        template<int...S> struct gens<0, S...>{ typedef seq<S...> type; };

        template<typename Callable, typename... Args, int... S>
        static void unpack_and_call(Callable&& callback, _detail::seq<S...>, std::tuple<Args...>&& args) {
            callback(std::get<S>(args)...);
        }
    } // namespace _detail


    template<typename Callable, typename... Args>
    static void unpack_and_call(Callable&& callback, std::tuple<Args...>&& args) {
        _detail::unpack_and_call(std::forward<Callable>(callback),
                                 typename _detail::gens<sizeof...(Args)>::type(),
                                 std::forward<std::tuple<Args...>>(args));
    }
} // namespace lsmp
