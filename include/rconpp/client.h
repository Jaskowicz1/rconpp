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
#include <atomic>
#include <thread>
#include <condition_variable>
#include "utilities.h"

namespace rconpp {

struct queued_request {
	std::string data{};
	int32_t id{0};
	data_type type{data_type::SERVERDATA_AUTH};
	std::function<void(const response& response)> callback;
};

class RCONPP_EXPORT rcon_client {
	const std::string address{};
	const int port{0};
	const std::string password{};
#ifdef _WIN32
	SOCKET sock{INVALID_SOCKET};
#else
	int sock{0};
#endif

	std::vector<queued_request> requests_queued{};

	std::thread queue_runner;

public:
	std::atomic<bool> connected{false};

	std::function<void(const std::string_view& log)> on_log;

	std::condition_variable terminating;

	/**
	 * @brief rcon_client constuctor.
	 *
	 * @param addr The IP Address (NOT domain) to connect to.
	 * @param _port The port to connect to.
	 * @param pass The password for the RCON server you are connecting to.
	 *
	 * @note This is a blocking call (done on purpose). It needs to wait to connect to the RCON server before anything else happens.
	 * It will timeout after 4 seconds if it can't connect.
	 */
	rcon_client(const std::string_view addr, const int _port, const std::string_view pass);

	~rcon_client();

	void start(bool return_after);

	/**
	 * @brief Send data to the connected RCON server. Requests from this function are added to a queue (`requests_queued`) and are handled by a different thread.
	 *
	 * @param data Data to send to the server.
	 * @param id ID of the packet. Try to make sure you aren't sending multiple requests, at the same time, with the same ID as it may cause issues.
	 * @param type The type of packet to send.
	 * @param callback The callback function that will fire when the data is returned.
	 *
	 * @warning If you are expecting no response from the server, do NOT use the callback. You will halt the RCON process until the next received message (which will chain).
	 */
	void send_data(const std::string_view data, const int32_t id, data_type type, std::function<void(const response& retrieved_data)> callback = {}) {
		requests_queued.emplace_back(queued_request{ std::string{data}, id, type, std::move(callback) });
	}

	/**
	 * @brief Send data to the connected RCON server.
	 *
	 * @param data Data to send to the server.
	 * @param id ID of the packet. Try to make sure you aren't sending multiple requests, at the same time, with the same ID as it may cause issues.
	 * @param type The type of packet to send.
	 * @param feedback Should the client expect a message back from the server? (optional, default is true).
	 *
	 * @warning If you are expecting no response from the server, set `feedback` to false. Otherwise, you will halt the RCON process for 4 seconds.
	 *
	 * @returns Data given by the server from the request.
	 */
	response send_data_sync(const std::string_view data, const int32_t id, data_type type, bool feedback = true);

private:

	/**
	 * @brief Connects to RCON using `address`, `port`, and `password`.
	 * Those values are pre-filled when constructing this class.
	 *
	 * @warning This should only ever be called by the constructor.
	 * The constructor calls this function once it has filled in the required data and proceeds to login.
	 */
	bool connect_to_server();

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
	 *
	 * @return A packet structure containing the length, size, data, and if server responded.
	 */
	packet read_packet();

	/**
	 * @brief Reads the first 4 bytes of a packet to get the packet size (not to be mistaken with length).
	 *
	 * @return The size (not length) of the packet.
	 */
	int read_packet_size();
};

} // namespace rconpp