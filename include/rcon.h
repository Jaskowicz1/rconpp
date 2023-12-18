#pragma once

#include <string>
#include <functional>

#define DEFAULT_TIMEOUT 4
#define HEADER_SIZE 14

enum data_type {
	/**
	 * @brief A command packet.
	 *
	 * @note The server *may* send a `SERVERDATA_RESPONSE_VALUE` packet if the request was successful.
	 * However, The server can also not send a packet back if it only processes the packet and does nothing else.
	 * You should take this into account by either not using the callback or by turning feedback off.
	 */
	SERVERDATA_EXECCOMMAND = 2,

	/**
	 * @brief An authorisation packet.
	 *
	 * The server will send an empty `SERVERDATA_AUTH_RESPONSE` packet if the request was successful.
	 */
	SERVERDATA_AUTH = 3,
};

struct rcon_packet {
	unsigned int bytes;
	unsigned char* data;
	bool server_responded;

	~rcon_packet() {
		delete[] data;
	}
};

struct rcon_response {
	std::string data;
	bool server_responded;
};

struct rcon_queued_request {
	std::string data;
	int32_t id;
	data_type type;
	std::function<void(const rcon_response& response)> callback;

	rcon_queued_request(const std::string& _data, const int32_t _id, const data_type _type, std::function<void(const rcon_response& retrieved_data)> _callback) : data(_data), id(_id), type(_type), callback(_callback) {}
};

class rcon {
	const std::string address;
	const unsigned int port;
	const std::string password;
	int sock{0};
	bool connected{false};
	
	std::vector<rcon_queued_request> requests_queued{};
    
public:
    
	/**
	 * @brief rcon constuctor. Initiates a connection to an RCON server with the parameters given.
	 *
	 * @note This is a blocking call (done on purpose). It needs to wait to connect to the RCON server before anything else happens.
	 * It will timeout after 4 seconds if it can't connect.
	 */
	rcon(const std::string& addr, const unsigned int _port, const std::string& pass);
    
	/**
	 * @brief Send data to the connected RCON server. Requests from this function are added to a queue (`requests_queued`) and are handled by a different thread.
	 *
	 * @param data Data to send to the server.
	 * @param id ID of the packet. Try to make sure you aren't sending multiple requests, at the same time, with the same ID as it may cause issues.
	 * @param type The type of packet to send.
	 *
	 * @warning If you are expecting no response from the server, do NOT use the callback. You will halt the RCON process until the next received message (which will chain).
	 */
	void send_data(const std::string& data, const int32_t id, data_type type, std::function<void(const rcon_response& retrieved_data)> callback = {});
    
	/**
	 * @brief Send data to the connected RCON server.
	 *
	 * @param data Data to send to the server.
	 * @param id ID of the packet. Try to make sure you aren't sending multiple requests, at the same time, with the same ID as it may cause issues.
	 * @param type The type of packet to send.
	 * @param feedback Should the client expect a message back from the server? (optional, default is true).
	 *
	 * @warning If you are expecting no response from the server, set `feedback` to false. Otherwise, you will halt the RCON process for 4 seconds.
	 *
	 * @returns Data given by the server from the request.
	 */
	const rcon_response send_data_sync(const std::string data, const int32_t id, data_type type, bool feedback = true);
    
private:
    
	/**
	 * @brief Connects to RCON using `address`, `port`, and `password`.
	 * Those values are pre-filled when constructing this class.
	 *
	 * @warning This should only ever be called by the constructor.
	 * The constructor calls this function once it has filled in the required data and proceeds to login.
	 */
	bool connect();
    
	/**
	 * @brief Form a valid RCON packet.
	 *
	 * @param packet[] The packet to update information for.
	 * @param data The data to add to the packet.
	 * @param id The ID of the request.
	 * @param type The type of packet.
	 */
	void form_packet(unsigned char packet[], const std::string& data, int32_t id, int32_t type);
    
	/**
	 * @brief Ask to receive information from the server for a specified ID.
	 *
	 * @param id The ID that we should except the server to return, alongside information.
	 *
	 * @return Data given by the server.
	 */
	rcon_response receive_information(int32_t id, data_type type);
    
	/**
	 * @brief Gathers all the packet's content (based on the length returned by `read_packet_length`)
	 */
	rcon_packet read_packet();
    
	const size_t read_packet_length();

	inline const int byte32_to_int(unsigned char* buffer) {
		return static_cast<int>(buffer[0] | buffer[1] << 8 | buffer[2] << 16 | buffer[3] << 24);
	}
};
