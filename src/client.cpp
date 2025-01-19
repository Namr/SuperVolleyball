#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <queue>
#include <string>

#include <raylib.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include "game_state.hpp"

using std::chrono::duration;
using std::chrono::seconds;
using std::chrono::steady_clock;

constexpr size_t INPUT_HISTORY_CAPACITY = 300;
constexpr int SCENE_MAIN_MENU = 0;
constexpr int SCENE_ROOM_SELECT = 1;
constexpr int SCENE_SETTINGS = 2;
constexpr int SCENE_SET_NAME = 3;
constexpr uint16_t NICKNAME_MAX_LENGTH = 6;

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
      MessageTag msg_tag;
      std::stringstream ss(std::ios::binary | std::ios_base::app |
                           std::ios_base::in | std::ios_base::out);
      ss.write((char const *)incoming_msg->m_pData, incoming_msg->m_cbSize);
      cereal::BinaryInputArchive dearchive(ss);
      dearchive(msg_tag);
      if (msg_tag.type == MSG_LOBBY_STATE) {
        LobbyState lobby_state_msg;
        dearchive(lobby_state_msg);
        rooms = std::move(lobby_state_msg.available_rooms);
      } else if (msg_tag.type == MSG_ROOM_STATE) {
        RoomState room_state_msg;
        dearchive(room_state_msg);

        // detect match start to reset game state
        if (room_state_msg.state == RS_PLAYING &&
            room_state->state != room_state_msg.state) {
          resetGameState(game_state);
        }

        room_state = room_state_msg;
      } else if (msg_tag.type == MSG_PING) {
        // just send it right back
        PingMessage ping_msg;
        dearchive(ping_msg);
        sendPing(ping_msg);
      } else if (msg_tag.type == MSG_GAME_STATE) {
        GameState game_state_msg;
        dearchive(game_state_msg);
        // find the time in our history buffer where this recvd state was
        // computed from if that state does not match what we recvd, we force
        // update it and then recompute using future inputs the server
        // presumably has not consumed yet
        if (room_state->state == RS_PLAYING) {
          bool is_recomputing = false;
          bool found_id = false;
          GameState running_gamestate;
          for (std::pair<InputMessage, GameState> &p : input_history) {
            if (is_recomputing) {
              updatePlayerState(running_gamestate, p.first, DESIRED_TICK_LENGTH,
                                room_state->player_index);
              updateGameState(running_gamestate, DESIRED_TICK_LENGTH);
              running_gamestate.tick = p.first.tick;
              p.second = running_gamestate;
            } else {
              // find the timestamp where this recv'd state was computed from
              if (game_state_msg.tick == p.first.tick) {
                found_id = true;
                if (game_state_msg != p.second) {
                  /*
                  std::cout << "disagreement on tick: " << game_state_msg.tick;
                  std::cout << " ( " << game_state_msg.ball.pos.x << ", "
                            << game_state_msg.ball.pos.y << ") vs ";
                  std::cout << " ( " << p.second.ball.pos.x << ", "
                            << p.second.ball.pos.y << ")\n";
                  */
                  is_recomputing = true;
                  p.second = game_state_msg;
                  running_gamestate = game_state_msg;
                  running_gamestate.tick = p.first.tick;
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

          // if we outran our buffer ...?
          // FIXME: this can lead to a desync lol
          if (!found_id) {
            std::cout << "we didn't find id: " << game_state_msg.tick
                      << " and we're on id: " << game_state.tick << std::endl;
          }
        }
      } else {
        std::cout << "WARN: we got an unexpected message type from the server: "
                  << msg_tag.type << std::endl;
      }

      incoming_msg->Release();
    }
  }

  void runCallbacks() {
    current_callback_instance_ = this;
    network_interface_->RunCallbacks();
  }

  void updateRoomList() {
    RoomRequest msg;
    msg.command = RR_LIST_ROOMS;
    sendRoomRequest(msg);
  }

  void joinRoom(uint16_t desired_room) {
    RoomRequest msg;
    msg.command = RR_JOIN_ROOM;
    msg.desired_room = desired_room;
    msg.nickname = nickname;
    sendRoomRequest(msg);
  }

  void makeRoom() {
    RoomRequest msg;
    msg.command = RR_MAKE_ROOM;
    msg.nickname = nickname;
    sendRoomRequest(msg);
  }

  void saveFrame(InputMessage input) {
    input_history.push_back(std::make_pair(input, game_state));
    if (input_history.size() > INPUT_HISTORY_CAPACITY) {
      input_history.pop_front();
    }
  }

  void sendInput(const InputMessage &input) {
    MessageTag msg_tag;
    msg_tag.type = MSG_CLIENT_INPUT;
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg_tag);
      archive(input);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection_, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Unreliable, nullptr);
  }

  void sendPing(const PingMessage &ping) {
    MessageTag msg_tag;
    msg_tag.type = MSG_PING;
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg_tag);
      archive(ping);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection_, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Reliable, nullptr);
  }

  void sendRoomRequest(RoomRequest &room_request) {
    MessageTag msg_tag;
    msg_tag.type = MSG_ROOM_REQUEST;
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg_tag);
      archive(room_request);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection_, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Reliable, nullptr);
  }

  std::vector<int> rooms;
  bool connected = false;
  std::optional<RoomState> room_state;
  GameState game_state;
  std::string nickname; // FIXME why are there two of these... this is dumb
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

