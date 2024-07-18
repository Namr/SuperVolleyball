#include <iostream>
#include <raylib.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <string>

class Client {
public:
  Client() = default;

  void start() {
    // init connection lib
    SteamDatagramErrMsg error_msg;
    if (!GameNetworkingSockets_Init(nullptr, error_msg)) {
      std::cout
          << "ERROR: Failed to Intialize Game Networking Sockets because: "
          << error_msg << std::endl;
      exit(1);
    }
    network_interface_ = SteamNetworkingSockets();

    // connect to server
    // TODO set server address from raylib input
    SteamNetworkingIPAddr server_address;
    server_address.ParseString("127.0.0.1:25565");
    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               (void *)connectionStatusCallback);
    connection_ =
        network_interface_->ConnectByIPAddress(server_address, 1, &opt);
    if (connection_ == k_HSteamNetConnection_Invalid) {
      std::cout << "Could not connect to server" << std::endl;
      exit(1);
    }
  }

  ~Client() {
    network_interface_->CloseConnection(connection_, 0, "Client Exiting", true);
  }

  void processIncomingMessages() {
    // go through all messages one at a time
    while (true) {
      ISteamNetworkingMessage *incoming_msg = nullptr;
      int num_msgs = network_interface_->ReceiveMessagesOnConnection(
          connection_, &incoming_msg, 1);

      if (num_msgs == 0) {
        break;
      }

      if (num_msgs < 0) {
        std::cout << "WARNING: we failed to get a message from the poll group"
                  << std::endl;
      }

      std::string msg;
      msg.assign((const char *)incoming_msg->m_pData, incoming_msg->m_cbSize);
      std::cout << msg << std::endl;
      incoming_msg->Release();
    }
  }

  void runCallbacks() {
    current_callback_instance_ = this;
    network_interface_->RunCallbacks();
  }

  void sendString(const std::string &msg) {
    network_interface_->SendMessageToConnection(
        connection_, msg.c_str(), (uint32)msg.length(),
        k_nSteamNetworkingSend_Reliable, nullptr);
  }

private:
  // singleton-ish structure here s.t we can use C API to call callbacks
  // not very cool!
  static Client *current_callback_instance_;

  static void
  connectionStatusCallback(SteamNetConnectionStatusChangedCallback_t *info) {
    current_callback_instance_->onConnectionStatusChanged(info);
  }

  void
  onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info) {
    switch (info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
      std::cout << "We lost connection the server" << std::endl;
      exit(1);
      break;
    default:
      break;
    }
  }

  ISteamNetworkingSockets *network_interface_ = nullptr;
  HSteamNetConnection connection_;
};

Client *Client::current_callback_instance_ = nullptr;

int main() {
  Client client;
  client.start();
  InitWindow(800, 450, "SuperVolleyball");

  while (!WindowShouldClose()) {
    BeginDrawing();
    client.runCallbacks();
    client.processIncomingMessages();
    ClearBackground(RAYWHITE);
    DrawText("there should be a game here.... eventually", 190, 200, 20,
             LIGHTGRAY);

    if (IsKeyReleased(KEY_SPACE)) {
      std::cout << "trying to do this" << std::endl;
      client.sendString("ping");
    }
    EndDrawing();
  }

  CloseWindow();

  return 0;
}
