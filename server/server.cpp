#include "rtc/rtc.hpp"
#include <iostream>
#include <variant>
#include <chrono>
#include <thread>

int main() {
  rtc::WebSocketServer::Configuration server_config;
  server_config.enableTls = false;
  server_config.port = 8080;
  server_config.bindAddress = "127.0.0.1";
  rtc::WebSocketServer ws_server(server_config);
  
  std::vector<std::shared_ptr<rtc::WebSocket>> client_connections;
  

  ws_server.onClient([&](std::shared_ptr<rtc::WebSocket> ws) {
    std::cout << "New Websocket client" << std::endl;
    
    ws->onOpen([]() {
        std::cout << "on open" << std::endl;
      });

    ws->onMessage([](std::variant<rtc::binary, rtc::string> message) {
        std::cout << "recv: " << std::get<rtc::string>(message) << std::endl;
      });
  
    client_connections.push_back(ws);
  });

  while(true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  return 0;
}
