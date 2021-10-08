#include "server.h"

#include "session.h"

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

// --------------------------------------------

Server::Server(boost::asio::io_context& io_context_,
               int port_,
               int socket_timeout_,
               int file_maxsize_,
               const std::string& file_prefix_)
: io_context(io_context_),
    acceptor(io_context_, tcp::endpoint(tcp::v4(), port_)),
    socket_timeout(socket_timeout_),
    client_sequence(0),
    file_maxsize(file_maxsize_),
    file_prefix(file_prefix_)
{
    DoAccept();
}

// --------------------------------------------

void Server::DoAccept()
{
    acceptor.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<Session>(std::move(socket), io_context, socket_timeout, ++client_sequence, file_maxsize, file_prefix)->Start();
        }
        DoAccept();
    });
}

// --------------------------------------------
