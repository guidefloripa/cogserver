/* session.cpp */

#include "session.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

#include <iostream>
#include <fstream>
#include <memory>

using boost::asio::ip::tcp;

#define DEBUG_MESSAGE

// --------------------------------------------

Session::Session(tcp::socket socket,
                 boost::asio::io_context& io_context,
                 int timeout_,
                 int client_id_,
                 int file_maxsize_,
                 const std::string& file_prefix_)
: socket(std::move(socket)),
    timer(io_context),
    timeout(timeout_),
    client_id(client_id_),
    file(nullptr),
    file_bytes_write(file_maxsize_),
    file_id(0),
    file_maxsize(file_maxsize_),
    file_prefix(file_prefix_)
{
    
    boost::posix_time::ptime time = boost::posix_time::second_clock::local_time();
    boost::posix_time::time_facet facet("%Y%m%d%H%M%S");
    
    filename_stream.imbue(std::locale(std::locale::classic(), &facet));
    filename_stream << time;
   
#ifdef DEBUG_MESSAGE
    std::cout << "Create Session (client " << client_id << ")" <<std::endl;
#endif
}

// --------------------------------------------

Session::~Session()
{
    if (file)
        delete file;

#ifdef DEBUG_MESSAGE
    std::cout << "End Session (client " << client_id << ")" << std::endl;
#endif
}

// --------------------------------------------

void Session::Start()
{
    DoRead();
    StartTimer();
}

// --------------------------------------------

void Session::StartTimer()
{
    timer.expires_after(std::chrono::seconds(timeout));
    timer.async_wait(std::bind(&Session::OnTimeout, shared_from_this(), std::placeholders::_1));
}

// --------------------------------------------

void Session::OnTimeout(boost::system::error_code const& ec)
{
    if (ec == boost::asio::error::operation_aborted)
        return;
    
    socket.close();
    if (file)
        file->flush();
    
#ifdef DEBUG_MESSAGE
    std::cout << "Cancel Session (client " << client_id << ")" << std::endl;
#endif
}

// --------------------------------------------

void Session::OpenNewDirectory()
{
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

// --------------------------------------------

void Session::OpenNewFile()
{
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

// --------------------------------------------

void Session::DoRead()
{
    socket.async_read_some(boost::asio::buffer(buffer),
                            std::bind(&Session::OnRead, shared_from_this(),
                                      std::placeholders::_1,
                                      std::placeholders::_2));
}

// --------------------------------------------

void Session::DoWrite(std::size_t length)
{
    int len_w = 0;
    int to_write;
    while (len_w < length) {
        if (file_bytes_write == file_maxsize)
            OpenNewFile();
        
        to_write = length - len_w;
        if (to_write > (file_maxsize-file_bytes_write))
            to_write = file_maxsize-file_bytes_write;
        
        std::copy(buffer.begin()+len_w, buffer.begin()+len_w+to_write, std::ostream_iterator<char>{*file, ""});
        //file->flush();
        file_bytes_write += to_write;
        len_w += to_write;
    }
    
    DoRead();
    
    /*boost::asio::async_write(socket_,
                             boost::asio::buffer(buffer_, length),
                             std::bind(&Session::OnWrite, shared_from_this(),
                                       std::placeholders::_1,
                                       std::placeholders::_2));*/
}

// --------------------------------------------

void Session::OnRead(boost::system::error_code ec, std::size_t length)
{
    StartTimer();
    
    if (!ec) {
#ifdef DEBUG_MESSAGE
        std::cout << "Read (client " << client_id << "): " << length << std::endl;
#endif
        DoWrite(length);
    }
    else {
        if (ec == boost::asio::error::eof) {
            std::cerr << "Socket read EOF: " << ec.message() << std::endl;
        } else if (ec == boost::asio::error::operation_aborted) {
            std::cerr << "Socket operation aborted: " << ec.message() << std::endl;
        } else {
            std::cerr << "Socket read error: " << ec.message() << std::endl;
        }
    }
}

// --------------------------------------------
