#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <string>
#include <strstream>
#include <thread>
#include <unordered_map>

#include "game_state.hpp"

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::chrono::system_clock;

constexpr uint16_t PORT = 25565;
constexpr uint32_t CLIENT_RUNWAY = 6;

class Room {
public:
  std::mutex lock;
  RoomState room_state;
  GameState game_state;
  std::array<std::optional<HSteamNetConnection>, PLAYERS_PER_ROOM> players;
  std::function<void()> propogate_state_callback;
  uint32_t should_ping_counter = 0;

  std::optional<size_t> playerIndexOfConnection(HSteamNetConnection conn) {
    for (int i = 0; i < PLAYERS_PER_ROOM; i++) {
      if (this->players[i] == conn) {
        return i;
      }
    }
    return std::nullopt;
  }

  void startMatch() {
    resetGameState(game_state);
    room_state.state = RS_PLAYING;
    game_tick_thread_ = std::thread(&Room::gameLogicThread, this);
  }

  void endMatch() {
    {
      std::scoped_lock l(lock);
      room_state.state = RS_WAITING;
      message_queue_.clear();
    }
    game_tick_thread_.join();
  }

  void feedInput(InputMessage input, int player_index) {
    std::scoped_lock l(lock);
    message_queue_.push_back(std::make_pair(input, player_index));
  }

private:
  // game logic thread
  std::list<std::pair<InputMessage, int>>
      message_queue_; // pair (input, player_idx)
  std::thread game_tick_thread_;

  bool areClientsAhead(std::array<bool, PLAYERS_PER_ROOM> &ready_list) {
    {
      std::scoped_lock l(lock);
      for (const auto &pair : message_queue_) {
        if (pair.first.tick >= CLIENT_RUNWAY) {
          ready_list[pair.second] = true;
        }
      }
    }

    return std::all_of(std::begin(ready_list), std::end(ready_list),
                       [](bool i) { return i; });
  }

  void gameLogicThread() {
    // wait for clients to get ahead before starting the game loop
    std::array<bool, PLAYERS_PER_ROOM> ready; // TODO: bitset
    ready.fill(false);
    while (!areClientsAhead(ready)) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds((int)(DESIRED_TICK_LENGTH * 1000.0)));
    }

    double delta_time = 0.0;
    double time = 0.0;
    uint32_t tick = 0;

    auto frame_start = steady_clock::now();
    double time_accumulator =
        DESIRED_TICK_LENGTH; // this forces a tick on the first frame

    // game loop
    while (true) {
      double delta_time =
          static_cast<duration<double>>(steady_clock::now() - frame_start)
              .count();
      frame_start = steady_clock::now();

      // lock room state
      {
        std::scoped_lock l(lock);
        // should we exit?
        if (room_state.state != RS_PLAYING) {
          break;
        }

        time_accumulator += delta_time;

        // move game logic forward in equally sized ticks
        while (time_accumulator >= DESIRED_TICK_LENGTH) {
          time_accumulator -= DESIRED_TICK_LENGTH;

          // consume inputs that correspond to this tick
          auto input_iterator = message_queue_.begin();
          while (input_iterator != message_queue_.end()) {
            if (input_iterator->first.tick == tick) {
              updatePlayerState(game_state, input_iterator->first,
                                DESIRED_TICK_LENGTH, input_iterator->second);
              input_iterator = message_queue_.erase(input_iterator);
            } else if (input_iterator->first.tick < tick) {
              std::cout << "client is behind!!! " << input_iterator->first.tick
                        << " vs " << tick << std::endl;
              message_queue_.clear();
            } else {
              input_iterator++;
            }
          }

          updateGameState(game_state, DESIRED_TICK_LENGTH);
          game_state.tick = tick;
          tick++;
        }
      }
      propogate_state_callback();

      // sleep s.t we tick at the correct rate
      // TODO: just record the delta_time in ms to start with...
      double elapsed_time =
          static_cast<duration<float>>(steady_clock::now() - frame_start)
              .count();
      float sleep_time = DESIRED_TICK_LENGTH - elapsed_time;
      if (sleep_time > 0.0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds((int)(sleep_time * 1000.0)));
      }
    }
  }
};

