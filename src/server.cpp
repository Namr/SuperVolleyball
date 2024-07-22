#include <iomanip>
#include <iostream>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <string>
#include <strstream>
#include <thread>
#include <unordered_map>

#include "network_signals.hpp"

constexpr uint16_t PORT = 25565;

struct Room {
  GameState game_state;
  std::vector<uint16_t> players;
};

class Server {
public:
  Server() = default;

  void start() {
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
        ServerNetworkMessage response; // allocate our response

        // if not in room, respond to room requests
        if (connected_clients_[incoming_msg->m_conn] == nullptr) {
          switch (msg.room_request.command) {
          case RR_LIST_ROOMS:
            for (auto it : rooms_) {
              response.rooms.push_back(it.first);
            }
            sendRequest(response, incoming_msg->m_conn);
            break;
          case RR_JOIN_ROOM:
            if (joinRoom(incoming_msg->m_conn, msg.room_request.desired_room)) {
              response.current_room = msg.room_request.desired_room;
            } else {
              // TODO: throw error
            }
            sendRequest(response, incoming_msg->m_conn);
            break;
          case RR_MAKE_ROOM:
            response.current_room = makeRoom();
            if (!joinRoom(incoming_msg->m_conn, response.current_room)) {
              // TODO throw error
            }
            sendRequest(response, incoming_msg->m_conn);
            break;
          default:
            break;
          }
        } else {
          // is the room in a waiting mode?
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
    connected_clients_.emplace(info->m_hConn, nullptr);
    std::cout << "we got a new connection!" << std::endl;
  }

  void onClientDisconnection(SteamNetConnectionStatusChangedCallback_t *info) {
    // only deal with it if they were previously connected
    if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
      // delete connection from storage
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

  bool joinRoom(uint16_t player, uint16_t room) {
    if (rooms_.find(room) == rooms_.end()) {
      return false;
    }
    connected_clients_[player] = rooms_[room];
    rooms_[room]->players.push_back(player);
    return true;
  }

  uint16_t makeRoom() {
    rooms_[next_room_id_] = std::make_shared<Room>();
    return next_room_id_++;
  }

  bool leaveRoom(uint16_t player, uint16_t room) {
    if (rooms_.find(room) == rooms_.end()) {
      return false;
    }
    rooms_[room]->players.erase(std::find(rooms_[room]->players.begin(),
                                          rooms_[room]->players.end(), player));
    connected_clients_[player] = nullptr;
    return true;
  }

  ISteamNetworkingSockets *network_interface_ = nullptr;
  HSteamListenSocket socket_;
  HSteamNetPollGroup poll_group_;
  std::unordered_map<HSteamNetConnection, std::shared_ptr<Room>>
      connected_clients_;
  std::unordered_map<uint16_t, std::shared_ptr<Room>> rooms_;
  uint16_t next_room_id_ = 0;
  bool should_quit_ = false;
};
Server *Server::current_callback_instance_ = nullptr;

int main() {
  Server server;
  std::cout << "Spinning Server..." << std::endl;
  server.start();
  return 0;
}
