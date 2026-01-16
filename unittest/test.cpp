#include "../include/rconpp/rcon.h"

int main() {

	try {
		std::cout << "Attempting invalid client setups..." << "\n";

		// Bad ports
		rconpp::rcon_client badclient_a("", -1, "");
		rconpp::rcon_client badclient_b("", 65536, "");

		// Bad addresses
		rconpp::rcon_client badclient_c("", 9, "");
		rconpp::rcon_client badclient_d("0.1.2.3", 9, "");

		std::cout << "No errors encountered, invalid client setups passed!" << "\n";
	} catch(std::exception& e) {
		std::cout << "Invalid client tests failed. Reason: " << e.what() << "\n";
		return -1;
	}

	try {
		std::cout << "Attempting Full Server test..." << "\n";

		rconpp::rcon_server server("0.0.0.0", 27012, "testing");

		server.on_log = [](const std::string_view log) {
			std::cout << "SERVER: " << log << "\n";
		};

		server.on_command = [](const rconpp::client_command& command) {
			if (command.command == "test") {
				return "Success";
			}

			return "Bad Command";
		};

		server.start(true);

		std::cout << "Waiting 1 second, then booting 3 clients..." << "\n";
		std::this_thread::sleep_for(std::chrono::seconds(1));

		for(int i=0; i < 3; i++)
		{
			rconpp::rcon_client client("127.0.0.1", 27012, "testing");

			client.on_log = [](const std::string_view log) {
				std::cout << "CLIENT: " << log << "\n";
			};

			client.start(true);

			if (client.connected) {
				std::cout << "Client connected! Sending test command..." << "\n";
				rconpp::response res = client.send_data_sync("test", 3, rconpp::data_type::SERVERDATA_EXECCOMMAND);

				if (res.server_responded && res.data.find("Success") != std::string::npos) {
					std::cout << "Server responded with Success, Client " << std::to_string(i+1) << " passed!" << "\n";
				} else {
					std::cout << "Bad response received! Response from server was: " << res.data << "\n";
					throw std::logic_error("No server response or bad response sent by server.");
				}
			} else {
				throw std::logic_error("Failed to make a connection to the server.");
			}

			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		std::cout << "All clients received a response, Full server test passed!" << "\n";
	} catch(std::exception& e) {
		std::cout << "Full Server test failed. Reason: " << e.what() << "\n";
		return -1;
	}

	if (std::getenv("RCON_TESTING_IP") && std::getenv("RCON_TESTING_PORT") &&
			std::getenv("RCON_TESTING_PASSWORD")) {
		try {
			std::cout << "Attempting Online Client test..." << "\n";

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
					std::cout << "Server responded, Client test passed!" << "\n";
				} else {
					throw std::logic_error("No server response.");
				}
			} else {
				throw std::logic_error("Failed to make a connection to the server.");
			}
		} catch(std::exception& e) {
			std::cout << "Online Client test failed. Reason: " << e.what() << "\n";
			return -1;
		}
	}
	else {
		std::cout << "Online Client test not running as environment variables not set." << "\n";
	}

	return 0;
}