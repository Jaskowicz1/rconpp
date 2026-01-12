#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <fcntl.h>
#include <string>
#include <functional>
#include <vector>
#include <iostream>
#include <cstring>
#include <thread>
#include <condition_variable>
#include "utilities.h"

namespace rconpp {

struct connected_client {
	sockaddr_in sock_info{};
	int socket{0};
	bool connected{false};

	bool authenticated{false};

	bool force_disconnect{false};

	time_t last_heartbeat{0};
};

struct client_command {
	connected_client client;
	std::string command{};
};

class RCONPP_EXPORT rcon_server {
	std::string address{};
	int port{0};
	std::string password{};

#ifdef _WIN32
	SOCKET sock{INVALID_SOCKET};
#else
	int sock{0};
#endif

	std::thread accept_connections_runner;
	std::mutex connected_clients_mutex;

public:
	bool online{false};

	std::function<std::string(const client_command& command)> on_command;

	std::function<void(const std::string_view log)> on_log = {};

	std::condition_variable terminating;

	/**
	 * @brief A map of connected clients. The key is their socket to talk to.
	 */
	std::unordered_map<int, connected_client> connected_clients{};

	std::unordered_map<int, std::thread> request_handlers{};

	/**
	 * @brief rcon_server constuctor. Initiates a connection to an RCON server with the parameters given.
	 *
	 * @param addr The IP Address (NOT domain) to connect to.
	 * @param _port The port to connect to.
	 * @param pass The password for the RCON server you are connecting to.
	 *
	 * @note This is a blocking call (done on purpose). It needs to wait to connect to the RCON server before anything else happens.
	 * It will timeout after 4 seconds if it can't connect.
	 */
	rcon_server(const std::string_view addr, const int _port, const std::string_view pass);

	~rcon_server();

	void start(bool return_after);

	/**
	 * @brief Disconnect a client from the server.
	 *
	 * @param client_socket The socket of the client to disconnect.
	 * @param remove_after Should remove client from connected_clients after?
	 */
	void disconnect_client(int client_socket, bool remove_after = true);

private:

	/**
	 * @brief Connects to RCON using `address`, `port`, and `password`.
	 * Those values are pre-filled when constructing this class.
	 *
	 * @warning This should only ever be called by the constructor.
	 * The constructor calls this function once it has filled in the required data and proceeds to login.
	 */
	bool startup_server();

	/**
	 * @brief Gathers all the packet's content (based on the length returned by `read_packet_length`)
	 *
	 * @param client Client to read packet from.
	 */
	void read_packet(connected_client& client);

	/**
	 * @brief Sends a heartbeat to a client.
	 *
	 * @param client Client to send a heartbeat to.
	 *
	 * @returns bool, true is heartbeat was sent, otherwise false.
	 */
	bool send_heartbeat(connected_client& client);
};

} // namespace rconpp
