#include <chrono>
#include <iostream>
#include <string>

#include <raylib.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include "network_signals.hpp"

using std::chrono::duration;
using std::chrono::seconds;
using std::chrono::steady_clock;

constexpr size_t INPUT_HISTORY_CAPACITY = 30;

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

      room_state = msg.room_state;
      // find the time in our history buffer where this recvd state was computed
      // from if that state does not match what we recvd, we force update it and
      // then recompute using future inputs the server presumably has not
      // consumed yet
      if (room_state->state == RS_PLAYING) {
        bool is_recomputing = false;
        bool found_id = false;
        GameState running_gamestate;
        for (std::pair<InputMessage, GameState> &p : input_history) {
          if (is_recomputing) {
            updatePlayerState(running_gamestate, p.first, player_index);
            p.second = running_gamestate;
          } else {
            // find the timestamp where this recv'd state was computed from
            if (p.first.id == msg.last_input_id) {
              found_id = true;
              if (msg.game_state != p.second) {
                is_recomputing = true;
                p.second = msg.game_state;
                running_gamestate = msg.game_state;
              } else {
                break;
              }
            }
          }
        }

        // if we had to recompute, update the master gamestate as well
        if (is_recomputing) {
          game_state = input_history.back().second;
        }

        // if we outran our buffer, force a jump
        if (!found_id) {
          std::cout << "forced an update" << std::endl;
          game_state = msg.game_state;
        }
      }

      player_index = msg.player_number != -1 ? msg.player_number : player_index;

      if (!msg.available_rooms.empty()) {
        rooms = std::move(msg.available_rooms);
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

  void saveFrame(InputMessage input) {
    input_history.push_back(std::make_pair(input, *game_state));
    if (input_history.size() > INPUT_HISTORY_CAPACITY) {
      input_history.pop_front();
    }
  }

  void sendInput(const InputMessage &input) {
    ClientNetworkMessage msg;
    msg.room_request.command = RR_NO_REQUEST;
    msg.inputs.push_back(input);
    sendRequestUnreliably(msg);
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

  void sendRequestUnreliably(ClientNetworkMessage &msg) {
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection_, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Unreliable, nullptr);
  }

  std::vector<int> rooms;
  int player_index = -1;
  std::optional<RoomState> room_state;
  std::optional<GameState> game_state;
  std::deque<std::pair<InputMessage, GameState>> input_history;

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

InputMessage getInput(float delta_time) {
  static int input_id = 0;
  InputMessage i;
  i.id = input_id++;
  i.delta_time = delta_time;
  i.up = IsKeyDown(KEY_UP);
  i.down = IsKeyDown(KEY_DOWN);
  return i;
}

void drawGameState(const GameState &state) {
  DrawRectangle((int)state.p1_paddle.pos.x, (int)state.p1_paddle.pos.y,
                (int)paddle_width, (int)paddle_height, WHITE);
  DrawRectangle((int)state.p2_paddle.pos.x, (int)state.p2_paddle.pos.y,
                (int)paddle_width, (int)paddle_height, WHITE);

  DrawCircle((int)state.ball.pos.x, (int)state.ball.pos.y, ball_radius, WHITE);
}

int main() {
  Client client;
  client.start();
  InitWindow(800, 450, "SuperVolleyball");
  SetTargetFPS(60);

  client.updateRoomList();

  size_t selected_room = 0;
  float delta_time = 0.0;
  while (!WindowShouldClose()) {
    auto frame_start = steady_clock::now();
    BeginDrawing();
    client.runCallbacks();
    client.processIncomingMessages();
    ClearBackground(BLACK);
    // handle joining rooms
    if (client.room_state == std::nullopt ||
        client.room_state->current_room == -1) {
      DrawTextCentered("Welcome to SuperVolleyball!", 400, 0, 20, RAYWHITE);
      DrawTextCentered(
          "Press R to refresh room list or press C to make a new room", 400, 20,
          20, RAYWHITE);
      DrawTextCentered("Rooms:", 400, 40, 20, RAYWHITE);

      int line_start = 60;
      for (int i = 0; i < client.rooms.size(); i++) {
        if (i == selected_room) {
          std::string text = "< " + std::to_string(client.rooms[i]) + " >";
          DrawText(text.c_str(), 400, line_start, 20, RAYWHITE);
        } else {
          DrawText(std::to_string(client.rooms[i]).c_str(), 400, line_start, 20,
                   RAYWHITE);
        }
        line_start += 20;
      }

      if (IsKeyReleased(KEY_C)) {
        client.makeRoom();
      } else if (IsKeyReleased(KEY_R)) {
        client.updateRoomList();
      } else if (IsKeyReleased(KEY_ENTER)) {
        client.joinRoom(client.rooms[selected_room]);
      } else if (IsKeyReleased(KEY_DOWN)) {
        selected_room++;
      } else if (IsKeyReleased(KEY_UP)) {
        selected_room--;
      }

      selected_room = std::clamp(selected_room, 0UL, client.rooms.size() - 1);
    } else {
      if (client.room_state->state == RS_WAITING) {
        // wait for match to start
        std::string room_id = "you are in room: " +
                              std::to_string(client.room_state->current_room);
        std::string room_members =
            "there are " + std::to_string(client.room_state->num_connected) +
            " players here";
        DrawTextCentered(room_id.c_str(), 400, 100, 20, LIGHTGRAY);
        DrawTextCentered(room_members.c_str(), 400, 120, 20, LIGHTGRAY);
      } else {
        // actually play the match

        InputMessage input = getInput(delta_time);
        client.sendInput(input);
        updatePlayerState(*client.game_state, input, client.player_index);
        updateGameState(*client.game_state, delta_time);
        client.saveFrame(input);
        drawGameState(*client.game_state);
      }
    }

    EndDrawing();
    delta_time =
        static_cast<duration<float>>(steady_clock::now() - frame_start).count();
  }

  CloseWindow();

  return 0;
}
