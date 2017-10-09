#include "tcp_connection.hpp"
#include <asio/connect.hpp>

using namespace asio::ip;

lsmp::tcp_connection::tcp_connection(asio::io_service& io_service, const std::string& target_host, uint16_t port)
    : socket(io_service) {

    tcp::resolver resolver(io_service);
    auto endpoints = resolver.resolve({ target_host, std::to_string(port), tcp::resolver::query::numeric_service });
    asio::connect(socket, endpoints);
}

lsmp::tcp_connection::~tcp_connection() {
    cancel_async();
}

std::vector<uint8_t> lsmp::tcp_connection::read_packet() {
    if (!async_buffer.empty()) throw std::domain_error("read_packet called when async operation active");
    std::lock_guard<std::mutex> lock(socket_access_mutex);

    uint8_t marker[4];
    union {
        uint8_t size_bytes[8];
        uint64_t excepted_size;
    };

    // TODO: Implement read timeout
    // Notes:
    // * ASIO doesn't seems to provide simple way to timeout a sync operation.
    // * http://www.boost.org/doc/libs/1_52_0/doc/html/boost_asio/example/timeouts/blocking_tcp_client.cpp

    // read marker and size
    socket.read_some(asio::buffer(marker, sizeof(marker)));
    if (marker[0] != 'L' ||
        marker[1] != 'S' ||
        marker[2] != 'M' ||
        marker[3] != 'P') throw corrupted_packet_error("Missing start marker.");

    socket.read_some(asio::buffer(size_bytes, sizeof(size_bytes)));

    std::vector<uint8_t> result;
    uint16_t byte_buffer;
    while (result.size() < excepted_size) {
        socket.read_some(asio::buffer(&byte_buffer, 1));
        result.push_back(byte_buffer);
    }

    return result;
}

void lsmp::tcp_connection::send_packet(const std::vector<uint8_t>& packet) {
    static const uint8_t marker[] = { 'L', 'S', 'M', 'P' };
    socket.write_some(asio::buffer(marker, sizeof(marker)));
    union {
        uint8_t size_bytes[8];
        uint64_t size;
    };
    size = packet.size();
    socket.write_some(asio::buffer(size_bytes, sizeof(size_bytes)));
    socket.write_some(asio::buffer(packet));
}

void lsmp::tcp_connection::async_read_packet(const lsmp::tcp_connection::async_read_callback& callback) noexcept {
    if (!async_buffer.empty()) {
        callback({}, std::domain_error("Async operation is already active."));
        return;
    }

    read_async_inner(callback);
}

void lsmp::tcp_connection::async_send_packet(const std::vector<uint8_t>& packet, const lsmp::tcp_connection::async_send_callback& callback) noexcept {
    auto send_buffer = std::make_shared<std::vector<uint8_t>>();

    send_buffer->reserve(4 + 8 + packet.size());

    send_buffer->push_back('L');
    send_buffer->push_back('S');
    send_buffer->push_back('M');
    send_buffer->push_back('P');

    union {
        uint8_t size_bytes[8];
        uint64_t size;
    };
    size = packet.size();

    std::copy(size_bytes, size_bytes + sizeof(size_bytes), std::back_inserter(*send_buffer));
    std::copy(packet.begin(), packet.end(), std::back_inserter(*send_buffer));

    socket.async_write_some(asio::buffer(*send_buffer), [send_buffer,callback](const auto& ec, size_t) {
        callback(asio::system_error(ec));
    });
}

void lsmp::tcp_connection::cancel_async() noexcept {
    socket.cancel();
}

void lsmp::tcp_connection::read_async_inner(async_read_callback callback) {
    if (async_buffer.size() == 4 && async_read_excepted_size == 0) {
        // We don't know packet size but have 4 bytes in buffer, looks like marker, let's check it!
        if (async_buffer[0] != 'L' ||
            async_buffer[1] != 'S' ||
            async_buffer[2] != 'M' ||
            async_buffer[3] != 'P') {

            callback({}, corrupted_packet_error("Missing or invalid marker."));
            return;
        }

        // Ok, marker is valid, let's read size.
        async_buffer.resize(8);
        socket.async_read_some(asio::buffer(async_buffer.data(), 8), [this,callback](auto ec, size_t) {
            if (ec) {
                callback({}, asio::system_error(ec));
            }

            read_async_inner(callback);
        });
    }

    if (async_buffer.size() == 8 && async_read_excepted_size == 0) {
        async_read_excepted_size = *reinterpret_cast<uint64_t*>(async_buffer.data());

        socket.async_read_some(asio::buffer(async_buffer), [this,callback](auto ec, size_t) {
            if (ec) {
                callback({}, asio::system_error(ec));
                return;
            }

            callback(async_buffer, {});
        });
    }
}

