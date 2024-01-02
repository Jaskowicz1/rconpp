#include "server.h"

#include "utilities.h"

rconpp::rcon_server::rcon_server(const std::string_view addr, const int port, const std::string_view pass) {

	if(port > 65535) {
		std::cout << "Invalid port! The port can't exceed 65535!" << "\n";
		return;
	}

	serv_info.address = addr;
	serv_info.port = port;
	serv_info.password = pass;

	std::cout << "Attempting to startup an RCON server..." << "\n";

	if (!startup_server()) {
		std::cout << "RCON is aborting as it failed to initiate server." << "\n";
		return;
	}

	online = true;

	std::cout << "Server is now listening, initiating runners..." << "\n";

	accept_connections_runner = std::thread([this]() {
		while (online) {
			connected_client client{};
			struct sockaddr_in client_info{};

			socklen_t client_len = sizeof(client_info);
			int client_socket = accept(sock, reinterpret_cast<sockaddr*>(&client_info), &client_len);

			if(client_socket == -1) {
				std::cout << "client with socket: \"" << client_socket << "\" failed to connect." << "\n";
				continue;
			}

			std::cout << "Client [" << inet_ntoa(client_info.sin_addr) << ":" << ntohs(client_info.sin_port) << "] has connected to the server." << "\n";

			client.sock_info = client_info;
			client.socket = client_socket;
			client.connected = true;

			std::thread client_thread([this, client]{
				read_packet(client);
			});

			request_handlers.insert({ client_socket, std::move(client_thread) });

			request_handlers.at(client_socket).detach();

			connected_clients.insert({});
		}
	});

	accept_connections_runner.detach();

	std::cout << "Server is now ready!" << "\n";
}

rconpp::rcon_server::~rcon_server() {
	// Set connected to false, meaning no requests can be attempted during shutdown.
	online = false;

	// Safely disconnect all clients from server.
	for(const auto& client : connected_clients) {
		disconnect_client(client.first);
	}

#ifdef _WIN32
	closesocket(sock);
	WSACleanup();
#else
	close(sock);
#endif
	if (accept_connections_runner.joinable()) {
		accept_connections_runner.join();
	}
}

bool rconpp::rcon_server::startup_server() {
#ifdef _WIN32
	// Initialize Winsock
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		std::cout << "WSAStartup failed. Error: " << result << std::endl;
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
		std::cout << "Failed to open socket." << "\n";
		report_error();
		return false;
	}

	struct sockaddr_in server{};

	// Setup port, address, and family.
	server.sin_family = AF_INET;
#ifdef _WIN32
	#ifdef UNICODE
		InetPton(AF_INET, std::wstring(address.begin(), address.end()).c_str(), &server.sin_addr.s_addr);
	#else
		InetPton(AF_INET, address.c_str(), &server.sin_addr.s_addr);
	#endif
#else
	server.sin_addr.s_addr = INADDR_ANY;
#endif
	server.sin_port = htons(serv_info.port);

	int allow = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &allow, sizeof(allow));
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &allow, sizeof(allow));


	// Connect to the socket and set the status of the connection.
	int status = bind(sock, reinterpret_cast<const sockaddr*>(&server), sizeof(server));

	if (status == -1) {
		report_error();
		return false;
	}

	status = listen(sock, SOMAXCONN);

	if(status == -1) {
		report_error();
		return false;
	}

	return true;
}

void rconpp::rcon_server::disconnect_client(const int client_socket) {
	close(client_socket);

	connected_clients.at(client_socket).connected = false;

	if(request_handlers.at(client_socket).joinable()) {
		request_handlers.at(client_socket).join();
	}

	connected_clients.erase(client_socket);
}

void rconpp::rcon_server::read_packet(rconpp::connected_client client) {
	while(client.connected) {
		size_t packet_size = read_packet_size(client);

		if(packet_size <= 10) {
			continue;
		}

		std::vector<char> buffer{};
		buffer.resize(packet_size);

		if (recv(client.socket, buffer.data(), packet_size, 0) == -1) {
			std::cout << "Failed to get a packet from client." << "\n";
			report_error();
		}

		std::string packet_data(&buffer[8], &buffer[buffer.size()-2]);
		int id = bit32_to_int(buffer);
		int type = type_to_int(buffer);

		rconpp::packet packet_to_send{};

		if(!client.authenticated) {
			if(packet_data == serv_info.password) {
				packet_to_send = form_packet("", id, rconpp::data_type::SERVERDATA_AUTH_RESPONSE);
				client.authenticated = true;
			} else {
				packet_to_send = form_packet("", -1, rconpp::data_type::SERVERDATA_AUTH_RESPONSE);
			}
		} else {
			if(type != rconpp::data_type::SERVERDATA_EXECCOMMAND) {
				packet_to_send = form_packet("Invalid packet type (" + std::to_string(type) + "). Double check your packets.", id, rconpp::data_type::SERVERDATA_RESPONSE_VALUE);
				std::cout << "Invalid packet type (" + std::to_string(type) + ") sent by [" + inet_ntoa(client.sock_info.sin_addr) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "]. Double check your packets." << "\n";
			} else {
				std::cout << "Client [" << inet_ntoa(client.sock_info.sin_addr) << ":" << ntohs(client.sock_info.sin_port) << "] has asked to execute the command: \"" << packet_data << "\"" << "\n";
				if(!on_command) {
					/*
					 * Whilst sending information about the server not responding would be nice,
					 * we would end up with the possibility of clients thinking that is the response.
					 * It's better to just send no information and let clients assume that meant
					 * the server didn't like the command.
					 */
					packet_to_send = form_packet("", id, rconpp::data_type::SERVERDATA_RESPONSE_VALUE);
				} else {
					client_command command{};
					command.command = packet_data;
					command.client = client;

					std::string text_to_send = on_command(command);

					packet_to_send = form_packet(text_to_send, id, rconpp::data_type::SERVERDATA_RESPONSE_VALUE);
				}
			}
		}

		if (send(client.socket, packet_to_send.data.data(), packet_to_send.length, 0) < 0) {
			std::cout << "Sending failed!" << "\n";
			report_error();
			continue;
		}
	}
}

int rconpp::rcon_server::read_packet_size(const rconpp::connected_client client) {
	std::vector<char> buffer{};
	buffer.resize(4);

	/*
	 * RCON gives the packet SIZE in the first four (4) bytes of each packet.
	 * We simply just want to read that and then return it.
	 */
	if (recv(client.socket, buffer.data(), 4, 0) == -1) {
		std::cout << "Did not receive a packet in time. Did the server send a response?" << "\n";
		report_error();
		return -1;
	}

	return bit32_to_int(buffer);
}
