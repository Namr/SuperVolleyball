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
      constexpr double FRAME_EPSILON = 1.0 / 60.0;
      if (room_state->state == RS_PLAYING) {
        bool is_recomputing = false;
        bool found_id = false;
        GameState running_gamestate;
        for (std::pair<InputMessage, GameState> &p : input_history) {
          if (is_recomputing) {
            double delta_time = (p.first.time - running_gamestate.time);
            updatePlayerState(running_gamestate, p.first, delta_time,
                              player_index);
            running_gamestate.time = p.first.time;
            p.second = running_gamestate;
          } else {
            // find the timestamp where this recv'd state was computed from
            if (std::abs(p.first.time - msg.game_state.time) < FRAME_EPSILON) {
              found_id = true;
              if (msg.game_state != p.second) {
                is_recomputing = true;
                p.second = msg.game_state;
                running_gamestate = msg.game_state;
                running_gamestate.time = p.first.time;
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
          game_state = msg.game_state;
          game_state->time += 1 / 60.0;
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

InputMessage getInput(double time) {
  InputMessage i;
  i.time = time;
  i.up = IsKeyDown(KEY_UP);
  i.down = IsKeyDown(KEY_DOWN);
  return i;
}

void drawGameState(const GameState &state, double w_ratio, double h_ratio) {
  // game pieces
  DrawRectangle((int)state.p1_paddle.pos.x * w_ratio, (int)state.p1_paddle.pos.y * h_ratio,
                (int)paddle_width * w_ratio, (int)paddle_height * h_ratio, WHITE);
  DrawRectangle((int)state.p2_paddle.pos.x * w_ratio, (int)state.p2_paddle.pos.y * h_ratio,
                (int)paddle_width * w_ratio, (int)paddle_height * h_ratio, WHITE);

  DrawRectangle((int)(state.ball.pos.x - ball_radius) * w_ratio,
                (int)(state.ball.pos.y - ball_radius) * h_ratio, (int)ball_radius * 2 * w_ratio,
                (int)ball_radius * 2 * h_ratio, WHITE);

  // divider lines
  constexpr int num_lines = 6;
  const int space_between_divider = 30 * h_ratio;
  const int rect_width = 10 * w_ratio;
  const int rect_spacing = ((arena_height * h_ratio) / num_lines);
  const int rect_height = rect_spacing - space_between_divider;
  for (int i = 0; i < num_lines; i++) {
    DrawRectangle((int)(arena_width / 2.0) * w_ratio,
                  (rect_spacing * i + (space_between_divider / 2.0)), rect_width,
                  (rect_height), WHITE);
  }

  // score
  char p1_score[20];
  char p2_score[20];
  snprintf(p1_score, 20, "%d", state.p1_score);
  snprintf(p2_score, 20, "%d", state.p2_score);

  DrawText(p1_score, (arena_width / 5) * w_ratio, 50 * h_ratio, 80 * h_ratio, WHITE);
  DrawText(p2_score, 4 * (arena_width / 5) * w_ratio, 50 * h_ratio, 80 * h_ratio, WHITE);
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
            time_ += delta_time_;
          }
        }
      }

      EndDrawing();
      delta_time_ =
          static_cast<duration<float>>(steady_clock::now() - frame_start)
              .count();
    }
  }

private:
  Client client_;
  size_t selection_ = 0;
  float delta_time_ = 0.0;
  double time_ = 0.0;
  int scene_ = SCENE_MAIN_MENU;
  int horizontal_resolution_ = 800;
  int vertical_resolution_ = 450;
  double w_ratio_ =  horizontal_resolution_ / arena_width;
  double h_ratio_ = vertical_resolution_ / arena_height;

  void handle_menu_movement(size_t max_selection_value) {
    if (IsKeyReleased(KEY_DOWN)) {
      selection_++;
    } else if (IsKeyReleased(KEY_UP)) {
      selection_--;
    }

    selection_ = std::clamp(selection_, (size_t)0, max_selection_value);
  }

  void main_menu() {
    DrawTextCentered("Welcome to SuperVolleyball!", 400 * w_ratio_, 120 * h_ratio_, 20 * h_ratio_, RAYWHITE);

    if (client_.connected) {
      if (selection_ == 0) {
        DrawTextCentered("< Play >", 400 * w_ratio_, 160 * h_ratio_, 20 * h_ratio_, RAYWHITE);
      } else {
        DrawTextCentered("Play", 400 * w_ratio_, 160 * h_ratio_, 20 * h_ratio_, RAYWHITE);
      }
    } else {
      if (selection_ == 0) {
        DrawTextCentered("< Connecting to server... >", 400 * w_ratio_, 160 * h_ratio_, 20 * h_ratio_, RAYWHITE);
      } else {
        DrawTextCentered("Connecting to server...", 400 * w_ratio_, 160 * h_ratio_, 20 * h_ratio_, RAYWHITE);
      }
    }

    if (selection_ == 1) {
      DrawTextCentered("< Settings >", 400 * w_ratio_, 200 * h_ratio_, 20 * h_ratio_, RAYWHITE);
    } else {
      DrawTextCentered("Settings", 400 * w_ratio_, 200 * h_ratio_, 20 * h_ratio_, RAYWHITE);
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
    DrawTextCentered(resolution.c_str(), 400 * w_ratio_, 120 * h_ratio_, 20 * h_ratio_, RAYWHITE);

    if (IsKeyReleased(KEY_ESCAPE)) {
      scene_ = SCENE_MAIN_MENU;
      selection_ = 0;
      return;
    }

    int old_horiz = horizontal_resolution_;
    int old_vert = vertical_resolution_;

    horizontal_resolution_ = AVAILABLE_RESOLUTIONS[selection_].first;
    vertical_resolution_ = AVAILABLE_RESOLUTIONS[selection_].second;
    w_ratio_ =  horizontal_resolution_ / arena_width;
    h_ratio_ = vertical_resolution_ / arena_height;

    if (old_horiz != horizontal_resolution_ ||
        old_vert != vertical_resolution_) {
      SetWindowSize(horizontal_resolution_, vertical_resolution_);
    }

    handle_menu_movement(AVAILABLE_RESOLUTIONS.size() - 1);
  }

  void room_selection() {
    DrawTextCentered(
        "Press R to refresh room list or press C to make a new room", 400 * w_ratio_, 20 * h_ratio_, 20 * h_ratio_, RAYWHITE);
    DrawTextCentered("Rooms:", 400 * w_ratio_, 40 * h_ratio_, 20 * h_ratio_, RAYWHITE);

    int line_start = 60;
    for (int i = 0; i < client_.rooms.size(); i++) {
      if (i == selection_) {
        std::string text = "< " + std::to_string(client_.rooms[i]) + " >";
        DrawText(text.c_str(), 400 * w_ratio_, line_start * h_ratio_, 20 * h_ratio_, RAYWHITE);
      } else {
        DrawText(std::to_string(client_.rooms[i]).c_str(), 400 * w_ratio_, line_start * h_ratio_, 20 * h_ratio_,
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
    DrawTextCentered(room_id.c_str(), 400 * w_ratio_, 100 * h_ratio_, 20 * h_ratio_, LIGHTGRAY);
    DrawTextCentered(room_members.c_str(), 400 * w_ratio_, 120 * h_ratio_, 20 * h_ratio_, LIGHTGRAY);
  }

  void play_game() {
    time_ = client_.game_state->time;
    InputMessage input = getInput(time_);
    client_.sendInput(input);

    updatePlayerState(*client_.game_state, input, delta_time_,
                      client_.player_index);
    updateGameState(*client_.game_state, delta_time_);
    client_.saveFrame(input);
    drawGameState(*client_.game_state, horizontal_resolution_ / arena_width, vertical_resolution_ / arena_height);
  }
};

int main() {
  Game game;
  game.start();
  game.run();
  return 0;
}
