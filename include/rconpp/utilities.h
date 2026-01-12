#pragma once

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <functional>

#include "export.h"
#include <string>
#include <string_view>
#include <vector>
#include <iostream>

namespace rconpp {

constexpr int DEFAULT_TIMEOUT = 4; // In Seconds.
constexpr int MIN_PACKET_SIZE = 10;
constexpr int MIN_PACKET_LENGTH = 14;
constexpr int MAX_RETRIES_TO_RECEIVE_INFO = 500;
constexpr int HEARTBEAT_TIME = 30;

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif


enum data_type {
	/**
	 * @brief A response to a SERVERDATA_EXECOMMAND packet.
	 *
	 * @note You should **ALWAYS** send this packet upon an SERVERDATA_EXECCOMMAND packet.
	 * Whilst rcon++ will handle no response safely, other libraries may not.
	 */
	SERVERDATA_RESPONSE_VALUE = 0,

	/**
	 * @brief A command packet.
	 *
	 * @note The server *may* send a `SERVERDATA_RESPONSE_VALUE` packet if the request was successful.
	 * However, The server can (but shouldn't) choose to not send a packet back if it only processes the packet and does nothing else.
	 * You should take this into account by either not using the callback or by turning feedback off.
	 */
	SERVERDATA_EXECCOMMAND = 2,

	/**
	 * @brief A response to an authorisation packet.
	 *
	 * @warning If you are wishing to send this packet, you should only **EVER** send this as an empty packet.
	 */
	SERVERDATA_AUTH_RESPONSE = 2,

	/**
	 * @brief An authorisation packet.
	 *
	 * The server will send an empty `SERVERDATA_AUTH_RESPONSE` packet if the request was successful.
	 */
	SERVERDATA_AUTH = 3,
};

struct packet {
	int length{-1};
	int size{-1};
	std::vector<char> data{};
	bool server_responded{false};
};

struct response {
	std::string data{};
	bool server_responded{false};
};

enum error_type {
	DISCONNECTED = 0,
	BAD_FD = 1,
	SHUTTING_DOWN = 2,
};

/**
 * @brief Form a valid RCON packet.
 *
 * @param data The data to add to the packet.
 * @param id The ID of the request.
 * @param type The type of packet.
 *
 * @returns The packet data (as an array of chars) to send to a server.
 */
RCONPP_EXPORT packet form_packet(const std::string_view data, int32_t id, int32_t type);

/**
 * @brief Turn the first 4 bytes of a buffer (which ideally a 32 bit int) into an integer.
 *
 * @param buffer The bytes to turn into an integer.
 *
 * @return The value of the 4 bytes.
 */
RCONPP_EXPORT int bit32_to_int(const std::vector<char>& buffer);

/**
 * @brief Turn the second lot of 4 bytes (bytes 4-7) of a buffer (which ideally a 32 bit int) into an integer.
 *
 * @param buffer The bytes to turn into an integer.
 *
 * @return The value of the 4 bytes.
 */
RCONPP_EXPORT int type_to_int(const std::vector<char>& buffer);

/**
 * @brief Converts the last error into error_type and reports it.
 */
RCONPP_EXPORT error_type report_get_last_error();

/**
 * @brief Reads the first 4 bytes of a packet to get the packet size (not to be mistaken with length).
 *
 * @return The size (not length) of the packet.
 */
RCONPP_EXPORT int read_packet_size(int socket);

} // namespace rconpp
