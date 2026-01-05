#include "../include/rconpp/rcon.h"

int main() {

	try {
		std::cout << "Attempting invalid client setups!" << "\n";

		// Bad ports
		rconpp::rcon_client badclient_a("", -1, "");
		rconpp::rcon_client badclient_b("", 65536, "");

		// Bad addresses
		rconpp::rcon_client badclient_c("", 9, "");
		rconpp::rcon_client badclient_d("0.1.2.3", 9, "");

		std::cout << "No errors encountered, invalid client setups passed!" << "\n";
	} catch(std::exception& e) {
		std::cout << "Invalid client tests failed. Reason: " << e.what() << "\n";
	}

	try {
		std::cout << "Attempting valid client setup!" << "\n";

		if (!std::getenv("RCON_TESTING_IP") || !std::getenv("RCON_TESTING_PORT") ||
		    !std::getenv("RCON_TESTING_PASSWORD")) {
			throw std::invalid_argument("Environment variables not set.");
		}

		rconpp::rcon_client client(std::getenv("RCON_TESTING_IP"), std::stoi(std::getenv("RCON_TESTING_PORT")),
					   std::getenv("RCON_TESTING_PASSWORD"));

		client.on_log = [](const std::string_view log) {
			std::cout << log << "\n";
		};

	 	client.start(true);

		if (client.connected) {
			rconpp::response res = client.send_data_sync("testing", 3, rconpp::data_type::SERVERDATA_EXECCOMMAND);

			if (res.server_responded) {
				std::cout << "Server responded!" << "\n";
				std::cout << "Client test passed!" << "\n";
			} else {
				throw std::logic_error("No server response.");
			}
		} else {
			throw std::logic_error("Failed to make a connection to the server.");
		}
	} catch(std::exception& e) {
		std::cout << "Client test failed. Reason: " << e.what() << "\n";
	}

	try {
		rconpp::rcon_server server("0.0.0.0", 27015, "testing");

		server.on_log = [](const std::string_view log) {
			std::cout << log << "\n";
		};

		server.on_command = [](const rconpp::client_command& command) {
			if (command.command == "test") {
				return "This is a test!";
			} else if (command.command == "/players") {
				return "Players: none lol";
			} else {
				return "Hello!";
			}
		};

		server.start(true);

		std::cout << "Server test passed!" << "\n";
	} catch(std::exception& e) {
		std::cout << "Server test failed. Reason: " << e.what() << "\n";
	}
}