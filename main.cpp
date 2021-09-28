#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>

namespace po = boost::program_options;
using namespace std;

void to_cout(const std::vector<std::string> &v)
{
  std::copy(v.begin(), v.end(), std::ostream_iterator<std::string>{
    std::cout, " "});
}

int main(int argc, char* argv[]) {
    string config_filename;
    int port;
    int file_maxsize;
    string file_prefix;
    int socket_timeout;
    
    po::options_description server_options("settings");
    server_options.add_options()
        ("port", po::value<int>(&port)->default_value(5500))
        ("file_maxsize", po::value<int>(&file_maxsize)->default_value(1000))
        ("file_prefix", po::value<string>(&file_prefix))
        ("socket_timeout", po::value<int>(&socket_timeout)->default_value(60));
    ;
    
    po::options_description program_options("Allowed options");
    program_options.add_options()
        ("help", "produce help message")
        ("config,c", po::value<string>()->default_value("config.cfg"), "configuration file")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, program_options), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << program_options << endl;
        return 1;
    }
    
    if (vm.count("config")) {
        cout << "Config: " << vm["config"].as<string>() << "\n";
        config_filename = vm["config"].as<string>();
    }
    else {
        cout << "Config was not set!" << endl;
        return 1;
    }
    
    try {
        ifstream config_file(config_filename);
        po::variables_map vm_server;
        po::store(po::parse_config_file(config_file, server_options), vm_server);
        po::notify(vm_server);
        
        cout << "Port: " << port << endl;
        cout << "File maxsize: " << file_maxsize << endl;
        cout << "File prefix: " << file_prefix << endl;
        cout << "Socket timeout: " << socket_timeout << endl;
    }
    catch(std::exception& E)
    {
        std::cout << E.what() << std::endl;
    }
}
