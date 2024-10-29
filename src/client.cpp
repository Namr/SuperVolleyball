#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <queue>
#include <string>

#include <raylib.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include "network_signals.hpp"

using std::chrono::duration;
using std::chrono::seconds;
using std::chrono::steady_clock;

constexpr size_t INPUT_HISTORY_CAPACITY = 30;
constexpr int SCENE_MAIN_MENU = 0;
constexpr int SCENE_ROOM_SELECT = 1;
constexpr int SCENE_SETTINGS = 2;
constexpr std::array<std::pair<int, int>, 4> AVAILABLE_RESOLUTIONS = {
    std::make_pair(800, 450), std::make_pair(1280, 820),
    std::make_pair(1920, 1080), std::make_pair(2560, 1440)};

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
    }
    network_interface_ = SteamNetworkingSockets();

    // connect to server
    // TODO set server address from raylib input
    SteamNetworkingIPAddr server_address;
    server_address.ParseString("64.23.207.248:25565");
    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               (void *)connectionStatusCallback);
    connection_ =
        network_interface_->ConnectByIPAddress(server_address, 1, &opt);
    if (connection_ == k_HSteamNetConnection_Invalid) {
      std::cout << "Server connection parameters were invalid!" << std::endl;
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
  bool connected = false;
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
    case k_ESteamNetworkingConnectionState_Connected:
      connected = true;
      break;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
      connected = false;
      std::cout << "We lost connection the server" << std::endl;
      exit(1); // TODO: maybe try to gracefully exit back to the title screen
               // instead of crashing?
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
  // game pieces
  DrawRectangle((int)state.p1_paddle.pos.x, (int)state.p1_paddle.pos.y,
                (int)paddle_width, (int)paddle_height, WHITE);
  DrawRectangle((int)state.p2_paddle.pos.x, (int)state.p2_paddle.pos.y,
                (int)paddle_width, (int)paddle_height, WHITE);

  DrawRectangle((int)state.ball.pos.x - ball_radius,
                (int)state.ball.pos.y - ball_radius, (int)ball_radius * 2,
                (int)ball_radius * 2, WHITE);

  DrawRectangle((int)state.ball.pos.x - ball_radius,
                (int)state.ball.pos.y - ball_radius, (int)ball_radius * 2,
                (int)ball_radius * 2, WHITE);

  // divider lines
  constexpr int num_lines = 6;
  constexpr int space_between_divider = 30;
  constexpr int rect_width = 10;
  constexpr int rect_spacing = (arena_height) / num_lines;
  constexpr int rect_height = rect_spacing - space_between_divider;
  for (int i = 0; i < num_lines; i++) {
    DrawRectangle((int)arena_width / 2.0,
                  rect_spacing * i + (space_between_divider / 2.0), rect_width,
                  rect_height, WHITE);
  }

  // score
  char p1_score[20];
  char p2_score[20];
  snprintf(p1_score, 20, "%d", state.p1_score);
  snprintf(p2_score, 20, "%d", state.p2_score);

  DrawText(p1_score, (arena_width / 5), 50, 80, WHITE);
  DrawText(p2_score, 4 * (arena_width / 5), 50, 80, WHITE);
}

class Game {
public:
  Game() = default;
  ~Game() { CloseWindow(); }

  void start() {
    client_.start();
    InitWindow(horizontal_resolution_, vertical_resolution_, "SuperVolleyball");
    SetTargetFPS(60);
    SetExitKey(0);
    client_.updateRoomList();
  }

  void run() {
    while (!WindowShouldClose()) {
      auto frame_start = steady_clock::now();
      BeginDrawing();
      client_.runCallbacks();
      client_.processIncomingMessages();
      ClearBackground(BLACK);

      // menu system
      if (!client_.connected || scene_ != SCENE_ROOM_SELECT) {
        switch (scene_) {
        default:
        case SCENE_MAIN_MENU:
          main_menu();
          break;
        case SCENE_SETTINGS:
          settings();
          break;
        }
      } else {
        // handle joining rooms & playing the game, this depends just
        // on the state the server sends so we don't use the scene state machine
        // anymore
        if (client_.room_state == std::nullopt ||
            client_.room_state->current_room == -1) {
          room_selection();
        } else {
          if (client_.room_state->state == RS_WAITING) {
            wait_for_match_start();
          } else {
            play_game();
          }
        }
      }

      EndDrawing();
      delta_time =
          static_cast<duration<float>>(steady_clock::now() - frame_start)
              .count();
    }
  }

private:
  Client client_;
  size_t selection_ = 0;
  float delta_time = 0.0;
  int scene_ = SCENE_MAIN_MENU;
  int horizontal_resolution_ = 800;
  int vertical_resolution_ = 450;

