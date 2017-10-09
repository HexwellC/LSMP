#include "tcp_server.hpp"

lsmp::tcp_server::tcp_server(asio::io_service& io_service, const asio::ip::tcp::endpoint& listen_endpoint)
    : acceptor(io_service, listen_endpoint) {}

lsmp::tcp_connection lsmp::tcp_server::wait_for_connection() {
    asio::ip::tcp::socket socket(acceptor.get_io_service());
    acceptor.accept(socket);
    return tcp_connection(std::move(socket));
}

void lsmp::tcp_server::async_wait_for_connection(lsmp::tcp_server::async_accept_callback callback) noexcept {
    auto socket = std::make_shared<asio::ip::tcp::socket>(acceptor.get_io_service());

    acceptor.async_accept(*socket, [socket, callback](asio::error_code ec) {
        callback(tcp_connection(std::move(*socket)), ec);
    });
}
