#pragma once

#ifdef _WIN32
#include <winsock2.h>
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

namespace rconpp {

constexpr int DEFAULT_TIMEOUT = 4;
constexpr int HEADER_SIZE = 14;

enum data_type {
	/**
	 * @brief A command packet.
	 *
	 * @note The server *may* send a `SERVERDATA_RESPONSE_VALUE` packet if the request was successful.
	 * However, The server can also not send a packet back if it only processes the packet and does nothing else.
	 * You should take this into account by either not using the callback or by turning feedback off.
	 */
	SERVERDATA_EXECCOMMAND = 2,

	/**
	 * @brief An authorisation packet.
	 *
	 * The server will send an empty `SERVERDATA_AUTH_RESPONSE` packet if the request was successful.
	 */
	SERVERDATA_AUTH = 3,
};

struct packet {
	unsigned int bytes;
	std::vector<char> data;
	//unsigned char* data;
	bool server_responded;

	//~packet() {
	//	delete[] data;
	//}
};

struct response {
	std::string data;
	bool server_responded;
};

struct queued_request {
	std::string data;
	int32_t id;
	data_type type;
	std::function<void(const response& response)> callback;
};

class rcon {
	const std::string address;
	const unsigned int port;
	const std::string password;
#ifdef _WIN32
	SOCKET sock{INVALID_SOCKET};
#else
	int sock{ 0 };
#endif
	bool connected{false};
	
	std::vector<queued_request> requests_queued{};
    
public:
    
	/**
	 * @brief rcon constuctor. Initiates a connection to an RCON server with the parameters given.
	 *
	 * @note This is a blocking call (done on purpose). It needs to wait to connect to the RCON server before anything else happens.
	 * It will timeout after 4 seconds if it can't connect.
	 */
	rcon(const std::string& addr, const unsigned int _port, const std::string& pass) : address(addr), port(_port), password(pass) {

		std::cout << "Attempting connection to RCON server..." << "\n";

		if (!connect()) {
			std::cout << "RCON is aborting as it failed to initiate." << "\n";
			return;
		}

		std::cout << "Connected successfully! Sending login data..." << "\n";

		// The server will send SERVERDATA_AUTH_RESPONSE once it's happy. If it's not -1, the server will have accepted us!
		response response = send_data_sync(pass, 1, data_type::SERVERDATA_AUTH, true);

		if (!response.server_responded) {
			std::cout << "Login data was incorrect. RCON will now abort." << "\n";
			return;
		}

		std::cout << "Sent login data." << "\n";

		connected = true;

		std::thread queue_runner([this]() {
			while (connected) {
				if (requests_queued.empty()) {
					continue;
				}

				for (queued_request request : requests_queued) {
					// Send data to callback if it's been set.
					if (request.callback)
						request.callback(send_data_sync(request.data, request.id, request.type));
					else
						send_data_sync(request.data, request.id, request.type, false);
				}

				requests_queued.clear();
			}
			});

		queue_runner.detach();
	};

	~rcon() {
		#ifdef _WIN32
		closesocket(sock);
		WSACleanup();
		#else
		close(sock);
		#endif


	}
    
	/**
	 * @brief Send data to the connected RCON server. Requests from this function are added to a queue (`requests_queued`) and are handled by a different thread.
	 *
	 * @param data Data to send to the server.
	 * @param id ID of the packet. Try to make sure you aren't sending multiple requests, at the same time, with the same ID as it may cause issues.
	 * @param type The type of packet to send.
	 *
	 * @warning If you are expecting no response from the server, do NOT use the callback. You will halt the RCON process until the next received message (which will chain).
	 */
	void send_data(const std::string& data, const int32_t id, data_type type, std::function<void(const response& retrieved_data)> callback = {}) {
		requests_queued.emplace_back(queued_request{data, id, type, std::move(callback)});
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
	response send_data_sync(const std::string data, const int32_t id, data_type type, bool feedback = true) {
		if (!connected && type != data_type::SERVERDATA_AUTH) {
			std::cout << "Cannot send data when not connected." << "\n";
			return { "", false };
		}

		unsigned long long packet_len = data.length() + HEADER_SIZE;
		std::vector<char> formed_packet = form_packet(data, id, type);

		if (send(sock, formed_packet.data(), packet_len, 0) < 0) {
			std::cout << "Sending failed!" << "\n";
			return { "", false };
		}

		if (!feedback) {
			// Because we do not want any feedback, we just send no data and say the server didn't respond.
			return { "", false };
		}

		// Server will send a SERVERDATA_RESPONSE_VALUE packet.
		return receive_information(id, type);
	}
    
private:
    
	/**
	 * @brief Connects to RCON using `address`, `port`, and `password`.
	 * Those values are pre-filled when constructing this class.
	 *
	 * @warning This should only ever be called by the constructor.
	 * The constructor calls this function once it has filled in the required data and proceeds to login.
	 */
	bool connect() {
		// Create new TCP socket.
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

#ifdef _WIN32
		if (sock == INVALID_SOCKET) {
			std::cout << "Failed to open socket." << "\n";
			return false;
		}
#else
		if (sock == -1) {
			std::cout << "Failed to open socket." << "\n";
			return false;
		}
#endif

		// Setup port, address, and family.
		struct sockaddr_in server;
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = inet_addr(address.c_str());
		server.sin_port = htons(port);

		// Make it non-blocking.
#ifdef _WIN32
		u_long ul{ 1 };
		ioctlsocket(sock, FIONBIO, &ul);
#else
		fcntl(sock, F_SETFL, O_NONBLOCK);
#endif

		// Set a timeout of 4 seconds.
		struct timeval tv {};
		tv.tv_sec = DEFAULT_TIMEOUT;
		tv.tv_usec = 0;

#ifdef _WIN32
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &DEFAULT_TIMEOUT, sizeof(4));
#else
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

		// Connect to the socket and set the status to our temp status.
		if (::connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
			if (errno != EINPROGRESS) {
				return false;
			}
		}

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		// Create temp status
		int status = select(sock + 1, nullptr, &fds, nullptr, &tv);

		//fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);

		// If status wasn't zero, we successfully connected.
		return status != 0;
	}
    
