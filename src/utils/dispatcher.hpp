#include <any>
#include <functional>
#include <tuple>
#include <vector>
#include <unordered_map>

#include "unpack_and_call.hpp"

namespace lsmp {
    template<typename InputT, typename TypeCodeT>
    struct no_converter {
        using type_code_type      = TypeCodeT;
        using input_type          = InputT;

        template<TypeCodeT TypeCode>
        struct handler {
            using args_tuple    = std::tuple<InputT>;
            using type          = std::function<void(args_tuple)>;
        };

        template<TypeCodeT TypeCode>
        static auto convert(const input_type& input) {
            return std::make_tuple(input);
        }
    };

    /**
     * (C++17) Universal object dispatcher.
     *
     * \tparam TypeCodeT    Type for identifier of input type.
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
     */
    template<typename TypeCodeT,
             typename InputT,
             typename ConverterT = no_converter<InputT, TypeCodeT>>
    class dispatcher {
    public:
        template<TypeCodeT TypeCode>
        void add_handler(typename ConverterT::template handler<TypeCode>::type handler) {
            handlers[TypeCode].push_back(handler);
        }

        template<TypeCodeT TypeCode>
        void dispatch(const typename ConverterT::input_type& input) const {
            auto handlers_it = handlers.find(TypeCode);
            if (handlers_it == handlers.end()) return;

            for (const auto& any_handler : handlers_it->second) {
                using handler_type = typename ConverterT::template handler<TypeCode>::type;

                unpack_and_call(std::any_cast<handler_type>(any_handler), ConverterT::template convert<TypeCode>(input));
            }
        }
    private:
        std::unordered_map<TypeCodeT, std::vector<std::any>> handlers;
    };
} // namespace lsmp

/*
Test case:

using namespace lsmp;

struct string_to_integral_converter {
    using type_code_type   = int;
    using input_type    = std::string;

    template<type_code_type TypeCode>
    struct handler {
        static_assert(TypeCode == sizeof(short) || TypeCode == sizeof(int) || TypeCode == sizeof(long),
                      "string_to_integral converter supports only signed short, int and long.");
    };

    template<type_code_type TypeCode>
    static auto convert(const input_type&) {
        static_assert(TypeCode == sizeof(short) || TypeCode == sizeof(int) || TypeCode == sizeof(long),
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