InputMessage getInput(uint32_t tick) {
  InputMessage i;
  i.tick = tick;
  i.up = IsKeyDown(KEY_UP);
  i.down = IsKeyDown(KEY_DOWN);
  i.left = IsKeyDown(KEY_LEFT);
  i.right = IsKeyDown(KEY_RIGHT);
  return i;
}

void drawGameState(const GameState &state, double w_ratio, double h_ratio) {
  // game pieces
  DrawRectangle((int)state.p1.pos.x * w_ratio, (int)state.p1.pos.y * h_ratio,
                (int)paddle_width * w_ratio, (int)paddle_height * h_ratio,
                WHITE);
  DrawRectangle((int)state.p2.pos.x * w_ratio, (int)state.p2.pos.y * h_ratio,
                (int)paddle_width * w_ratio, (int)paddle_height * h_ratio,
                WHITE);
  DrawRectangle((int)state.p3.pos.x * w_ratio, (int)state.p3.pos.y * h_ratio,
                (int)paddle_width * w_ratio, (int)paddle_height * h_ratio,
                WHITE);
  DrawRectangle((int)state.p4.pos.x * w_ratio, (int)state.p4.pos.y * h_ratio,
                (int)paddle_width * w_ratio, (int)paddle_height * h_ratio,
                WHITE);

  DrawRectangle((int)(state.ball.pos.x - ball_radius) * w_ratio,
                (int)(state.ball.pos.y - ball_radius) * h_ratio,
                (int)ball_radius * 2 * w_ratio, (int)ball_radius * 2 * h_ratio,
                WHITE);

  // divider lines
  constexpr int num_lines = 6;
  const int space_between_divider = 30 * h_ratio;
  const int rect_width = center_line_width * w_ratio;
  const int rect_spacing = ((arena_height * h_ratio) / num_lines);
  const int rect_height = rect_spacing - space_between_divider;
  for (int i = 0; i < num_lines; i++) {
    DrawRectangle((int)(arena_width / 2.0) * w_ratio,
                  (rect_spacing * i + (space_between_divider / 2.0)),
                  rect_width, (rect_height), WHITE);
  }

  // score
  char p1_score[20];
  char p2_score[20];
  snprintf(p1_score, 20, "%d", state.p1_score);
  snprintf(p2_score, 20, "%d", state.p2_score);

  DrawText(p1_score, (arena_width / 5) * w_ratio, 50 * h_ratio, 80 * h_ratio,
           WHITE);
  DrawText(p2_score, 4 * (arena_width / 5) * w_ratio, 50 * h_ratio,
           80 * h_ratio, WHITE);
}

void drawRoomState(const RoomState &state, double w_ratio, double h_ratio) {
  char p1[40];
  char p2[40];
  snprintf(p1, 20, "%s %d ms", state.nicknames[0].c_str(), state.pings[0]);
  snprintf(p2, 20, "%s %d ms", state.nicknames[1].c_str(), state.pings[1]);

  DrawTextCentered(p1, (arena_width / 20) * w_ratio, 20 * h_ratio, 10 * h_ratio,
                   WHITE);
  DrawTextCentered(p2, 19 * (arena_width / 20) * w_ratio, 20 * h_ratio,
                   10 * h_ratio, WHITE);
}

