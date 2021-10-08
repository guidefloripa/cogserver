/* main.cpp */

#include "server.h"

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#include <iostream>
#include <fstream>

namespace po = boost::program_options;

// --------------------------------------------

int main(int argc, char* argv[])
{
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
        std::cout << "Config: " << vm["config"].as<std::string>() << std::endl;
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

// --------------------------------------------