  void handle_menu_movement(size_t max_selection_value) {
    if (IsKeyReleased(KEY_DOWN)) {
      selection_++;
    } else if (IsKeyReleased(KEY_UP)) {
      selection_--;
    }

    selection_ = std::clamp(selection_, (size_t)0, max_selection_value);
  }

  void main_menu() {
    DrawTextCentered("Welcome to SuperVolleyball!", 400, 120, 20, RAYWHITE);

    if (client_.connected) {
      if (selection_ == 0) {
        DrawTextCentered("< Play >", 400, 160, 20, RAYWHITE);
      } else {
        DrawTextCentered("Play", 400, 160, 20, RAYWHITE);
      }
    } else {
      if (selection_ == 0) {
        DrawTextCentered("< Connecting to server... >", 400, 160, 20, RAYWHITE);
      } else {
        DrawTextCentered("Connecting to server...", 400, 160, 20, RAYWHITE);
      }
    }

    if (selection_ == 1) {
      DrawTextCentered("< Settings >", 400, 200, 20, RAYWHITE);
    } else {
      DrawTextCentered("Settings", 400, 200, 20, RAYWHITE);
    }

    if (IsKeyReleased(KEY_ENTER)) {
      if (selection_ == 0 && client_.connected) {
        scene_ = SCENE_ROOM_SELECT;
        selection_ = 0;
      } else if (selection_ == 1) {
        scene_ = SCENE_SETTINGS;
        selection_ = 0;
      }
    }

    handle_menu_movement(1);
  }

  void settings() {
    std::string resolution = "Resolution: < " +
                             std::to_string(horizontal_resolution_) + " x " +
                             std::to_string(vertical_resolution_) + " >";
    DrawTextCentered(resolution.c_str(), 400, 120, 20, RAYWHITE);

    if (IsKeyReleased(KEY_ESCAPE)) {
      scene_ = SCENE_MAIN_MENU;
      selection_ = 0;
    }

    int old_horiz = horizontal_resolution_;
    int old_vert = vertical_resolution_;

    horizontal_resolution_ = AVAILABLE_RESOLUTIONS[selection_].first;
    vertical_resolution_ = AVAILABLE_RESOLUTIONS[selection_].second;

    if (old_horiz != horizontal_resolution_ ||
        old_vert != vertical_resolution_) {
      SetWindowSize(horizontal_resolution_, vertical_resolution_);
    }

    handle_menu_movement(AVAILABLE_RESOLUTIONS.size() - 1);
  }

  void room_selection() {
    DrawTextCentered(
        "Press R to refresh room list or press C to make a new room", 400, 20,
        20, RAYWHITE);
    DrawTextCentered("Rooms:", 400, 40, 20, RAYWHITE);

    int line_start = 60;
    for (int i = 0; i < client_.rooms.size(); i++) {
      if (i == selection_) {
        std::string text = "< " + std::to_string(client_.rooms[i]) + " >";
        DrawText(text.c_str(), 400, line_start, 20, RAYWHITE);
      } else {
        DrawText(std::to_string(client_.rooms[i]).c_str(), 400, line_start, 20,
                 RAYWHITE);
      }
      line_start += 20;
    }

    if (IsKeyReleased(KEY_C)) {
      client_.makeRoom();
    } else if (IsKeyReleased(KEY_R)) {
      client_.updateRoomList();
    } else if (IsKeyReleased(KEY_ENTER)) {
      client_.joinRoom(client_.rooms[selection_]);
    }
    handle_menu_movement(client_.rooms.size() - 1);
  }

  void wait_for_match_start() {
    std::string room_id =
        "you are in room: " + std::to_string(client_.room_state->current_room);
    std::string room_members =
        "there are " + std::to_string(client_.room_state->num_connected) +
        " players here";
    DrawTextCentered(room_id.c_str(), 400, 100, 20, LIGHTGRAY);
    DrawTextCentered(room_members.c_str(), 400, 120, 20, LIGHTGRAY);
  }

  void play_game() {
    InputMessage input = getInput(delta_time);
    client_.sendInput(input);
    updatePlayerState(*client_.game_state, input, client_.player_index);
    updateGameState(*client_.game_state, delta_time);
    client_.saveFrame(input);
    drawGameState(*client_.game_state);
  }
};

int main() {
  Game game;
  game.start();
  game.run();
  return 0;
}
