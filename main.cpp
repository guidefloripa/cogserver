#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

#include <iostream>
#include <fstream>

#include <sys/stat.h>

// --------------------------------------------

namespace po = boost::program_options;
using boost::asio::ip::tcp;

enum { BUF_SIZE = 1024 };

// --------------------------------------------

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, boost::asio::io_context& io_context, int timeout_, int client_id_,  int file_maxsize_,
       const std::string& file_prefix_) : socket(std::move(socket)), timer(io_context), timeout(timeout_), client_id(client_id_), file(nullptr), file_bytes_write(file_maxsize_), file_id(0), file_maxsize(file_maxsize_), file_prefix(file_prefix_) {
        
        boost::posix_time::ptime time = boost::posix_time::second_clock::local_time();
        boost::posix_time::time_facet facet("%Y%m%d%H%M%S");
        
        filename_stream.imbue(std::locale(std::locale::classic(), &facet));
        filename_stream << time;
        
        std::cout << "Create Session (client " << client_id << ")" <<std::endl;
    }
    
    ~Session() {
        if (file)
            delete file;
            
        std::cout << "End Session (client " << client_id << ")" << std::endl;
    }
    
    void Start() {
        DoRead();
        
        timer.expires_after(std::chrono::seconds(timeout));
        timer.async_wait(std::bind(&Session::OnTimeout, shared_from_this(), std::placeholders::_1));
    }
    
private:
    
    void OnTimeout(boost::system::error_code const& ec) {
        if (ec == boost::asio::error::operation_aborted)
            return;
        std::cout << "Cancel Session (client " << client_id << ")" << std::endl;
        socket.close();
    }
    
    void OpenNewDirectory() {
        if (!directory.empty())
            return;
        
        int n = 0;
        while (true) {
            std::stringstream temp;
            n++;
            
            temp << "cnx_" << filename_stream.str();
            if (n > 1)
                temp << "_" << n;
            
            boost::filesystem::path dir(temp.str());
            if (!boost::filesystem::exists(dir)) {
                boost::filesystem::create_directory(dir);
                
                temp << boost::filesystem::path::preferred_separator;
                directory = temp.str();
                break;
            }
        }
    }
    
    void OpenNewFile() {
        if (file)
            delete file;
        
        std::stringstream temp;
        file_id++;
        
        OpenNewDirectory();
        temp << directory;
        
        if (!file_prefix.empty())
            temp << file_prefix << "_";
        
        temp << filename_stream.str();
        
        if (file_id > 1)
            temp << "_" << file_id;
        
        file = new std::ofstream(temp.str(), std::ios::out | std::ofstream::binary);
        file_bytes_write = 0;
    }
    
    void DoRead() {
        socket.async_read_some(boost::asio::buffer(buffer),
                                std::bind(&Session::OnRead, shared_from_this(),
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    }
    
    void DoWrite(std::size_t length) {
        int w = 0;
        int to_write;
        while (w < length) {
            if (file_bytes_write == file_maxsize)
                OpenNewFile();
            
            to_write = length - w;
            if (to_write > (file_maxsize-file_bytes_write))
                to_write = file_maxsize-file_bytes_write;
            
            std::copy(buffer.begin()+w, buffer.begin()+w+to_write, std::ostream_iterator<char>{*file, ""});
            file->flush();
            file_bytes_write += to_write;
            w += to_write;
        }
        
        DoRead();
        
        /*boost::asio::async_write(socket_,
                                 boost::asio::buffer(buffer_, length),
                                 std::bind(&Session::OnWrite, shared_from_this(),
                                           std::placeholders::_1,
                                           std::placeholders::_2));*/
    }
    
    void OnRead(boost::system::error_code ec, std::size_t length) {
        timer.expires_after(std::chrono::seconds(timeout));
        timer.async_wait(std::bind(&Session::OnTimeout, shared_from_this(), std::placeholders::_1));
        
        if (!ec) {
            std::cout << "Read (client " << client_id << "): " << length << std::endl;
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
    
    tcp::socket socket;
    std::array<char, BUF_SIZE> buffer;
    boost::asio::steady_timer timer;
    int timeout;
    int client_id;
    
    std::string directory;
    std::stringstream filename_stream;
    std::ofstream* file;
    int file_bytes_write;
    int file_id;
    int file_maxsize;
    const std::string& file_prefix;
};

// --------------------------------------------

class Server {
public:
    Server(boost::asio::io_context& io_context_, int port_, int socket_timeout_, int file_maxsize_,
    const std::string& file_prefix_)
    : io_context(io_context_), acceptor(io_context_, tcp::endpoint(tcp::v4(), port_)), socket_timeout(socket_timeout_), client_sequence(0), file_maxsize(file_maxsize_), file_prefix(file_prefix_) {
        DoAccept();
    }
    
private:
    void DoAccept() {
        acceptor.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), io_context, socket_timeout, ++client_sequence, file_maxsize, file_prefix)->Start();
            }
            DoAccept();
        });
    }
    
    boost::asio::io_context& io_context;
    tcp::acceptor acceptor;
    int socket_timeout;
    int client_sequence;
    int file_maxsize;
    const std::string& file_prefix;
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
        Server server(io_context, port, socket_timeout, file_maxsize, file_prefix);
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    
    return 0;
}
