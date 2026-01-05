#include <mutex>
#include "server.h"

#include "utilities.h"

rconpp::rcon_server::rcon_server(const std::string_view addr, const int _port, const std::string_view pass) : address(addr), port(_port), password(pass) {
}

rconpp::rcon_server::~rcon_server() {
	// Set connected to false, meaning no requests can be attempted during shutdown.
	online = false;

	terminating.notify_all();

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

	if (heartbeat_runner.joinable()) {
		heartbeat_runner.join();
	}
}

bool rconpp::rcon_server::startup_server() {
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

	sockaddr_in server{};

	// Setup port, address, and family.
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	int allow = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&allow), sizeof(allow));

	// Connect to the socket and set the status of the connection.
	int status = bind(sock, reinterpret_cast<const sockaddr*>(&server), sizeof(server));

	if (status == -1) {
		report_error();
		return false;
	}

	status = listen(sock, SOMAXCONN);

	if (status == -1) {
		report_error();
		return false;
	}

	return true;
}

void rconpp::rcon_server::disconnect_client(const int client_socket) {

#ifdef _WIN32
	closesocket(client_socket);
#else
	close(client_socket);
#endif

	connected_clients.at(client_socket).connected = false;

	if (request_handlers.at(client_socket).joinable()) {
		request_handlers.at(client_socket).join();
	}

	connected_clients.erase(client_socket);
}

void rconpp::rcon_server::read_packet(rconpp::connected_client client) {
	const int packet_size = read_packet_size(static_cast<int>(sock), on_log);

	if (packet_size <= MIN_PACKET_SIZE) {
		return;
	}

	std::vector<char> buffer{};
	buffer.resize(packet_size);

	if (recv(client.socket, buffer.data(), packet_size, 0) == -1) {
		on_log("Failed to get a packet from client.");
		report_error();
		return;
	}

	std::string packet_data(&buffer[8], &buffer[buffer.size()-2]);
	int id = bit32_to_int(buffer);
	int type = type_to_int(buffer);

	rconpp::packet packet_to_send{};

	if (!client.authenticated) {
		if (packet_data == password) {
			packet_to_send = form_packet("", id, rconpp::data_type::SERVERDATA_AUTH_RESPONSE);
			client.authenticated = true;
		} else {
			packet_to_send = form_packet("", -1, rconpp::data_type::SERVERDATA_AUTH_RESPONSE);
		}
	} else {
		if (type != rconpp::data_type::SERVERDATA_EXECCOMMAND) {
			packet_to_send = form_packet("Invalid packet type (" + std::to_string(type) + "). Double check your packets.", id, rconpp::data_type::SERVERDATA_RESPONSE_VALUE);
			on_log("Invalid packet type (" + std::to_string(type) + ") sent by [" + inet_ntoa(client.sock_info.sin_addr) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "]. Double check your packets.");
		} else {
			on_log("Client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "] has asked to execute the command: \"" + packet_data + "\"");
			if (!on_command) {
				on_log("You have not set any response for on_command! The server will default to a blank response.");

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

				on_log("Sending reply \"" + text_to_send + "\" to client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "].");

				packet_to_send = form_packet(text_to_send, id, rconpp::data_type::SERVERDATA_RESPONSE_VALUE);
			}
		}
	}

	on_log("Sending packet (of size: " + std::to_string(packet_to_send.length) + ") to client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "]");

	if (send(client.socket, packet_to_send.data.data(), packet_to_send.length, 0) < 0) {
		on_log("Sending failed!");
		report_error();
		return;
	}
}

void rconpp::rcon_server::start(bool return_after) {
	auto block_calling_thread = [this]() {
		std::mutex thread_mutex;
		std::unique_lock thread_lock(thread_mutex);
		this->terminating.wait(thread_lock);
	};

	if (port > 65535) {
		on_log("Invalid port! The port can't exceed 65535!");
		return;
	}

	on_log("Attempting to startup an RCON server...");

	if (!startup_server()) {
		on_log("RCON++ is aborting as it failed to initiate server.");
		return;
	}

	online = true;

	on_log("Server is now listening, initiating runners...");

	accept_connections_runner = std::thread([this]() {
		while (online) {
			connected_client client{};
			struct sockaddr_in client_info{};

			socklen_t client_len = sizeof(client_info);
			int client_socket = accept(sock, reinterpret_cast<sockaddr*>(&client_info), &client_len);

			if (client_socket == -1) {
				on_log("client with socket: \"" + std::to_string(client_socket) + "\" failed to connect.");
				continue;
			}

			on_log("Client [" + std::string(inet_ntoa(client_info.sin_addr)) + ":" + std::to_string(ntohs(client_info.sin_port)) + "] has connected to the server.");

			client.sock_info = client_info;
			client.socket = client_socket;
			client.connected = true;

			std::thread client_thread([this, client]{
				while (client.connected) {
					read_packet(client);

					time_t current_time = time(nullptr);
					if (client_socket_to_last_heartbeat.find(client.socket) == client_socket_to_last_heartbeat.end()) {
						client_socket_to_last_heartbeat.insert({ client.socket, current_time });
					} else {
						time_t last_time = client_socket_to_last_heartbeat.at(client.socket);

						if (current_time - last_time >= HEARTBEAT_TIME)
						{
							send_heartbeat(client);

							// We should check if the heartbeat actually got anything, if it does then insert back into the map.
							// if it failed, we bin that client off and shut this thread down.
						}
					}

					// No need to let the server keep running this causing 100% usage on a thread, we can wait a bit between requests.
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			});

			request_handlers.insert({ client_socket, std::move(client_thread) });

			request_handlers.at(client_socket).detach();

			connected_clients.insert({ client_socket, client });
		}
	});

	accept_connections_runner.detach();

	on_log("Server is now ready!");

	if (!return_after) {
		block_calling_thread();
	}
}
