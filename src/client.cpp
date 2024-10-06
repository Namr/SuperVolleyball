#include <iostream>
#include <raylib.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <string>

#include "network_signals.hpp"

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

      // deserialize the server request
      ServerNetworkMessage msg;
      {
        std::stringstream ss(std::ios::binary | std::ios_base::app |
                             std::ios_base::in | std::ios_base::out);
        ss.write((char const *)incoming_msg->m_pData, incoming_msg->m_cbSize);
        cereal::BinaryInputArchive dearchive(ss);
        dearchive(msg);
      }

      if (msg.room_state.current_room != -1) {
        current_room_ = msg.room_state.current_room;
      }

      if (msg.room_state.num_connected != -1) {
        num_players_ = msg.room_state.num_connected;
      }

      if (!msg.available_rooms.empty()) {
        rooms_ = std::move(msg.available_rooms);
      }

      incoming_msg->Release();
    }
  }

  void runCallbacks() {
    current_callback_instance_ = this;
    network_interface_->RunCallbacks();
  }

  void updateRoomList() {
    ClientNetworkMessage msg;
    msg.room_request.command = RR_LIST_ROOMS;
    sendRequest(msg);
  }

  void joinRoom(uint16_t desired_room) {
    ClientNetworkMessage msg;
    msg.room_request.command = RR_JOIN_ROOM;
    msg.room_request.desired_room = desired_room;
    sendRequest(msg);
  }

  void makeRoom() {
    ClientNetworkMessage msg;
    msg.room_request.command = RR_MAKE_ROOM;
    sendRequest(msg);
  }

  void sendRequest(ClientNetworkMessage &msg) {
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection_, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Reliable, nullptr);
  }

  uint16_t current_room_ = -1;
  uint16_t num_players_ = -1;
  std::vector<int> rooms_;

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

void DrawTextCentered(const std::string &text, int x, int y, int font_size,
                      Color color) {
  int width = MeasureText(text.c_str(), font_size) / 2;
  DrawText(text.c_str(), x - width, y, font_size, color);
}

int main() {
  Client client;
  client.start();
  InitWindow(800, 450, "SuperVolleyball");

  client.updateRoomList();

  size_t selected_room = 0;
  while (!WindowShouldClose()) {
    BeginDrawing();
    client.runCallbacks();
    client.processIncomingMessages();
    ClearBackground(BLACK);
    // handle joining rooms
    if (client.current_room_ == static_cast<uint16_t>(-1)) {
      DrawTextCentered("Welcome to SuperVolleyball!", 400, 0, 20, RAYWHITE);
      DrawTextCentered(
          "Press R to refresh room list or press C to make a new room", 400, 20,
          20, RAYWHITE);
      DrawTextCentered("Rooms:", 400, 40, 20, RAYWHITE);

      int line_start = 60;
      for (int i = 0; i < client.rooms_.size(); i++) {
        if (i == selected_room) {
          std::string text = "< " + std::to_string(client.rooms_[i]) + " >";
          DrawText(text.c_str(), 400, line_start, 20, RAYWHITE);
        } else {
          DrawText(std::to_string(client.rooms_[i]).c_str(), 400, line_start,
                   20, RAYWHITE);
        }
        line_start += 20;
      }

      if (IsKeyReleased(KEY_C)) {
        client.makeRoom();
      } else if (IsKeyReleased(KEY_R)) {
        client.updateRoomList();
      } else if (IsKeyReleased(KEY_ENTER)) {
        client.joinRoom(client.rooms_[selected_room]);
      } else if (IsKeyReleased(KEY_DOWN)) {
        selected_room++;
      } else if (IsKeyReleased(KEY_UP)) {
        selected_room--;
      }

      selected_room = std::clamp(selected_room, 0UL, client.rooms_.size() - 1);
    } else {
      // wait for match to start
      std::string room_id =
          "you are in room: " + std::to_string(client.current_room_);
      std::string room_members =
          "there are " + std::to_string(client.num_players_) + " players here";
      DrawTextCentered(room_id.c_str(), 400, 100, 20, LIGHTGRAY);
      DrawTextCentered(room_members.c_str(), 400, 120, 20, LIGHTGRAY);

      // actually play the match
    }

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
