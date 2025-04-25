#pragma once

#include "SocketServer.hpp" // Included for client_socket type
#include <string>
#include <sys/json.h> // Include QNX JSON header

namespace qnx {

/**
 * @brief Handles JSON messages received from clients
 *
 * Processes incoming JSON messages, performs the requested operations,
 * and generates appropriate JSON responses using QNX JSON library.
 *
 * @param client_socket The socket descriptor for the client connection
 * @param message The JSON message received from the client
 * @return std::string JSON response to be sent back to the client
 */
std::string handleMessage(int client_socket, const std::string &message);

/**
 * @brief Validates that the input is properly formatted JSON using QNX JSON
 * library
 *
 * @param json_str The JSON string to validate
 * @return bool True if valid JSON, false otherwise
 */
bool validateJson(const std::string &json_str);

/**
 * @brief Converts internal data structures to JSON format using QNX JSON
 * library
 *
 * @param data The data to convert to JSON
 * @return std::string The JSON representation of the data
 */
std::string toJson(const std::string &data);

/**
 * @brief Handles specific JSON command types using QNX JSON library
 *
 * @param command The command to process
 * @param raw_params_json The raw JSON string containing the parameters
 * @param encoder Pointer to the QNX JSON encoder for building the response
 * @return std::string JSON response
 */
std::string processCommand(const std::string &command,
                           const std::string &raw_params_json,
                           json_encoder_t *encoder);
} // namespace qnx