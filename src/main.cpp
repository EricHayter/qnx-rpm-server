/**
 * @file main.cpp
 * @brief Main entry point for the QNX Remote Process Monitor (RPM) server
 *
 * This application implements a server that allows remote monitoring and
 * management of processes running on a QNX system. It provides functionality
 * for authentication, process listing, detailed process information retrieval,
 * and process control (suspend, resume, terminate).
 *
 * The server communicates with clients using a JSON-based protocol over TCP/IP
 * and handles concurrent client connections.
 */

#include "ProcessCore.hpp"
#include "ProcessGroup.hpp"
#include "ProcessControl.hpp"
#include "SocketServer.hpp"
#include "ProcessHistory.hpp"
#include "Authenticator.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <sstream>
#include <iomanip> // For std::setw, std::fixed, std::setprecision
#include <vector>
#include <string>
#include <optional>
#include <sys/json.h> // QNX native JSON library

/**
 * @brief JSON helper functions for request/response handling
 */
// --- Simple JSON Helpers ---

/**
 * @brief Create a JSON error response
 *
 * Generates a standardized error response in JSON format with the
 * provided error message and details.
 *
 * @param error The primary error message
 * @param details Additional error details or context
 * @return A JSON-formatted error response string
 */
std::string create_error_response(const std::string &error, const std::string &details)
{
    json_encoder_t *enc = json_encoder_create();
    json_encoder_start_object(enc, NULL);
    json_encoder_add_string(enc, "error", error.c_str());
    json_encoder_add_string(enc, "details", details.c_str());
    json_encoder_end_object(enc);
    const char *json_str = json_encoder_buffer(enc);
    std::string response(json_str);
    json_encoder_destroy(enc);
    return response;
}

// --- End Simple JSON Helpers ---

/**
 * @brief Flag to control the server's main loop
 * Set to false to initiate a clean shutdown
 */
std::atomic<bool> running(true);

/**
 * @brief Signal handler for graceful termination
 *
 * Handles SIGINT and SIGTERM signals by setting the running flag to false,
 * which will trigger a clean shutdown of the server.
 *
 * @param signal The signal number that was received
 */
void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        running = false;
    }
}

/**
 * @brief Message handler callback for the SocketServer
 *
 * This is the core request handler that processes all client requests.
 * It parses incoming JSON messages, performs the requested operations,
 * and generates appropriate JSON responses.
 *
 * @param client_socket The socket descriptor for the client connection
 * @param message The JSON message received from the client
 * @return A JSON-formatted response string
 */
