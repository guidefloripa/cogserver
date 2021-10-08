#ifndef _SERVER_H_
#define _SERVER_H_

#include <boost/asio.hpp>

class Server {
public:
    
    Server(boost::asio::io_context& io_context_, int port_, int socket_timeout_, int file_maxsize_, const std::string& file_prefix_);
    
private:
    
    void DoAccept();
    
    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    int socket_timeout;
    int client_sequence;
    int file_maxsize;
    const std::string& file_prefix;
};

#endif /* _SERVER_H_ */