class Server {
public:
  Server() = default;

  void start() {
    // init rooms
    for (int i = 0; i < MAX_ROOMS; i++) {
      rooms_[i].room_state.current_room = i;
      rooms_[i].propogate_state_callback =
          std::bind(&Server::propogateGameState, this, i);
    }

    // init connection lib
    SteamDatagramErrMsg error_msg;
    if (!GameNetworkingSockets_Init(nullptr, error_msg)) {
      std::cout << "ERROR: Failed to Intialize Game Networking Sockets: "
                << error_msg << std::endl;
      exit(1);
    }
    network_interface_ = SteamNetworkingSockets();

    // start listening to connections & setup callbacks
    SteamNetworkingIPAddr local_address;
    local_address.Clear();
    local_address.m_port = PORT;
    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               (void *)connectionStatusCallback);
    socket_ = network_interface_->CreateListenSocketIP(local_address, 1, &opt);
    if (socket_ == k_HSteamListenSocket_Invalid) {
      std::cout << "ERROR: Could not listen on port: " << PORT << std::endl;
      exit(1);
    }
    poll_group_ = network_interface_->CreatePollGroup();
    if (poll_group_ == k_HSteamNetPollGroup_Invalid) {
      std::cout << "ERROR: Could not listen on port: " << PORT << std::endl;
      exit(1);
    }

    // loop through callbacks at desired tick rate
    while (!should_quit_) {
      handleMessages();
      runCallbacks();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  ~Server() {
    // loop through all connections and close them cleanly
    for (const auto &it : connected_clients_) {
      network_interface_->CloseConnection(it.first, 0, "Server Shutdown", true);
    }
    connected_clients_.clear();

    // shut down the network interface
    network_interface_->CloseListenSocket(socket_);
    socket_ = k_HSteamListenSocket_Invalid;
    network_interface_->DestroyPollGroup(poll_group_);
    poll_group_ = k_HSteamNetPollGroup_Invalid;
  }

private:
  // singleton-ish structure here s.t we can use C API to call callbacks
  // not very cool!
  static Server *current_callback_instance_;
  void runCallbacks() {
    current_callback_instance_ = this;
    network_interface_->RunCallbacks();
  }

  static void
  connectionStatusCallback(SteamNetConnectionStatusChangedCallback_t *info) {
    current_callback_instance_->onConnectionStatusChanged(info);
  }

  void handleMessages() {
    // go through all messages one at a time
    while (true) {
      ISteamNetworkingMessage *incoming_msg = nullptr;
      int num_msgs = network_interface_->ReceiveMessagesOnPollGroup(
          poll_group_, &incoming_msg, 1);

      if (num_msgs == 0) {
        break;
      }

      if (num_msgs < 0) {
        std::cout << "WARNING: we failed to get a message from the poll group"
                  << std::endl;
      }

      // lookup connection and make sure its registered
      if (connected_clients_.find(incoming_msg->m_conn) !=
          connected_clients_.end()) {
        // deserialize the client request
        MessageTag msg_tag;
        std::stringstream ss(std::ios::binary | std::ios_base::app |
                             std::ios_base::in | std::ios_base::out);
        ss.write((char const *)incoming_msg->m_pData, incoming_msg->m_cbSize);
        cereal::BinaryInputArchive dearchive(ss);
        dearchive(msg_tag);
        if (msg_tag.type == MSG_ROOM_REQUEST) {
          RoomRequest room_request_msg;
          dearchive(room_request_msg);

          if (room_request_msg.command == RR_LIST_ROOMS) {
            LobbyState response;
            for (int i = 0; i < MAX_ROOMS; i++) {
              if (rooms_[i].room_state.num_connected > 0) {
                response.available_rooms.push_back(i);
              }
            }
            sendLobbyState(response, incoming_msg->m_conn);
          } else if (room_request_msg.command == RR_JOIN_ROOM) {
            if (!joinRoom(incoming_msg->m_conn, room_request_msg.desired_room,
                          room_request_msg.nickname)) {
              // send error
              std::cout << "error joining a room\n";
            }
          } else if (room_request_msg.command == RR_MAKE_ROOM) {
            int room_id =
                makeRoom(incoming_msg->m_conn, room_request_msg.nickname);
            if (room_id == -1) {
              // send error
              std::cout << "error making a room\n";
            }
          }
        } else if (msg_tag.type == MSG_CLIENT_INPUT) {
          InputMessage input_msg;
          dearchive(input_msg);

          int room_id = connected_clients_[incoming_msg->m_conn];
          Room &room = rooms_[room_id];
          std::optional<size_t> maybe_player_index =
              room.playerIndexOfConnection(incoming_msg->m_conn);
          if (!maybe_player_index.has_value()) {
            continue;
          }
          size_t player_index = maybe_player_index.value();
          if (room.room_state.state == RS_WAITING) {
            // do nothing
          } else {
            // if not, feed player inputs
            room.feedInput(input_msg, player_index);
          }
        } else if (msg_tag.type == MSG_PING) {
          // get current time
          uint32_t current_time = duration_cast<milliseconds>(
                                      system_clock::now().time_since_epoch())
                                      .count();
          PingMessage ping_msg;
          dearchive(ping_msg);

          // compute round trip in ms
          uint32_t ping = current_time - ping_msg.server_send_time;

          // store it in the room
          int room_id = connected_clients_[incoming_msg->m_conn];
          Room &room = rooms_[room_id];
          std::optional<size_t> maybe_player_index =
              room.playerIndexOfConnection(incoming_msg->m_conn);
          if (!maybe_player_index.has_value()) {
            continue;
          }
          size_t player_index = maybe_player_index.value();
          room.room_state.pings[player_index] = ping;
          propogateRoomState(room_id);
        }
      }
      incoming_msg->Release();
    }
  }

  void
  onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info) {
    switch (info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_Connecting:
      onNewClient(info);
      break;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
      onClientDisconnection(info);
      break;
    default:
      break;
    }
  }