std::string messageHandler(int client_socket, const std::string &message)
{
    try
    {
        // Create JSON decoder for parsing the request
        json_decoder_t *decoder = json_decoder_create();
        json_decoder_error_t status = json_decoder_parse_json_str(decoder, message.c_str());

        if (status != JSON_DECODER_OK)
        {
            json_decoder_destroy(decoder);
            return create_error_response("Invalid JSON format", "Failed to parse JSON request");
        }

        // Navigate to root object
        json_decoder_push_object(decoder, NULL, false);

        // Get request_type
        const char *req_type_ptr = NULL;
        if (json_decoder_get_string(decoder, "request_type", &req_type_ptr, false) != JSON_DECODER_OK || req_type_ptr == NULL)
        {
            json_decoder_destroy(decoder);
            return create_error_response("Missing or invalid 'request_type'", "Request type must be a string");
        }

        std::string req_type(req_type_ptr);

        // Create encoder for response
        json_encoder_t *encoder = json_encoder_create();
        json_encoder_start_object(encoder, NULL);
        json_encoder_add_string(encoder, "request_type", req_type.c_str());

        // Get singleton instances for core services
        auto &proc_core = qnx::core::ProcessCore::getInstance();
        auto &proc_hist = qnx::history::ProcessHistory::getInstance();
        // ProcessControl is a static utility class, not a singleton
        // auto &proc_ctrl = qnx::utils::ProcessControl::getInstance();

        // --- Handle Different Request Types ---
        if (req_type == qnx::network::MSG_GET_PROCESSES)
        {
            // Handle request to get all processes
            const auto &proc_list = proc_core.getProcessList();

            // Add array of PIDs
            json_encoder_start_array(encoder, "pids");
            for (const auto &proc : proc_list)
            {
                json_encoder_add_int(encoder, NULL, proc.getPid());
            }
            json_encoder_end_array(encoder);
        }
        else if (req_type == qnx::network::MSG_GET_SIMPLE_DETAILS)
        {
            // Handle request to get basic process details
            int pid = 0;
            if (json_decoder_get_int(decoder, "PID", &pid, false) != JSON_DECODER_OK)
            {
                json_decoder_destroy(decoder);
                json_encoder_destroy(encoder);
                return create_error_response("Missing or invalid 'PID'", "PID must be an integer");
            }

            json_encoder_add_int(encoder, "pid", pid);
            auto proc_info = proc_core.getProcessById(pid);

            if (proc_info)
            {
                // Add basic process information to response
                json_encoder_add_string(encoder, "name", proc_info->getName().c_str());
                json_encoder_add_double(encoder, "cpu_usage", proc_info->getCpuUsage());
                json_encoder_add_int_ll(encoder, "ram_usage", proc_info->getMemoryUsage());

                // Calculate uptime
                auto now = std::chrono::system_clock::now();
                auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - proc_info->getStartTime());
                json_encoder_add_int_ll(encoder, "uptime", uptime_ms.count());
            }
            else
            {
                json_encoder_add_string(encoder, "error", "Process not found");
            }
        }
        else if (req_type == qnx::network::MSG_GET_DETAILED_DETAILS)
        {
            // Handle request to get detailed process history
            int pid = 0;
            if (json_decoder_get_int(decoder, "PID", &pid, false) != JSON_DECODER_OK)
            {
                json_decoder_destroy(decoder);
                json_encoder_destroy(encoder);
                return create_error_response("Missing or invalid 'PID'", "PID must be an integer");
            }

            json_encoder_add_int(encoder, "pid", pid);

            // Add current data to history first
            auto current_info = proc_core.getProcessById(pid);
            if (current_info)
            {
                proc_hist.addEntry(pid, current_info->getCpuUsage(), current_info->getMemoryUsage());
            }

            // Retrieve history (limit to e.g., 60 entries)
            auto history_entries = proc_hist.getEntries(pid, 60);

            // Build response with historical data
            json_encoder_start_array(encoder, "entries");
            for (const auto &entry : history_entries)
            {
                auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        entry.timestamp.time_since_epoch())
                                        .count();

                json_encoder_start_object(encoder, NULL);
                json_encoder_add_double(encoder, "cpu_usage", entry.cpu_usage);
                json_encoder_add_int_ll(encoder, "memory_usage", entry.memory_usage);
                json_encoder_add_int_ll(encoder, "timestamp", timestamp_ms);
                json_encoder_end_object(encoder);
            }
            json_encoder_end_array(encoder);
        }
        else if (req_type == qnx::network::MSG_SUSPEND_PROCESS)
        {
            // Handle request to suspend a process
            int pid = 0;
            if (json_decoder_get_int(decoder, "PID", &pid, false) != JSON_DECODER_OK)
            {
                json_decoder_destroy(decoder);
                json_encoder_destroy(encoder);
                return create_error_response("Missing or invalid 'PID'", "PID must be an integer");
            }

            json_encoder_add_int(encoder, "pid", pid);
            json_encoder_add_bool(encoder, "success", qnx::utils::ProcessControl::suspend(pid));
        }
        else if (req_type == qnx::network::MSG_RESUME_PROCESS)
        {
            // Handle request to resume a suspended process
            int pid = 0;
            if (json_decoder_get_int(decoder, "PID", &pid, false) != JSON_DECODER_OK)
            {
                json_decoder_destroy(decoder);
                json_encoder_destroy(encoder);
                return create_error_response("Missing or invalid 'PID'", "PID must be an integer");
            }

            json_encoder_add_int(encoder, "pid", pid);
            json_encoder_add_bool(encoder, "success", qnx::utils::ProcessControl::resume(pid));
        }
        else if (req_type == qnx::network::MSG_TERMINATE_PROCESS)
        {
            // Handle request to terminate a process
            int pid = 0;
            if (json_decoder_get_int(decoder, "PID", &pid, false) != JSON_DECODER_OK)
            {
                json_decoder_destroy(decoder);
                json_encoder_destroy(encoder);
                return create_error_response("Missing or invalid 'PID'", "PID must be an integer");
            }

            json_encoder_add_int(encoder, "pid", pid);
            json_encoder_add_bool(encoder, "success", qnx::utils::ProcessControl::terminate(pid));
        }
        else if (req_type == qnx::network::MSG_AUTH_LOGIN)
        {
            // Handle login authentication
            const char *username_ptr = NULL;
            const char *password_ptr = NULL;

            if (json_decoder_get_string(decoder, "username", &username_ptr, false) != JSON_DECODER_OK ||
                json_decoder_get_string(decoder, "password", &password_ptr, false) != JSON_DECODER_OK ||
                username_ptr == NULL || password_ptr == NULL)
            {
                json_decoder_destroy(decoder);
                json_encoder_destroy(encoder);
                return create_error_response("Missing or invalid credentials",
                                             "Both username and password must be provided as strings");
            }

            std::string username(username_ptr);
            std::string password(password_ptr);

            bool authenticated = Authenticator::ValidateLogin(username, password);
            json_encoder_add_bool(encoder, "authenticated", authenticated);
        }
        else
        {
            // Handle unknown request type
            json_decoder_destroy(decoder);
            json_encoder_destroy(encoder);
            return create_error_response("Unknown request type", "Request type '" + req_type + "' is not recognized");
        }

        // Clean up decoder since we're done with it
        json_decoder_destroy(decoder);

        // Finalize and get response
        json_encoder_end_object(encoder);
        const char *json_response = json_encoder_buffer(encoder);
        std::string response_str(json_response);
        json_encoder_destroy(encoder);

        return response_str;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error processing message: " << e.what() << std::endl;
        return create_error_response("Failed to process request", e.what());
    }
}

