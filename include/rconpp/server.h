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
#include "utilities.h"

namespace rconpp {

struct connected_client {
	struct sockaddr_in sock_info{};
	int socket{0};
	bool connected{false};

	bool authenticated{false};
};

struct server_info {
	std::string address{};
	int port{0};
	std::string password{};
};

struct client_command {
	connected_client client;
	std::string command{};
};

class RCONPP_EXPORT rcon_server {
	server_info serv_info{};

#ifdef _WIN32
	SOCKET sock{INVALID_SOCKET};
#else
	int sock{0};
#endif

	std::thread accept_connections_runner;

public:
	bool online{false};

	std::function<std::string(const client_command& command)> on_command;

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
	rcon_server(const std::string_view addr, const int port, const std::string_view pass);

	~rcon_server();

	/**
	 * @brief Disconnect a client from the server.
	 *
	 * @param client_socket The socket of the client to disconnect.
	 */
	void disconnect_client(const int client_socket);

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
	 * @brief Ask to receive information from the server for a specified ID.
	 *
	 * @param id The ID that we should except the server to return, alongside information.
	 * @param type The type of packet that we should expect.
	 *
	 * @return Data given by the server.
	 */
	response receive_information(int32_t id, data_type type);

	/**
	 * @brief Gathers all the packet's content (based on the length returned by `read_packet_length`)
	 */
	void read_packet(rconpp::connected_client client);

	/**
	 * @brief Reads the first 4 bytes of a packet to get the packet size (not to be mistaken with length).
	 *
	 * @return The size (not length) of the packet.
	 */
	int read_packet_size(const rconpp::connected_client client);
};

} // namespace rconpp