  void onNewClient(SteamNetConnectionStatusChangedCallback_t *info) {
    // accept
    if (network_interface_->AcceptConnection(info->m_hConn) != k_EResultOK) {
      // something went wrong with accepting this connection
      network_interface_->CloseConnection(info->m_hConn, 0, nullptr, false);
      std::cout << "WARNING: we failed to accept a connection" << std::endl;
      return;
    }

    // assign to poll group
    if (network_interface_->SetConnectionPollGroup(
            info->m_hConn, poll_group_) != k_EResultOK) {
      // something went wrong with the poll group
      network_interface_->CloseConnection(info->m_hConn, 0, nullptr, false);
      std::cout << "WARNING: we failed to accept a connection because it could "
                   "not be added to a poll group"
                << std::endl;
      return;
    }

    // store the connection somewhere we can use it
    connected_clients_.emplace(info->m_hConn, -1);
    std::cout << "we got a new connection!" << std::endl;
  }

  void onClientDisconnection(SteamNetConnectionStatusChangedCallback_t *info) {
    // only deal with it if they were previously connected
    if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {

      // delete connection from storage
      if (connected_clients_[info->m_hConn] != -1) {
        leaveRoom(info->m_hConn, connected_clients_[info->m_hConn]);
      }
      connected_clients_.erase(info->m_hConn);

      // close out connection
      network_interface_->CloseConnection(info->m_hConn, 0, nullptr, false);
      std::cout << "client disconnected" << std::endl;
    }
  }

