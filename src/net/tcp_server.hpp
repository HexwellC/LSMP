#pragma once

#include <functional>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include "tcp_connection.hpp"

namespace lsmp {

class tcp_server {
public:
    using async_accept_callback = std::function<void(tcp_connection&&, const asio::error_code&)>;

    /**
     * Bind listener to specified endpoint.
     */
    tcp_server(asio::io_service& io_service, const asio::ip::tcp::endpoint& listen_endpoint);

    /**
     * Wait for incoming connections on bound endpoint.
     *
     * May throw asio::system_error.
     */
    tcp_connection wait_for_connection();

    /**
     * Asynchronously wait for incoming connections on bound endpoint.
     *
     * Callback will receive either valid connection with "no error" ec or invalid connection and error code.
     */
    void async_wait_for_connection(async_accept_callback callback) noexcept;

    asio::ip::tcp::acceptor acceptor;
};

} // namespace lsmp

