#include "../include/rcon.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <thread>

rcon::rcon(const std::string& addr, const unsigned int _port, const std::string& pass) : address(addr), port(_port), password(pass) {
    
	std::cout << "Attempting connection to RCON server..." << "\n";
    
	if(!connect()) {
		std::cout << "RCON is aborting as it failed to initiate." << "\n";
		close(sock);
		return;
	}
    
	std::cout << "Connected successfully! Sending login data..." << "\n";

	// The server will send SERVERDATA_AUTH_RESPONSE once it's happy. If it's not -1, the server will have accepted us!
	rcon_response response = send_data_sync(pass, 1, data_type::SERVERDATA_AUTH, true);

	if(!response.server_responded) {
		std::cout << "Login data was incorrect. RCON will now abort." << "\n";
		close(sock);
		return;
	}

	std::cout << "Sent login data." << "\n";

	connected = true;
    
	std::thread queue_runner([this]() {
		while(connected) {
			if(requests_queued.empty()) {
				continue;
			}
            
			for(rcon_queued_request request : requests_queued) {
				// Send data to callback if it's been set.
				if(request.callback)
					request.callback(send_data_sync(request.data, request.id, request.type));
				else
					send_data_sync(request.data, request.id, request.type, false);
			}
            
			requests_queued.clear();
		}
	});
    
	queue_runner.detach();
};

void rcon::send_data(const std::string& data, const int32_t id, data_type type, std::function<void(const rcon_response& retrieved_data)> callback) {
	requests_queued.emplace_back(data, id, type, callback);
}

const rcon_response rcon::send_data_sync(const std::string data, const int32_t id, data_type type, bool feedback) {
	if(!connected && type != data_type::SERVERDATA_AUTH) {
		std::cout << "Cannot send data when not connected." << "\n";
		return {"", false};
	}
    
	unsigned long long packet_len = data.length() + HEADER_SIZE;
	unsigned char packet[packet_len];
	form_packet(packet, data, id, type);
    
	if(::send(sock, packet, packet_len, 0) < 0) {
		std::cout << "Sending failed!" << "\n";
		return {"", false};
	}
    
	if(!feedback) {
		// Because we do not want any feedback, we just send no data and say the server didn't respond.
		return {"", false};
	}
    
	// Server will send a SERVERDATA_RESPONSE_VALUE packet.
	return receive_information(id, type);
}

bool rcon::connect() {
	// Create new TCP socket.
	sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(sock == -1) {
		std::cout << "Failed to open socket." << "\n";
		return false;
	}
    
	// Setup port, address, and family.
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(address.c_str());
	server.sin_port = htons(port);

	// Make it non-blocking.
	fcntl(sock, F_SETFL, O_NONBLOCK);

	// Set a timeout of 4 seconds.
	struct timeval tv{};
	tv.tv_sec = DEFAULT_TIMEOUT;
	tv.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
	// Connect to the socket and set the status to our temp status.
	if(::connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1) {
		if(errno != EINPROGRESS) {
			return false;
		}
	}

	// Create temp status
	int status = select(sock +1, nullptr, &fds, nullptr, &tv);
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);

	// If status wasn't zero, we successfully connected.
	return status != 0;
}

void rcon::form_packet(unsigned char packet[], const std::string& data, int32_t id, int32_t type) {
	const char nullbytes[] = {'\x00', '\x00'};
	const int32_t min_size = sizeof(id) + sizeof(type) + sizeof(nullbytes); // 10 bytes.
	const int32_t data_size = static_cast<int32_t>(data.size()) + min_size;
    
	if(data_size > 4096) {
		std::cout << "This packet is too big to send. Please generate a smaller packet." << "\n";
		return;
	}

	(memset(packet, '\0', data_size), (void) 0);

	// Each part is 4 bytes
	packet[0] = data_size;
	packet[4] = id;
	packet[8] = type;

	const char* data_chars = data.c_str();

	for(int i = 0; i < data_size; i++) {
		packet[12 + i] = data_chars[i];
	}
}

rcon_response rcon::receive_information(int32_t id, data_type type) {
	// Whilst this loop is better than a while loop,
	// it should really just keep going for a certain amount of seconds.
	for(int i=0; i < 500; i++) {
		rcon_packet packet = read_packet();

		if(packet.bytes == 0) {
			if(type != SERVERDATA_AUTH)
				return {"", packet.server_responded};
			else
				continue;
		}

		unsigned char* buffer = packet.data;

		if(type == SERVERDATA_AUTH) {
			if(byte32_to_int(buffer) == -1) {
				return {"", false};
			} else {
				if(byte32_to_int(packet.data) == id) {
					return {"", true};
				}
			}
		}

		int offset = packet.bytes - HEADER_SIZE + 3;

		if(offset == -1)
			continue;

		std::string part(&buffer[8], &buffer[8] + offset);

		if(byte32_to_int(packet.data) == id) {
			return {part, packet.server_responded};
		}
	}
	return {"", false};
}

rcon_packet rcon::read_packet() {
	size_t packet_length = read_packet_length();

	/*
	 * If the packet length is -1, the server didn't respond.
	 * If the packet length is 0, the server did respond but said nothing.
	 */
	if(packet_length == -1) {
		return {0, nullptr, false};
	} else if(packet_length == 0) {
		return {0, nullptr, true};
	}

	auto* buffer = new unsigned char[packet_length]{0};
	unsigned int bytes = 0;

	do {
		size_t recv_bytes = ::recv(sock, buffer, packet_length - bytes, 0);
		if(recv_bytes == -1) {
			std::cout << "Did not receive a packet in time. Did the server send a response?" << "\n";
			return {0, nullptr, false};
		}

		bytes += recv_bytes;

	} while(bytes < packet_length);

	return {bytes, buffer};
}

const size_t rcon::read_packet_length() {
	auto* buffer = new unsigned char[4]{0};
	size_t recv_bytes = ::recv(sock, buffer, 4, 0);
	if(recv_bytes == -1) {
		std::cout << "Did not receive a packet in time. Did the server send a response?" << "\n";
		return -1;
	}
	const size_t len = byte32_to_int(buffer);
	delete[] buffer;
	return len;
}