  void sendLobbyState(LobbyState &lobby_state, HSteamNetConnection connection) {
    MessageTag msg_tag;
    msg_tag.type = MSG_LOBBY_STATE;
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg_tag);
      archive(lobby_state);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Reliable, nullptr);
  }

  void sendRoomState(RoomState &room_state, HSteamNetConnection connection) {
    MessageTag msg_tag;
    msg_tag.type = MSG_ROOM_STATE;
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg_tag);
      archive(room_state);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Reliable, nullptr);
  }

  void sendPing(HSteamNetConnection connection) {
    MessageTag msg_tag;
    PingMessage msg;
    msg.server_send_time =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count();
    msg_tag.type = MSG_PING;
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg_tag);
      archive(msg);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Reliable, nullptr);
  }

  void sendGameState(GameState &game_state, HSteamNetConnection connection) {
    MessageTag msg_tag;
    msg_tag.type = MSG_GAME_STATE;
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg_tag);
      archive(game_state);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Unreliable, nullptr);
  }

  bool joinRoom(HSteamNetConnection player, int room_id,
                const std::string &nickname) {
    Room &room = rooms_[room_id];
    if (room.room_state.num_connected >= PLAYERS_PER_ROOM) {
      return false;
    }

    room.room_state.num_connected++;
    connected_clients_[player] = room_id;

    if (room.room_state.num_connected == PLAYERS_PER_ROOM) {
      room.startMatch();
    }

    // emplace player into the first empty slot
    for (int i = 0; i < PLAYERS_PER_ROOM; i++) {
      if (!room.players[i].has_value()) {
        room.players[i] = player;
        room.room_state.nicknames[i] = nickname;
        break;
      }
    }
    propogateRoomState(room_id);
    return true;
  }

  int makeRoom(HSteamNetConnection player, const std::string &nickname) {
    // find first empty room slot and join it
    for (int i = 0; i < MAX_ROOMS; i++) {
      if (rooms_[i].room_state.num_connected == 0 &&
          joinRoom(player, i, nickname) == true) {
        propogateRoomState(i);
        return i;
      }
    }
    return -1;
  }

  bool leaveRoom(HSteamNetConnection player, int room_id) {
    Room &room = rooms_[room_id];
    for (int i = 0; i < PLAYERS_PER_ROOM; i++) {
      if (room.players[i] == player) {
        room.players[i] = std::nullopt;
        connected_clients_[player] = -1;
        room.room_state.num_connected--;
        if (room.room_state.num_connected != PLAYERS_PER_ROOM &&
            room.room_state.state == RS_PLAYING) {
          room.endMatch();
        }
        propogateRoomState(room_id);
        return true;
      }
    }

    return false;
  }

  void propogateRoomState(int room_id) {
    RoomState msg;
    {
      std::scoped_lock lock(rooms_[room_id].lock);
      msg = rooms_[room_id].room_state;
    }

    for (int i = 0; i < PLAYERS_PER_ROOM; i++) {
      if (rooms_[room_id].players[i]) {
        msg.player_index = i;
        sendRoomState(msg, rooms_[room_id].players[i].value());
      }
    }
  }

  void propogateGameState(int room_id) {
    GameState msg;
    bool should_ping = false;
    {
      std::scoped_lock lock(rooms_[room_id].lock);
      msg = rooms_[room_id].game_state;
      should_ping = rooms_[room_id].should_ping_counter++ %
                        static_cast<uint32_t>(TICK_RATE * 2) ==
                    0;
    }

    for (int i = 0; i < PLAYERS_PER_ROOM; i++) {
      if (rooms_[room_id].players[i]) {
        sendGameState(msg, rooms_[room_id].players[i].value());
        if (should_ping) {
          sendPing(rooms_[room_id].players[i].value());
        }
      }
    }
  }

  ISteamNetworkingSockets *network_interface_ = nullptr;
  HSteamListenSocket socket_;
  HSteamNetPollGroup poll_group_;
  // maping of player network id -> room index
  std::unordered_map<HSteamNetConnection, int> connected_clients_;
  std::array<Room, MAX_ROOMS> rooms_;
  bool should_quit_ = false;
};
Server *Server::current_callback_instance_ = nullptr;

int main() {
  Server server;
  std::cout << "Spinning Server..." << std::endl;
  server.start();
  return 0;
}
