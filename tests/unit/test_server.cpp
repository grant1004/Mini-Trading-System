#include "network/tcp_server.cpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    try {
        mts::tcp_server::TCPServer server(8080);
        
        if (!server.start()) {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }
        
        std::cout << "Server running. Press Enter to stop..." << std::endl;
        std::cin.get();
        
        server.stop();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}