#include "../include/rconpp/rcon.h"

int main() {
	try {
		if (!std::getenv("RCON_TESTING_IP") || !std::getenv("RCON_TESTING_PORT") ||
		    !std::getenv("RCON_TESTING_PASSWORD")) {
			throw std::invalid_argument("Environment variables not set.");
		}

		rconpp::rcon_client client(std::getenv("RCON_TESTING_IP"), std::stoi(std::getenv("RCON_TESTING_PORT")),
					   std::getenv("RCON_TESTING_PASSWORD"));

		if (client.connected) {
			rconpp::response res = client.send_data_sync("testing", 3, rconpp::data_type::SERVERDATA_EXECCOMMAND);

			if (res.server_responded) {
				std::cout << "Server responded!" << "\n";
				std::cout << "Client test passed!" << "\n";
			} else {
				std::cout << "No server response." << "\n";
			}
		} else {
			std::cout << "No connection!" << "\n";
		}
	} catch(std::exception& e) {
		std::cout << "Client test failed. Reason: " << e.what() << "\n";
	}

	try {
		rconpp::rcon_server server("0.0.0.0", 27015, "testing");

		server.on_command = [](const rconpp::client_command& command) {
			if (command.command == "test") {
				return "This is a test!";
			} else if (command.command == "/players") {
				return "Players: none lol";
			} else {
				return "Hello!";
			}
		};

		std::cout << "Server test passed!" << "\n";
	} catch(std::exception& e) {
		std::cout << "Server test failed. Reason: " << e.what() << "\n";
	}
}