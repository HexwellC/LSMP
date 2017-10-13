#include <any>
#include <functional>
#include <tuple>
#include <vector>
#include <unordered_map>

namespace lsmp {
    template<typename InputT, typename EventIdT>
    struct no_converter {
        using event_id_t   = EventIdT;
        using input_type   = InputT;

        template<EventIdT EventId>
        struct handler {
            using args_tuple    = std::tuple<InputT>;
            using type          = std::function<void(args_tuple)>;
        };

        template<EventIdT EventId>
        static auto convert(const input_type& input) {
            return std::make_tuple(input);
        }
    };

    /**
     * (C++17) Universal event dispatcher.
     *
     * \tparam EventIdT     Type for event ID.
     * \tparam InputT       Source input type.
     * \tparam ConvereterT  Implements conversion from InputT to arguments for handlers.
     *                      (each input type can have different handler arguments).
     *
     * In terms of LSMP this class allows you to do the following:
     * ```cpp
     * lsmp::session s(something); // Inherits dispatcher.
     *
     * s.add_handler<lsmp::op::new_message>([](const lsmp::message& msg, const std::vector<lsmp::signature>& sigs) {});
     * s.add_handler<lsmp::op::error>([](const lsmp::error_info& ei, const std::vector<lsmp::signature>& sigs) {});
     *
     * // Goes to first handler.
     * s.dispatch<lsmp::op::new_message>({ json_msg, sigs });
     *
     * // Goes to second handler.
     * s.dispatch<lsmp::op::error>({ json_err, sigs });
     * ```
     *
     * ** HOW TO **
     *
     * 1. (Optional) Define converter type.
     *    - Must define `event_id_t` and `input_type` types (same as dispatcher's `EventIdT` and `InputT`).
     *    - Must define template struct `handler<event_id_t>` with `args_tuple` and `type` types.
     *      - `args_tuple` - std::tuple with arguments to pass to handler for this event_id.
     *      - `type`       - `std::function<void(args_tuple)>` but can be used to define custom handler type.
     *    - Must define static `convert<event_id_t>` function, should accept `input_type` and return
     *      matching `handler<event_id_t>::args_tuple`.
     *
     * 2. Use `dispatcher<event_id_t, input_type, converter>`.
     *
     * 3. See "Test case" at end of dispatcher.hpp for more advanced example.
     *
     */
    template<typename EventIdT,
             typename InputT,
             typename ConverterT = no_converter<InputT, EventIdT>>
    class dispatcher {
    public:
        static_assert(std::is_same<EventIdT, typename ConverterT::event_id_t>::value, "Converter is not compatible.");
        static_assert(std::is_same<InputT, typename ConverterT::input_type>::value, "Converter is not compatible.");

        template<EventIdT EventId>
        void add_handler(typename ConverterT::template handler<EventId>::type handler) {
            handlers[EventId].push_back(handler);
        }

        template<EventIdT EventId>
        void dispatch(const typename ConverterT::input_type& input) const {
            auto handlers_it = handlers.find(EventId);
            if (handlers_it == handlers.end()) return;

            for (const auto& any_handler : handlers_it->second) {
                using handler_type = typename ConverterT::template handler<EventId>::type;

                std::apply(std::any_cast<handler_type>(any_handler), ConverterT::template convert<EventId>(input));
            }
        }
    private:
        std::unordered_map<EventIdT, std::vector<std::any>> handlers;
    };
} // namespace lsmp

/*
Test case:

using namespace lsmp;

struct string_to_integral_converter {
    using event_id_t   = int;
    using input_type    = std::string;

    template<event_id_t EventId>
    struct handler {
        static_assert(EventId == sizeof(short) || EventId == sizeof(int) || EventId == sizeof(long),
                      "string_to_integral converter supports only signed short, int and long.");
    };

    template<event_id_t EventId>
    static auto convert(const input_type&) {
        static_assert(EventId == sizeof(short) || EventId == sizeof(int) || EventId == sizeof(long),
                      "string_to_integral converter supports only signed short, int and long.");
    }
};

template<>
struct string_to_integral_converter::handler<2> {
    using args_tuple = std::tuple<short, size_t>;
    using type = std::function<void(short, size_t)>;
};

template<>
struct string_to_integral_converter::handler<4> {
    using args_tuple = std::tuple<int, size_t>;
    using type = std::function<void(int, size_t)>;
};

template<>
struct string_to_integral_converter::handler<8> {
    using args_tuple = std::tuple<long, size_t>;
    using type = std::function<void(long, size_t)>;
};

template<>
auto string_to_integral_converter::convert<2>(const std::string& input) {
    return std::make_tuple(std::stoi(input), input.size());
}

template<>
auto string_to_integral_converter::convert<4>(const std::string& input) {
    return std::make_tuple(std::stoi(input), input.size());
}

template<>
auto string_to_integral_converter::convert<8>(const std::string& input) {
    return std::make_tuple(std::stol(input), input.size());
}

int main() {
    dispatcher<int, std::string, string_to_integral_converter> d;

    d.add_handler<2>([](short s, size_t) {
        std::cout << "Received short: " << s << '\n';
    });

    d.add_handler<8>([](long l, size_t) {
        std::cout << "Received long: " << l << '\n';
    });

    d.dispatch<2>("1000"); // sent to first callback, converted to short+string size by converter
    d.dispatch<8>("20230403003000"); // sent to second callback, converted to long+string size by converter
}
*/
