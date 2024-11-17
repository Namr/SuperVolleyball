#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <string>
#include <strstream>
#include <thread>
#include <unordered_map>

#include "network_signals.hpp"

using std::chrono::duration;
using std::chrono::seconds;
using std::chrono::steady_clock;

constexpr uint16_t PORT = 25565;
constexpr size_t MAX_ROOMS = 16;
constexpr size_t PLAYERS_PER_ROOM = 2;
constexpr float TICK_RATE = 65.0;
constexpr float DESIRED_FRAME_LENGTH = 1.0 / TICK_RATE;

class Room {
public:
  std::mutex lock;
  RoomState room_state;
  GameState game_state;
  std::array<std::optional<HSteamNetConnection>, PLAYERS_PER_ROOM> players;

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
    }
    game_tick_thread_.join();
  }

  void feedInput(InputMessage input, int player_index) {
    std::scoped_lock l(lock);
    message_queue_.push_back(std::make_pair(input, player_index));
  }

private:
  // game logic thread
  std::deque<std::pair<InputMessage, int>>
      message_queue_; // pair (input, player_idx)
  std::thread game_tick_thread_;

  void gameLogicThread() {
    double delta_time = 0.0;
    double time = 0.0;

    while (true) {
      auto frame_start = steady_clock::now();

      // lock room state
      {
        std::scoped_lock l(lock);
        // should we exit?
        if (room_state.state != RS_PLAYING) {
          break;
        }

        // consume inputs
        while (!message_queue_.empty()) {
          std::pair<InputMessage, int> &in = message_queue_.front();
          double player_delta = time - in.first.time;
          updatePlayerState(game_state, in.first, player_delta, in.second);
          message_queue_.pop_front();
        }

        // move game logic forward
        updateGameState(game_state, delta_time);
        game_state.time = time;
      }

      // sleep s.t we tick at the correct rate
      // TODO: just record the delta_time in ms to start with...
      delta_time =
          static_cast<duration<float>>(steady_clock::now() - frame_start)
              .count();
      float sleep_time = DESIRED_FRAME_LENGTH - delta_time;
      if (sleep_time > 0.0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds((int)(sleep_time * 1000.0)));
        delta_time =
            static_cast<duration<float>>(steady_clock::now() - frame_start)
                .count();
      }
      time += delta_time;
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
      // TODO: update game state
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
        ClientNetworkMessage msg;
        {
          std::stringstream ss(std::ios::binary | std::ios_base::app |
                               std::ios_base::in | std::ios_base::out);
          ss.write((char const *)incoming_msg->m_pData, incoming_msg->m_cbSize);
          cereal::BinaryInputArchive dearchive(ss);
          dearchive(msg);
        }

        // if not in room, respond to room requests
        if (connected_clients_[incoming_msg->m_conn] == -1) {
          ServerNetworkMessage response;
          if (msg.room_request.command == RR_LIST_ROOMS) {
            for (int i = 0; i < MAX_ROOMS; i++) {
              if (rooms_[i].room_state.num_connected > 0) {
                response.available_rooms.push_back(i);
              }
            }
            sendRequest(response, incoming_msg->m_conn);
          } else if (msg.room_request.command == RR_JOIN_ROOM) {
            if (!joinRoom(incoming_msg->m_conn,
                          msg.room_request.desired_room)) {
              // send error
              std::cout << "error joining a room\n";
            }
          } else if (msg.room_request.command == RR_MAKE_ROOM) {
            int room_id = makeRoom(incoming_msg->m_conn);
            if (room_id == -1) {
              // send error
              std::cout << "error making a room\n";
            }
          }
        } else {
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
            for (const InputMessage &i : msg.inputs) {
              room.feedInput(i, player_index);
            }
            propogateRoomState(room_id);
          }
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

  void sendRequest(ServerNetworkMessage &msg, HSteamNetConnection connection) {
    std::ostringstream response_stream(std::ios::binary | std::ios_base::app |
                                       std::ios_base::in | std::ios_base::out);
    {
      cereal::BinaryOutputArchive archive(response_stream);
      archive(msg);
    }
    std::string tmp_str = response_stream.str();
    network_interface_->SendMessageToConnection(
        connection, tmp_str.c_str(), tmp_str.size(),
        k_nSteamNetworkingSend_Reliable, nullptr);
  }

  bool joinRoom(HSteamNetConnection player, int room_id) {
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
        break;
      }
    }
    propogateRoomState(room_id);
    return true;
  }

  int makeRoom(HSteamNetConnection player) {
    // find first empty room slot and join it
    for (int i = 0; i < MAX_ROOMS; i++) {
      if (rooms_[i].room_state.num_connected == 0 &&
          joinRoom(player, i) == true) {
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
    ServerNetworkMessage msg;
    {
      std::scoped_lock lock(rooms_[room_id].lock);
      msg.room_state = rooms_[room_id].room_state;
      msg.game_state = rooms_[room_id].game_state;
    }

    for (int i = 0; i < PLAYERS_PER_ROOM; i++) {
      if (rooms_[room_id].players[i]) {
        msg.player_number = i;
        sendRequest(msg, rooms_[room_id].players[i].value());
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
