#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

#include <iostream>
#include <fstream>

// --------------------------------------------

namespace po = boost::program_options;
using boost::asio::ip::tcp;

enum { BUF_SIZE = 1024 };

// --------------------------------------------

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, boost::asio::io_context& io_context, int timeout, int client_id) : socket_(std::move(socket)), timer_(io_context), timeout_(timeout), client_id_(client_id), file_(nullptr), file_id_(0) {
        
        boost::posix_time::ptime time = boost::posix_time::second_clock::local_time();
        boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%Y%m%d%H%M%S");
        
        filename_stream_.imbue(std::locale(std::locale::classic(), facet));
        filename_stream_ << time;
        
        std::cout << "Create Session (client " << client_id_ << ")" <<std::endl;
    }
    
    ~Session() {
        if (file_)
            delete file_;
            
        std::cout << "End Session (client " << client_id_ << ")" << std::endl;
    }
    
    void Start() {
        OpenFile();
        DoRead();
        
        timer_.expires_from_now(boost::posix_time::seconds(timeout_));
        timer_.async_wait(std::bind(&Session::OnTimeout, shared_from_this(), std::placeholders::_1));
    }
    
    void OnTimeout(boost::system::error_code const& ec) {
        if (ec)
            return;
        std::cout << "Cancel Session (client " << client_id_ << ")" << std::endl;
        socket_.close();
    }
    
private:
    void OpenFile() {
        if (file_) {
            delete file_;
        }
        
        std::string filename;
        file_id_++;
        
        if (file_id_ > 1) {
            std::stringstream temp;
            temp << filename_stream_.str() << "_" << file_id_;
            filename = temp.str();
        }
        else {
            filename = filename_stream_.str();
        }
        
        file_ = new std::ofstream(filename, std::ios::out | std::ofstream::binary);
    }
    
    void DoRead() {
        socket_.async_read_some(boost::asio::buffer(buffer_),
                                std::bind(&Session::OnRead, shared_from_this(),
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    }
    
    void DoWrite(std::size_t length) {
        std::copy(buffer_.begin(), buffer_.begin()+length, std::ostream_iterator<char>{*file_, ""});
        file_->flush();
        
        DoRead();
        
        /*boost::asio::async_write(socket_,
                                 boost::asio::buffer(buffer_, length),
                                 std::bind(&Session::OnWrite, shared_from_this(),
                                           std::placeholders::_1,
                                           std::placeholders::_2));*/
    }
    
    void OnRead(boost::system::error_code ec, std::size_t length) {
        timer_.expires_from_now(boost::posix_time::seconds(timeout_));
        timer_.async_wait(std::bind(&Session::OnTimeout, shared_from_this(), std::placeholders::_1));
        
        if (!ec) {
            std::cout << "Read (client " << client_id_ << "): " << length << std::endl;
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
    boost::asio::deadline_timer timer_;
    int timeout_;
    int client_id_;
    
    std::stringstream filename_stream_;
    std::ofstream* file_;
    int file_id_;
};

// --------------------------------------------

class Server {
public:
    Server(boost::asio::io_context& io_context, int port, int socket_timeout)
    : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), socket_timeout_(socket_timeout), client_sequence_(0) {
        DoAccept();
    }
    
private:
    void DoAccept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), io_context_, socket_timeout_, ++client_sequence_)->Start();
            }
            DoAccept();
        });
    }
    
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    int socket_timeout_;
    int client_sequence_;
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
        if (!config_file.is_open()) {
            std::cout << config_filename << " not found. Using default configuration" << std::endl;
        }
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
        Server server(io_context, port, socket_timeout);
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    
    return 0;
}