class Game {
public:
  Game() = default;
  ~Game() { CloseWindow(); }

  void start() {
    client_.start();
    InitWindow(horizontal_resolution_, vertical_resolution_, "SuperVolleyball");
    SetTargetFPS(144);
    SetExitKey(0);
    client_.updateRoomList();
  }

  void run() {
    auto frame_start = steady_clock::now();
    while (!WindowShouldClose()) {
      delta_time_ =
          static_cast<duration<float>>(steady_clock::now() - frame_start)
              .count();
      frame_start = steady_clock::now();

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
        case SCENE_SET_NAME:
          set_name();
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
            // reset any possible left over state
            tick_ = 0;
            time_accumulator_ = 0;
            wait_for_match_start();
          } else {
            play_game();
          }
        }
      }
      EndDrawing();
    }
  }

private:
  GameState previous_gamestate_;
  Client client_;
  size_t selection_ = 0;
  std::string nickname_;

  uint32_t tick_ = 0;
  double delta_time_ = 0.0;
  double time_accumulator_ = 0.0;

  int scene_ = SCENE_MAIN_MENU;
  int horizontal_resolution_ = 800;
  int vertical_resolution_ = 450;
  double w_ratio_ = horizontal_resolution_ / arena_width;
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
    DrawTextCentered("Welcome to SuperVolleyball!", 400 * w_ratio_,
                     120 * h_ratio_, 20 * h_ratio_, RAYWHITE);

    if (client_.connected) {
      if (selection_ == 0) {
        DrawTextCentered("< Play >", 400 * w_ratio_, 160 * h_ratio_,
                         20 * h_ratio_, RAYWHITE);
      } else {
        DrawTextCentered("Play", 400 * w_ratio_, 160 * h_ratio_, 20 * h_ratio_,
                         RAYWHITE);
      }
    } else {
      if (selection_ == 0) {
        DrawTextCentered("< Connecting to server... >", 400 * w_ratio_,
                         160 * h_ratio_, 20 * h_ratio_, RAYWHITE);
      } else {
        DrawTextCentered("Connecting to server...", 400 * w_ratio_,
                         160 * h_ratio_, 20 * h_ratio_, RAYWHITE);
      }
    }

    if (selection_ == 1) {
      DrawTextCentered("< Settings >", 400 * w_ratio_, 200 * h_ratio_,
                       20 * h_ratio_, RAYWHITE);
    } else {
      DrawTextCentered("Settings", 400 * w_ratio_, 200 * h_ratio_,
                       20 * h_ratio_, RAYWHITE);
    }

    if (IsKeyReleased(KEY_ENTER)) {
      if (selection_ == 0 && client_.connected) {
        scene_ = SCENE_SET_NAME;
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
    DrawTextCentered(resolution.c_str(), 400 * w_ratio_, 120 * h_ratio_,
                     20 * h_ratio_, RAYWHITE);

    if (IsKeyReleased(KEY_ESCAPE)) {
      scene_ = SCENE_MAIN_MENU;
      selection_ = 0;
      return;
    }

    int old_horiz = horizontal_resolution_;
    int old_vert = vertical_resolution_;

    horizontal_resolution_ = AVAILABLE_RESOLUTIONS[selection_].first;
    vertical_resolution_ = AVAILABLE_RESOLUTIONS[selection_].second;
    w_ratio_ = horizontal_resolution_ / arena_width;
    h_ratio_ = vertical_resolution_ / arena_height;

    if (old_horiz != horizontal_resolution_ ||
        old_vert != vertical_resolution_) {
      SetWindowSize(horizontal_resolution_, vertical_resolution_);
    }

    handle_menu_movement(AVAILABLE_RESOLUTIONS.size() - 1);
  }

  void room_selection() {
    DrawTextCentered(
        "Press R to refresh room list or press C to make a new room",
        400 * w_ratio_, 20 * h_ratio_, 20 * h_ratio_, RAYWHITE);
    DrawTextCentered("Rooms:", 400 * w_ratio_, 40 * h_ratio_, 20 * h_ratio_,
                     RAYWHITE);

    int line_start = 60;
    for (int i = 0; i < client_.rooms.size(); i++) {
      if (i == selection_) {
        std::string text = "< " + std::to_string(client_.rooms[i]) + " >";
        DrawText(text.c_str(), 400 * w_ratio_, line_start * h_ratio_,
                 20 * h_ratio_, RAYWHITE);
      } else {
        DrawText(std::to_string(client_.rooms[i]).c_str(), 400 * w_ratio_,
                 line_start * h_ratio_, 20 * h_ratio_, RAYWHITE);
      }
      line_start += 20;
    }

    if (IsKeyReleased(KEY_C)) {
      client_.makeRoom();
    } else if (IsKeyReleased(KEY_R)) {
      client_.updateRoomList();
    } else if (IsKeyReleased(KEY_ENTER) && !client_.rooms.empty()) {
      client_.joinRoom(client_.rooms[selection_]);
    }
    handle_menu_movement(client_.rooms.size() - 1);
  }

  void set_name() {
    DrawTextCentered("Enter a username", 400 * w_ratio_, 120 * h_ratio_,
                     20 * h_ratio_, RAYWHITE);
    DrawTextCentered("Press enter to confirm", 400 * w_ratio_, 140 * h_ratio_,
                     20 * h_ratio_, RAYWHITE);

    // capture input
    char c;
    while ((c = GetCharPressed()) != 0 &&
           nickname_.length() < NICKNAME_MAX_LENGTH) {
      nickname_.push_back(c);
    }
    if (IsKeyReleased(KEY_BACKSPACE) && !nickname_.empty()) {
      nickname_.pop_back();
    }

    // print str
    DrawTextCentered(nickname_.c_str(), 400 * w_ratio_, 160 * h_ratio_,
                     20 * h_ratio_, RAYWHITE);

    if (IsKeyReleased(KEY_ENTER) && !nickname_.empty()) {
      scene_ = SCENE_ROOM_SELECT;
      selection_ = 0;
      client_.nickname = nickname_;
    }
  }

  void wait_for_match_start() {
    std::string room_id =
        "you are in room: " + std::to_string(client_.room_state->current_room);
    std::string room_members =
        "there are " + std::to_string(client_.room_state->num_connected) +
        " players here";
    DrawTextCentered(room_id.c_str(), 400 * w_ratio_, 100 * h_ratio_,
                     20 * h_ratio_, LIGHTGRAY);
    DrawTextCentered(room_members.c_str(), 400 * w_ratio_, 120 * h_ratio_,
                     20 * h_ratio_, LIGHTGRAY);

    if (client_.room_state) {
      RoomState &room = *client_.room_state;
      for (int i = 0; i < PLAYERS_PER_ROOM; i++) {
        if (!room.nicknames[i].empty()) {
          char p[40];
          snprintf(p, 20, "%s %d ms", room.nicknames[i].c_str(), room.pings[i]);
          DrawTextCentered(p, 400 * w_ratio_, (160 + i * 20) * h_ratio_,
                           20 * h_ratio_, LIGHTGRAY);
        }
      }
    }
  }

  void play_game() {
    time_accumulator_ += delta_time_;
    while (time_accumulator_ >= DESIRED_TICK_LENGTH) {
      time_accumulator_ -= DESIRED_TICK_LENGTH;
      // store previous
      previous_gamestate_ = client_.game_state;

      // input handling
      InputMessage input = getInput(tick_);
      client_.sendInput(input);

      // game update
      updatePlayerState(client_.game_state, input, DESIRED_TICK_LENGTH,
                        client_.room_state->player_index);
      updateGameState(client_.game_state, DESIRED_TICK_LENGTH);
      client_.game_state.tick = tick_;
      client_.saveFrame(input);
      tick_++;
    }

    // interpolate before drawing
    double a = time_accumulator_ / DESIRED_TICK_LENGTH;
    GameState state = interpolate(previous_gamestate_, client_.game_state, a);
    drawGameState(state, horizontal_resolution_ / arena_width,
                  vertical_resolution_ / arena_height);
    drawRoomState(*client_.room_state, horizontal_resolution_ / arena_width,
                  vertical_resolution_ / arena_height);
  }
};

int main() {
  Game game;
  game.start();
  game.run();
  return 0;
}
