#include <mutex>
#include "client.h"
#include "utilities.h"

rconpp::rcon_client::rcon_client(const std::string_view addr, const int _port, const std::string_view pass) : address(addr), port(_port), password(pass) {
}

rconpp::rcon_client::~rcon_client() {
	// Set connected to false, meaning no requests can be attempted during shutdown.
	connected = false;

	terminating.notify_all();

#ifdef _WIN32
	closesocket(sock);
	WSACleanup();
#else
	close(sock);
#endif
	// Join the queue runner (if allowed), meaning we await its end before killing this object, preventing any corruption.
	if (queue_runner.joinable()) {
		queue_runner.join();
	}
}

rconpp::response rconpp::rcon_client::send_data_sync(const std::string_view data, const int32_t id, rconpp::data_type type, bool feedback) {
	if (!connected && type != data_type::SERVERDATA_AUTH) {
		on_log("Cannot send data when not connected.");
		return { "", false };
	}

	packet formed_packet = form_packet(data, id, type);

	if (send(sock, formed_packet.data.data(), formed_packet.length, 0) < 0) {
		on_log("Sending failed!");
		report_error();
		return { "", false };
	}

	if (!feedback) {
		// Because we do not want any feedback, we just send no data and say the server didn't respond.
		return { "", false };
	}

	// Server will send a SERVERDATA_RESPONSE_VALUE packet.
	return receive_information(id, type);
}

bool rconpp::rcon_client::connect_to_server() {
#ifdef _WIN32
	// Initialize Winsock
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		on_log("WSAStartup failed. Error: " + std::to_string(result));
		return false;
	}
#endif

	// Create new TCP socket.
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

#ifdef _WIN32
	if (sock == INVALID_SOCKET) {
#else
	if (sock == -1) {
#endif
		on_log("Failed to open socket.");
		report_error();
		return false;
	}

	// Setup port, address, and family.
	sockaddr_in server{};
	server.sin_family = AF_INET;
#ifdef _WIN32
	#ifdef UNICODE
		InetPton(AF_INET, std::wstring(address.begin(), address.end()).c_str(), &server.sin_addr.s_addr);
	#else
		InetPton(AF_INET, address.c_str(), &server.sin_addr.s_addr);
	#endif
#else
	server.sin_addr.s_addr = inet_addr(address.c_str());
#endif
	server.sin_port = htons(port);

#ifdef _WIN32
	// DEFAULT_TIMEOUT is in seconds (library was originally built for Linux and Linux/Unix uses seconds, we just need to convert to milliseconds for Windows.
	const int corrected_timeout = DEFAULT_TIMEOUT * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&corrected_timeout, sizeof(corrected_timeout));
#else
	// Set a timeout of 4 seconds.
	struct timeval tv {};
	tv.tv_sec = DEFAULT_TIMEOUT;
	tv.tv_usec = 0;

	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

	// Connect to the socket and set the status of the connection.
	int status = connect(sock, (struct sockaddr*)&server, sizeof(server));

	if (status == -1) {
		return false;
	}

	return true;
}

rconpp::response rconpp::rcon_client::receive_information(int32_t id, rconpp::data_type type) {
	// Whilst this loop is better than a while loop,
	// it should really just keep going for a certain amount of seconds.
	for (int i = 0; i < MAX_RETRIES_TO_RECEIVE_INFO; i++) {
		packet packet_response = read_packet();

		const auto packet_type = static_cast<data_type>(bit32_to_int(packet_response.data));

		if (packet_response.length == 0) {
			if (packet_type != SERVERDATA_AUTH) {
				return { "", packet_response.server_responded };
			}

			continue;
		}

		if (type == SERVERDATA_AUTH) {
			return { "", packet_type == id };
		}

		if (packet_type == id) {
			std::string part{};

			if (packet_response.size > MIN_PACKET_SIZE) {
				part = std::string(&packet_response.data[8], &packet_response.data[packet_response.data.size()-1]);
			}

			return { part, packet_response.server_responded };
		}
	}

	return { "", false };
}

rconpp::packet rconpp::rcon_client::read_packet() {
	const int packet_size = read_packet_size(static_cast<int>(sock), on_log);

	packet temp_packet{};
	temp_packet.length = packet_size + 4;

	if (packet_size > 0) {
		temp_packet.size = packet_size;
	}

	// If the packet size is -1, the server didn't respond.
	if (packet_size == -1) {
		return temp_packet;
	}

	temp_packet.server_responded = true;

	// If the packet size is 0, the server did respond but said nothing.
	if (packet_size == 0) {
		return temp_packet;
	}

	std::vector<char> buffer{};
	buffer.resize(temp_packet.length);

	/*
	 * Receiving by the length of the packet will give us 4 extra bytes, so, we do by size here.
	 * This is because read_packet_size() reads the first 4 bytes and discards them.
	 */
	recv(sock, buffer.data(), temp_packet.size, 0);

	temp_packet.data = buffer;

	return temp_packet;
}

void rconpp::rcon_client::start(const bool return_after) {
	auto block_calling_thread = [this]() {
		std::mutex thread_mutex;
		std::unique_lock thread_lock(thread_mutex);
		this->terminating.wait(thread_lock);
	};

	if (address.empty()) {
		on_log("Address is empty! You need to pass a valid address!");
		return;
	}

	if (port > 65535) {
		on_log("Invalid port! The port can't exceed 65535!");
		return;
	}

	on_log("Attempting connection to RCON server...");

	if (!connect_to_server()) {
		on_log("RCON++ is aborting as it failed to initiate client.");
		return;
	}

	on_log("Connected successfully! Sending login data...");

	// The server will send SERVERDATA_AUTH_RESPONSE once it's happy. If it's not -1, the server will have accepted us!
	// We use the _sync method here to do a blocking call.
	const response response = send_data_sync(password, 1, data_type::SERVERDATA_AUTH, true);

	if (!response.server_responded) {
		on_log("Login data was incorrect. RCON++ will now abort.");
		return;
	}

	on_log("Sent login data.");

	connected = true;

	queue_runner = std::thread([this]() {
		while (connected) {
			if (requests_queued.empty()) {
				continue;
			}

			for (const queued_request& request : requests_queued) {
				// Send data to callback if it's been set.
				if (request.callback)
					request.callback(send_data_sync(request.data, request.id, request.type));
				else
					send_data_sync(request.data, request.id, request.type, false);
			}

			requests_queued.clear();
		}
	});

	if (!return_after) {
		block_calling_thread();
	}
};
