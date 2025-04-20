/**
 * @file SocketServer.hpp
 * @brief Socket server for handling network communications in the QNX Remote Process Monitor
 *
 * This file defines the SocketServer class that handles network connections and
 * processes client requests. It implements a TCP/IP socket server with support
 * for multiple clients and asynchronous message handling.
 */

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>

namespace qnx
{
    /**
     * @class SocketServer
     * @brief Manages network connections and handles client requests.
     *
     * This singleton class implements a TCP/IP socket server that:
     * - Listens for incoming client connections
     * - Processes JSON-formatted messages from clients
     * - Dispatches requests to appropriate handlers
     * - Sends responses back to clients
     *
     * The server runs in its own thread and can handle multiple client
     * connections simultaneously.
     */
    class SocketServer
    {
    public:
        /**
         * @brief Callback function type for message handling
         *
         * This function type is used to process incoming client messages.
         * It should parse the message, perform the requested operation,
         * and return a response to be sent back to the client.
         */
        using MessageHandler = std::function<std::string(int /* client_socket */, const std::string & /* message */)>;

        /**
         * @brief Get the singleton instance of SocketServer
         *
         * This method implements the singleton pattern, ensuring that
         * only one instance of the SocketServer exists in the application.
         *
         * @return Reference to the singleton instance
         */
        static SocketServer &getInstance();

        // Delete copy/move constructors and assignment operators to enforce singleton pattern
        SocketServer(const SocketServer &) = delete;
        SocketServer &operator=(const SocketServer &) = delete;
        SocketServer(SocketServer &&) = delete;
        SocketServer &operator=(SocketServer &&) = delete;

        /**
         * @brief Initialize and start the socket server.
         *
         * This method:
         * 1. Creates a socket and binds it to the specified port
         * 2. Starts listening for incoming connections
         * 3. Launches a server thread to accept and handle connections
         *
         * @param port The TCP port to listen on
         * @param handler The callback function to process incoming messages
         * @return true on successful initialization, false on error
         */
        bool init(int port, MessageHandler handler);

        /**
         * @brief Shut down the socket server.
         *
         * This method:
         * 1. Sets the running state to false
         * 2. Closes all client connections
         * 3. Closes the server socket
         * 4. Joins the server thread to ensure clean shutdown
         */
        void shutdown();

        /**
         * @brief Send a message to a specific client.
         *
         * This method sends a message to a specific client identified
         * by their socket descriptor.
         *
         * @param client_socket The client socket descriptor
         * @param message The message to send
         * @return true on successful send, false on error
         */
        bool send(int client_socket, const std::string &message);

        /**
         * @brief Broadcast a message to all connected clients.
         *
         * This method sends the same message to all currently
         * connected clients.
         *
         * @param message The message to broadcast
         */
        void broadcast(const std::string &message);

        /**
         * @brief Check if the server is running
         *
         * @return true if the server is initialized and running, false otherwise
         */
        bool isRunning() const { return running_.load(); }

        /**
         * @brief Message type constants for client-server communication
         */
        inline static const std::string GET_PROCESSES = "GetProcesses";
        inline static const std::string GET_SIMPLE_DETAILS = "GetSimpleProcessDetails";
        inline static const std::string GET_DETAILED_DETAILS = "GetDetailedProcessDetails";
        inline static const std::string SUSPEND_PROCESS = "SuspendProcess";
        inline static const std::string RESUME_PROCESS = "ResumeProcess";
        inline static const std::string TERMINATE_PROCESS = "TerminateProcess";
        inline static const std::string AUTH_LOGIN = "Login";

    private:
        /**
         * @brief Default constructor - private to enforce singleton pattern
         */
        SocketServer() = default;

        /**
         * @brief Destructor - ensures resources are properly cleaned up
         */
        ~SocketServer(); // Ensure resources are cleaned up

        /**
         * @brief Main server loop that accepts new connections
         *
         * This method runs in a separate thread and:
         * 1. Accepts incoming client connections
         * 2. Creates a client handler for each new connection
         * 3. Continues until the server is shut down
         */
        void serverLoop();

        /**
         * @brief Handles communication with a specific client
         *
         * This method:
         * 1. Receives messages from the client
         * 2. Passes messages to the message handler
         * 3. Sends responses back to the client
         * 4. Handles disconnection and cleanup
         *
         * @param client_socket The socket descriptor for the client connection
         */
        void handleClient(int client_socket);

        int server_fd_ = -1;                ///< Server socket file descriptor
        std::vector<int> client_sockets_;   ///< List of connected client sockets
        std::mutex clients_mutex_;          ///< Mutex to protect concurrent access to the client list
        std::atomic<bool> running_{false};  ///< Flag indicating if the server is running
        std::thread server_thread_;         ///< Thread that runs the server loop
        MessageHandler message_handler_;    ///< Callback function for processing messages
        struct sockaddr_in server_address_; ///< Server address configuration
    };
}