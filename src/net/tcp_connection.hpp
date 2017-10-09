#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <stdexcept>
#include <variant>
#include <functional>
#include <mutex>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>

namespace lsmp {
    struct corrupted_packet_error : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /**
     * Message-oriented TCP stream wrapper implemented as specified in LSMP spec.
     */
    class tcp_connection {
    public:
        /**
         * Initiate outgoing connection.
         */
        tcp_connection(asio::io_service& io_service, const std::string& target_host, uint16_t port);

        /**
         * Construct from existing socket, assumes it's already open.
         *
         * Takes ownership of socket object.
         */
        tcp_connection(asio::ip::tcp::socket&& socket) : socket(std::move(socket)) {}

        /**
         * Close connection and free socket.
         *
         * Cancels any async operations.
         */
        ~tcp_connection();

        using async_error         = std::optional<std::variant<asio::system_error, corrupted_packet_error, std::domain_error>>;
        using async_read_callback = std::function<void(const std::vector<uint8_t>&, const async_error&)>;
        using async_send_callback = std::function<void(const async_error&)>;

        /**
         * Blocking read operation.
         *
         * Thread-safe (synchronized with \ref send_packet and self).
         * May throw asio::system_error on I/O error or \ref corrupted_packet_error.
         */
        std::vector<uint8_t> read_packet();

        /**
         * Blocking send operation.
         *
         * Thread-safe (synchronized with \ref read_packet and self).
         * May throw asio::system_error on I/O error or \ref corrupted_packet_error..
         */
        void send_packet(const std::vector<uint8_t>& packet);

        /**
         * Async read operation, returns instantly.
         *
         * \warning This operation is NOT thread-safe.
         */
        void async_read_packet(const async_read_callback& callback) noexcept;

        /**
         * Async send operation.
         *
         * Packet copied to internal buffer, so it's save to destroy it after call to this function.
         * \warning This operation is NOT thread-safe.
         */
        void async_send_packet(const std::vector<uint8_t>& packet, const async_send_callback& callback) noexcept;

        /**
         * Cancel async operation if any.
         *
         * \warning Underlaying stream may be left in inconsistent state. So
         * all I/O functions will throw \ref corrupted_packet_error after this operation.
         */
        void cancel_async() noexcept;

        asio::ip::tcp::socket socket;
    private:
        void read_async_inner(async_read_callback callback);

        std::mutex socket_access_mutex;
        std::vector<uint8_t> async_buffer;
        uint64_t async_read_excepted_size = 0;

    };
} // namespace lsmp
