#include "utilities.h"

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <cstring>

rconpp::packet rconpp::form_packet(const std::string_view data, int32_t id, int32_t type) {
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

void rconpp::report_error() {
#ifdef _WIN32
	std::cout << "Error code: " << WSAGetLastError() << "\n";
#else
	std::cout << "Error code: " << errno << "\n";
#endif
}
