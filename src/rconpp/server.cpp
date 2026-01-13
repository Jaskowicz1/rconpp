#include <mutex>
#include <csignal>
#include "server.h"

#include "utilities.h"

rconpp::rcon_server::rcon_server(const std::string_view addr, const int _port, const std::string_view pass) : address(addr), port(_port), password(pass) {
}

rconpp::rcon_server::~rcon_server() {
	if (on_log) {
		on_log("RCON server is shutting down.");
	}

	// Set connected to false, meaning no requests can be attempted during shutdown.
	online = false;

	terminating.notify_all();

	// Safely disconnect all clients from server.
	for(const auto& client : connected_clients) {
		disconnect_client(client.first, false);
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
		on_log("WSAStartup failed. Error: " + std::to_string(result));
		return false;
	}
#else
	signal(SIGPIPE, SIG_IGN);
#endif

	// Create new TCP socket.
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

#ifdef _WIN32
	if (sock == INVALID_SOCKET) {
#else
	if (sock == -1) {
#endif
		const last_error err = get_last_error();
		on_log("Failed to open socket [Error code: " + std::to_string(err.error_code) + "]!");
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
		return false;
	}

	status = listen(sock, SOMAXCONN);

	if (status == -1) {
		return false;
	}

	return true;
}

void rconpp::rcon_server::disconnect_client(const SOCKET_TYPE client_socket, const bool remove_after /*= true*/) {

#ifdef _WIN32
	closesocket(client_socket);
#else
	close(client_socket);
#endif

	std::lock_guard guard(connected_clients_mutex);

	if (connected_clients.find(client_socket) == connected_clients.end())
	{
		return;
	}

	connected_client& client = connected_clients.at(client_socket);

	client.connected = false;
	client.authenticated = false;

	if (request_handlers.at(client_socket).joinable()) {
		request_handlers.at(client_socket).join();
	}

	on_log("Client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "] has been disconnected from the server.");

	if (remove_after) {
		connected_clients.erase(client_socket);
	}
}

void rconpp::rcon_server::read_packet(connected_client& client) {
	const int packet_size = read_packet_size(client.socket);

	// Silently ignore packet size.
	if (packet_size < MIN_PACKET_SIZE) {
		return;
	}

	std::vector<char> buffer{};
	buffer.resize(packet_size);

	if (recv(client.socket, buffer.data(), packet_size, MSG_NOSIGNAL) == -1) {
		const last_error err = get_last_error();
		on_log("Failed to get a packet from client [Error code: " + std::to_string(err.error_code) + "]!");
		return;
	}

	// Client is talking to us, we don't need to send a heartbeat if we're being talked to.
	client.last_heartbeat = time(nullptr);

	std::string packet_data(&buffer[8], &buffer[buffer.size()-2]);
	int id = bit32_to_int(buffer);
	int type = type_to_int(buffer);

	packet packet_to_send{};

	if (!client.authenticated) {
		on_log("Client not authenticated, handling authentication.");
		if (packet_data == password) {
			packet_to_send = form_packet("", id, SERVERDATA_AUTH_RESPONSE);
			client.authenticated = true;
			on_log("Client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "] has authenticated successfully!");
		} else {
			packet_to_send = form_packet("", -1, SERVERDATA_AUTH_RESPONSE);
			on_log("Client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "] failed authentication!");
		}
	} else {
		if (type != SERVERDATA_EXECCOMMAND) {
			packet_to_send = form_packet("Invalid packet type (" + std::to_string(type) + "). Double check your packets.", id, SERVERDATA_RESPONSE_VALUE);
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
				packet_to_send = form_packet("", id, SERVERDATA_RESPONSE_VALUE);
			} else {
				client_command command{};
				command.command = packet_data;
				command.client = client;

				std::string text_to_send = on_command(command);

				on_log("Sending reply \"" + text_to_send + "\" to client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "].");

				packet_to_send = form_packet(text_to_send, id, SERVERDATA_RESPONSE_VALUE);
			}
		}
	}

	on_log("Sending packet (of size: " + std::to_string(packet_to_send.length) + ") to client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "]");

	if (send(client.socket, packet_to_send.data.data(), packet_to_send.length, MSG_NOSIGNAL) < 0) {
		const last_error err = get_last_error();
		on_log("Sending failed [Error code: " + std::to_string(err.error_code) + "]!");
		return;
	}
}

bool rconpp::rcon_server::send_heartbeat(connected_client& client) {
	on_log("Sending heartbeat to client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "]");

	packet packet_to_send = form_packet("", -1, SERVERDATA_RESPONSE_VALUE);
	if (send(client.socket, packet_to_send.data.data(), packet_to_send.length, MSG_NOSIGNAL) < 0) {
		const last_error err = get_last_error();
		on_log("Failed to send a heartbeat to client [" + std::string(inet_ntoa(client.sock_info.sin_addr)) + ":" + std::to_string(ntohs(client.sock_info.sin_port)) + "] [Error code: " + std::to_string(err.error_code) + "]!");
		return false;
	}

	client.last_heartbeat = time(nullptr);

	return true;
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
		on_log("RCON server is aborting as it failed to initiate server.");
		return;
	}

	online = true;

	on_log("Server is now listening, initiating runners...");

	accept_connections_runner = std::thread([this]() {
		while (online) {
			sockaddr_in client_info{};

			socklen_t client_len = sizeof(client_info);
			SOCKET_TYPE client_socket = accept(sock, reinterpret_cast<sockaddr*>(&client_info), &client_len);

			if (client_socket == INVALID_SOCKET) {
				const last_error err = get_last_error();
				on_log("A new client attempted to join but failed [Error code: " + std::to_string(err.error_code) + "]!");
				continue;
			}

			on_log("Client [" + std::string(inet_ntoa(client_info.sin_addr)) + ":" + std::to_string(ntohs(client_info.sin_port)) + "] has connected to the server, asking for authentication.");

			connected_client client{};

			client.sock_info = client_info;
			client.socket = client_socket;
			client.connected = true;

			// It is rather inefficient to be spinning up a thread per client. The best way to do it is probably spinning up a thread per ~100 clients or something
			std::thread client_thread([this, &client]{
				while (client.connected && !client.pending_disconnect) {
					read_packet(client);

					const time_t current_time = time(nullptr);

					if (client.authenticated) {
						if (client.last_heartbeat == 0 || current_time - client.last_heartbeat >= HEARTBEAT_TIME)
						{
							if (!send_heartbeat(client)) {
								client.pending_disconnect = true;
							}
						}
					}

					if (client.pending_disconnect) {
						disconnect_client(client.socket);
					}

					// No need to let the server keep running this causing 100% usage on a thread, we can wait a bit between requests.
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			});

			request_handlers.insert({ client_socket, std::move(client_thread) });

			request_handlers.at(client_socket).detach();

			std::lock_guard guard(connected_clients_mutex);

			connected_clients.insert({ client_socket, client });
		}
	});

	accept_connections_runner.detach();

	on_log("Server is now ready!");

	if (!return_after) {
		block_calling_thread();
	}
}