	/**
	 * @brief Form a valid RCON packet.
	 *
	 * @param packet[] The packet to update information for.
	 * @param data The data to add to the packet.
	 * @param id The ID of the request.
	 * @param type The type of packet.
	 */
	std::vector<char> form_packet(const std::string& data, int32_t id, int32_t type) {
		const char nullbytes[] = { '\x00', '\x00' };
		const int32_t min_size = sizeof(id) + sizeof(type) + sizeof(nullbytes); // 10 bytes.
		const int32_t data_size = static_cast<int32_t>(data.size()) + min_size;

		if (data_size > 4096) {
			std::cout << "This packet is too big to send. Please generate a smaller packet." << "\n";
			return {};
		}

		std::vector<char> temp_packet;

		temp_packet.resize(data_size + sizeof(data_size) + sizeof(id)); /* make sure the vector is big enough to hold all the data */

		std::memcpy(temp_packet.data() + 0, &data_size, sizeof(data_size)); /* copy size into it */
		std::memcpy(temp_packet.data() + sizeof(data_size), &id, sizeof(id)); /* copy id into it */
		std::memcpy(temp_packet.data() + sizeof(data_size), &type, sizeof(type)); /* copy type into it */
		std::memcpy(temp_packet.data() + sizeof(data_size), data.data(), data.size()); /* copy data into it */

		//(memset(packet, '\0', data_size), (void)0);

		// Each part is 4 bytes
		//packet[0] = data_size;
		//packet[4] = id;
		//packet[8] = type;

		//const char* data_chars = data.c_str();

		//for (int i = 0; i < data_size; i++) {
		//	packet[12 + i] = data_chars[i];
		//}

		return temp_packet;
	}
    
	/**
	 * @brief Ask to receive information from the server for a specified ID.
	 *
	 * @param id The ID that we should except the server to return, alongside information.
	 *
	 * @return Data given by the server.
	 */
	response receive_information(int32_t id, data_type type) {
		// Whilst this loop is better than a while loop,
		// it should really just keep going for a certain amount of seconds.
		for (int i = 0; i < 500; i++) {
			packet packet = read_packet();

			if (packet.bytes == 0) {
				if (type != SERVERDATA_AUTH)
					return { "", packet.server_responded };
				else
					continue;
			}

			//unsigned char* buffer = packet.data;

			if (type == SERVERDATA_AUTH) {
				if (byte32_to_int(packet.data) == -1) {
					return { "", false };
				}
				else {
					if (byte32_to_int(packet.data) == id) {
						return { "", true };
					}
				}
			}

			int offset = packet.bytes - HEADER_SIZE + 3;

			if (offset == -1)
				continue;

			std::string part(&packet.data[8], &packet.data[8] + offset);

			if (byte32_to_int(packet.data) == id) {
				return { part, packet.server_responded };
			}
		}
		return { "", false };
	}
    
	/**
	 * @brief Gathers all the packet's content (based on the length returned by `read_packet_length`)
	 */
	packet read_packet() {
		size_t packet_length = read_packet_length();

		/*
		 * If the packet length is -1, the server didn't respond.
		 * If the packet length is 0, the server did respond but said nothing.
		 */
		if (packet_length == -1) {
			return { 0, {}, false };
		}
		else if (packet_length == 0) {
			return { 0, {}, true };
		}

		//auto* buffer = new unsigned char[packet_length] {0};
		std::vector<char> buffer;
		buffer.resize(packet_length);
		//std::string buffer;
		unsigned int bytes = 0;

		do {
			size_t recv_bytes = recv(sock, buffer.data(), packet_length - bytes, 0);
			if (recv_bytes == -1) {
				std::cout << "Did not receive a packet in time. Did the server send a response?" << "\n";
				return { 0, {}, false };
			}

			bytes += recv_bytes;

		} while (bytes < packet_length);

		return { bytes, buffer };
	}
    
	const int read_packet_length() {
		std::vector<char> buffer;
		buffer.resize(4);
		size_t recv_bytes = recv(sock, buffer.data(), 4, 0);
		if (recv_bytes == -1) {
			std::cout << "Did not receive a packet in time. Did the server send a response?" << "\n";
			return -1;
		}
		return byte32_to_int(buffer);
	}

	inline const int byte32_to_int(std::vector<char>& buffer) {
		return static_cast<int>(buffer[0] | buffer[1] << 8 | buffer[2] << 16 | buffer[3] << 24);
	}
};

} // namespace rconpp