/**
 * @brief Main entry point for the application
 *
 * Initializes the server components, sets up signal handlers, and
 * runs the main monitoring loop until termination is requested.
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return 0 on successful execution, non-zero on error
 */
int main(int argc, char *argv[])
{
    // Default port (can be overridden by arguments later)
    int port = 8080;

    try
    {
        // Set up signal handling
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        // Initialize core components
        auto &proc_core = qnx::core::ProcessCore::getInstance();
        auto &proc_group = qnx::process::ProcessGroup::getInstance();
        auto &proc_hist = qnx::history::ProcessHistory::getInstance();
        auto &socket_server = qnx::network::SocketServer::getInstance();

        std::cout << "Initializing modules..." << std::endl;
        if (!proc_core.init())
        {
            std::cerr << "Failed to initialize ProcessCore" << std::endl;
            return 1;
        }
        if (!proc_group.init())
        {
            std::cerr << "Failed to initialize ProcessGroup" << std::endl;
            return 1;
        }
        if (!proc_hist.init())
        { // Use default history sizes
            std::cerr << "Failed to initialize ProcessHistory" << std::endl;
            return 1;
        }
        // Initialize socket server, passing our message handler
        if (!socket_server.init(port, messageHandler))
        {
            std::cerr << "Failed to initialize SocketServer on port " << port << std::endl;
            // Attempt partial shutdown
            proc_hist.shutdown();
            proc_group.shutdown();
            proc_core.shutdown();
            return 1;
        }
        std::cout << "Initialization complete." << std::endl;

        // Create some default process groups (example)
        int system_group = proc_group.createGroup("System", 20);
        int user_group = proc_group.createGroup("User", 10);
        int background_group = proc_group.createGroup("Background", 5);

        // Mark variables as intentionally unused to suppress warnings
        (void)system_group;
        (void)user_group;
        (void)background_group;

        std::cout << "QNX Process Monitor running. Press Ctrl+C to exit." << std::endl;
        std::cout << "Listening for client connections on port " << port << std::endl;

        // Main monitoring loop (can be simplified now that server runs in own thread)
        while (running.load())
        {
            // Collect process information periodically
            int count = proc_core.collectInfo();
            if (count < 0)
            {
                std::cerr << "Warning: Failed to collect process information" << std::endl;
            }

            // Update group statistics
            proc_group.updateGroupStats();

            // Add current process data to history (example: add for all processes)
            const auto &process_list = proc_core.getProcessList();
            for (const auto &proc : process_list)
            {
                proc_hist.addEntry(proc.getPid(), proc.getCpuUsage(), proc.getMemoryUsage());
            }

            // Optionally: Display info locally (or rely solely on client requests)
            // std::cout << "\033[2J\033[H"; // Clear screen and move cursor to top
            // std::cout << "Monitoring " << count << " processes..." << std::endl;
            // proc_core.displayInfo();
            // proc_group.displayGroups();

            // Wait before next update cycle
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2s); // Adjust interval as needed
        }

        // Clean shutdown
        std::cout << "\nShutting down..." << std::endl;
        socket_server.shutdown();
        proc_hist.shutdown();
        proc_group.shutdown();
        proc_core.shutdown();

        std::cout << "Shutdown complete." << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Critical Error: " << e.what() << std::endl;
        // Attempt partial shutdown if possible
        if (qnx::network::SocketServer::getInstance().isRunning())
            qnx::network::SocketServer::getInstance().shutdown();
        qnx::history::ProcessHistory::getInstance().shutdown();
        qnx::process::ProcessGroup::getInstance().shutdown();
        qnx::core::ProcessCore::getInstance().shutdown();
        return 1;
    }
}