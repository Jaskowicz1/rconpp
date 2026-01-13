#include "utilities.h"

#include <iostream>
#include <cstring>

rconpp::packet rconpp::form_packet(const std::string_view data, const int32_t id, const int32_t type) {
	const int32_t data_size = static_cast<int32_t>(data.size()) + MIN_PACKET_SIZE;

	if (data_size > 4096) {
		std::cout << "This packet is too big to send. Please generate a smaller packet." << "\n";
		return {};
	}

	std::vector<char> temp_data(data_size + 4); /* Create a vector that exactly the size of the packet length. */

	std::memcpy(temp_data.data() + 0, &data_size, sizeof(data_size)); /* Copy size into it */
	std::memcpy(temp_data.data() + sizeof(data_size), &id, sizeof(id)); /* Copy id into it */
	std::memcpy(temp_data.data() + sizeof(data_size) + sizeof(id), &type, sizeof(type)); /* Copy type into it */
	std::memcpy(temp_data.data() + sizeof(data_size) + sizeof(id) + sizeof(type), data.data(), data.size()); /* Copy data into it */

	packet temp_packet;
	temp_packet.length = data_size + 4;
	temp_packet.size = data_size;
	temp_packet.data = temp_data;

	return temp_packet;
}

int rconpp::bit32_to_int(const std::vector<char>& buffer) {
	return static_cast<int>(buffer[0] | buffer[1] << 8 | buffer[2] << 16 | buffer[3] << 24);
}

int rconpp::type_to_int(const std::vector<char>& buffer) {
	return static_cast<int>(buffer[4] | buffer[5] << 8 | buffer[6] << 16 | buffer[7] << 24);
}

rconpp::last_error rconpp::get_last_error() {
	error_type last_error_type;

	int last_error_num{-1};

#ifdef _WIN32
	last_error_num = WSAGetLastError();
#else
	last_error_num = errno;
#endif

#ifdef _WIN32
	switch (last_error_num) {
		default:
		case WSAECONNRESET:
			last_error_type = DISCONNECTED;
			break;
		case WSAEINTR:
			last_error_type = SHUTTING_DOWN;
			break;
	}
#else
	switch (last_error_num) {
		default:
		case 32:
		case 104:
			last_error_type = DISCONNECTED;
			break;
	}
#endif

	last_error error{};
	error.type_of_error = last_error_type;
	error.error_code = last_error_num;
	return error;
}

int rconpp::read_packet_size(const SOCKET_TYPE socket) {
	std::vector<char> buffer{};
	buffer.resize(4);

	/*
	 * RCON gives the packet SIZE in the first four (4) bytes of each packet.
	 * We simply just want to read that and then return it.
	 */
	if (recv(socket, buffer.data(), 4, MSG_NOSIGNAL) == -1) {
		return -1;
	}

	return bit32_to_int(buffer);
}
