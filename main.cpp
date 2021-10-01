#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/core/ignore_unused.hpp>

#include <iostream>
#include <fstream>

// --------------------------------------------

namespace po = boost::program_options;
using boost::asio::ip::tcp;

enum { BUF_SIZE = 1024 };

// --------------------------------------------

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, boost::asio::io_context& io_context) : socket_(std::move(socket)), timer(io_context) {
        std::cout << "Create Session" << std::endl;
    }
    
    ~Session() {
        std::cout << "End Session" << std::endl;
    }
    
    void Start() {
        DoRead();
        
        timer.expires_from_now(boost::posix_time::seconds(5));
        timer.async_wait(std::bind(&Session::OnTimeout, shared_from_this(), std::placeholders::_1));
    }
    
    void OnTimeout(boost::system::error_code const& ec) {
        if (ec)
            return;
        std::cout << "Cancel Session" << std::endl;
        socket_.close();
    }
    
private:
    void DoRead() {
        socket_.async_read_some(boost::asio::buffer(buffer_),
                                std::bind(&Session::OnRead, shared_from_this(),
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    }
    
    void DoWrite(std::size_t length) {
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(buffer_, length),
                                 std::bind(&Session::OnWrite, shared_from_this(),
                                           std::placeholders::_1,
                                           std::placeholders::_2));
    }
    
    void OnRead(boost::system::error_code ec, std::size_t length) {
        timer.expires_from_now(boost::posix_time::seconds(5));
        timer.async_wait(std::bind(&Session::OnTimeout, shared_from_this(), std::placeholders::_1));
        
        if (!ec) {
            std::cout << "Read : " << length << std::endl;
            DoWrite(length);
        } else {
            if (ec == boost::asio::error::eof) {
                std::cerr << "Socket read EOF: " << ec.message() << std::endl;
            } else if (ec == boost::asio::error::operation_aborted) {
                // The socket of this connection has been closed.
                // This happens, e.g., when the server was stopped by a signal (Ctrl-C).
                std::cerr << "Socket operation aborted: " << ec.message() << std::endl;
            } else {
                std::cerr << "Socket read error: " << ec.message() << std::endl;
            }
        }
    }
    
    void OnWrite(boost::system::error_code ec, std::size_t length) {
        boost::ignore_unused(length);
        
        if (!ec) {
            DoRead();
        }
    }
    
    tcp::socket socket_;
    std::array<char, BUF_SIZE> buffer_;
    boost::asio::deadline_timer timer;
};

// --------------------------------------------

class Server {
public:
    Server(boost::asio::io_context& io_context, int port)
    : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        DoAccept();
    }
    
private:
    void DoAccept() {
        acceptor_.async_accept(
                               [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), io_context_)->Start();
            }
            DoAccept();
        });
    }
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

// --------------------------------------------

int main(int argc, char* argv[]) {
    std::string config_filename;
    int port;
    int file_maxsize;
    std::string file_prefix;
    int socket_timeout;
    
    po::options_description server_options("settings");
    server_options.add_options()
    ("port", po::value<int>(&port)->default_value(5500))
    ("file_maxsize", po::value<int>(&file_maxsize)->default_value(1000))
    ("file_prefix", po::value<std::string>(&file_prefix))
    ("socket_timeout", po::value<int>(&socket_timeout)->default_value(60));
    ;
    
    po::options_description program_options("Program options");
    program_options.add_options()
    ("help", "produce help message")
    ("config,c", po::value<std::string>()->default_value("config.cfg"), "configuration file")
    ;
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, program_options), vm);
    po::notify(vm);
    
    if (vm.count("help")) {
        std::cout << program_options << std::endl;
        return 1;
    }
    
    if (vm.count("config")) {
        std::cout << "Config: " << vm["config"].as<std::string>() << "\n";
        config_filename = vm["config"].as<std::string>();
    }
    else {
        std::cout << "Config was not set!" << std::endl;
        return 1;
    }
    
    try {
        std::ifstream config_file(config_filename);
        po::variables_map vm_server;
        po::store(po::parse_config_file(config_file, server_options), vm_server);
        po::notify(vm_server);
        
        std::cout << "Port: " << port << std::endl;
        std::cout << "File maxsize: " << file_maxsize << std::endl;
        std::cout << "File prefix: " << file_prefix << std::endl;
        std::cout << "Socket timeout: " << socket_timeout << std::endl;
    }
    catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    
    try {
        boost::asio::io_context io_context;
        Server server(io_context, port);
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    
    return 0;
}
