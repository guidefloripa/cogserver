#ifndef _SESSION_H_
#define _SESSION_H_

#include <boost/asio.hpp>

enum { BUF_SIZE = 1024 };

// --------------------------------------------

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::ip::tcp::socket socket,
            boost::asio::io_context& io_context,
            int timeout_,
            int client_id_,
            int file_maxsize_,
            const std::string& file_prefix_);
    
    ~Session();
    
    void Start();
    
private:
    void StartTimer();
    void OnTimeout(boost::system::error_code const& ec);
    
    void OpenNewDirectory() ;
    void OpenNewFile() ;
    
    void DoRead() ;
    void DoWrite(std::size_t length);
    
    void OnRead(boost::system::error_code ec, std::size_t length);
    
    boost::asio::ip::tcp::socket socket;
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

#endif /* _SESSION_H_ */
