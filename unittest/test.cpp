#include "../include/rconpp/rcon.h"

int main() {
	if(!std::getenv("RCON_TESTING_IP") || !std::getenv("RCON_TESTING_PORT") || !std::getenv("RCON_TESTING_PASSWORD")) {
		throw std::invalid_argument("Environment variables not set.");
	}

	rconpp::rcon_client client(std::getenv("RCON_TESTING_IP"), std::stoi(std::getenv("RCON_TESTING_PORT")), std::getenv("RCON_TESTING_PASSWORD"));

	if(client.connected) {
		std::cout << "Hello!" << "\n";

		rconpp::response res = client.send_data_sync("testing", 3, rconpp::data_type::SERVERDATA_EXECCOMMAND);

		if(res.server_responded) {
			std::cout << "Server responded!" << "\n";
		} else {
			std::cout << "No server response." << "\n";
		}
	} else {
		std::cout << "No connection!" << "\n";
	}
}