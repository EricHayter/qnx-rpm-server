/**
 * @file SocketServer.cpp
 * @brief Implementation of the Socket Server for QNX Remote Process Monitor
 *
 * This file implements a TCP/IP socket server that facilitates network
 * communication for the QNX Process Monitor application. It provides
 * functionality for:
 * - Creating and managing a server socket
 * - Accepting and handling multiple client connections
 * - Processing client messages through a message handler callback
 * - Sending responses to clients and broadcasting messages
 *
 * The implementation is thread-safe and handles socket operations in an
 * asynchronous manner using a dedicated server thread and non-blocking I/O
 * operations.
 */

#include "server/SocketServer.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>
#include <vector>

// QNX 8.0 compatibility helpers
#ifdef __QNXNTO__
extern "C" {
// Define missing function prototypes
extern int close(int fd);
extern ssize_t read(int fd, void *buf, size_t count);
extern int inet_ntop(int af, const void *src, char *dst, socklen_t size);
extern ssize_t send(int sockfd, const void *buf, size_t len, int flags);
extern ssize_t recv(int sockfd, void *buf, size_t len, int flags);
}
#endif

namespace qnx {
constexpr int MAX_CLIENTS = 30;
constexpr int BUFFER_SIZE = 4096;

/**
 * @brief Get the singleton instance of the SocketServer class
 *
 * This method implements the singleton pattern to ensure only one instance
 * of SocketServer exists throughout the application lifetime. It is thread-safe
 * due to the static local variable initialization guarantees of C++11.
 *
 * @return Reference to the singleton SocketServer instance
 */
SocketServer &SocketServer::getInstance() {
  static SocketServer instance;
  return instance;
}

/**
 * @brief Destructor for the SocketServer
 *
 * Ensures the server is properly shut down when the instance is destroyed,
 * closing all open connections and releasing resources.
 */
SocketServer::~SocketServer() { shutdown(); }

/**
 * @brief Initialize and start the socket server
 *
 * Creates a TCP/IP socket, binds it to the specified port, and starts
 * listening for incoming connections. Launches a server thread to handle
 * client connections and messages asynchronously.
 *
 * @param port The TCP port to listen on
 * @param handler The callback function to process incoming messages
 * @return true if initialization was successful, false otherwise
 */
bool SocketServer::init(int port, MessageHandler handler) {
  if (running_.load()) {
    std::cerr << "SocketServer already running." << std::endl;
    return true; // Already initialized
  }

  message_handler_ = handler;

  // Create socket
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ == -1) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to create socket: " << ec.message() << std::endl;
    return false;
  }

  // Set socket options for reuse
  int opt = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to set socket options: " << ec.message() << std::endl;
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  // Prepare the server address
  memset(&server_address_, 0, sizeof(server_address_));
  server_address_.sin_family = AF_INET;
  server_address_.sin_addr.s_addr = INADDR_ANY;
  server_address_.sin_port = htons(port);

  // Bind socket to port
  if (bind(server_fd_, (struct sockaddr *)&server_address_,
           sizeof(server_address_)) < 0) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to bind socket to port " << port << ": "
              << ec.message() << std::endl;
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  // Start listening for connections
  if (listen(server_fd_, MAX_CLIENTS) < 0) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to listen on socket: " << ec.message() << std::endl;
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  // Start server thread
  running_ = true;
  server_thread_ = std::thread(&SocketServer::serverLoop, this);

  std::cout << "Socket server initialized on port " << port << std::endl;
  return true;
}

/**
 * @brief Shut down the socket server
 *
 * Performs a clean shutdown of the server by:
 * 1. Setting the running flag to false
 * 2. Closing the server socket to unblock any blocking operations
 * 3. Closing all client connections
 * 4. Joining the server thread to ensure proper termination
 */
void SocketServer::shutdown() {
  if (!running_.exchange(false)) {
    return; // Already shut down or not running
  }

  // Close server socket to unblock accept()
  if (server_fd_ != -1) {
    close(server_fd_);
    server_fd_ = -1;
  }

  // Close all client sockets
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (int client_socket : client_sockets_) {
      close(client_socket);
    }
    client_sockets_.clear();
  }

  // Join the server thread
  if (server_thread_.joinable()) {
    server_thread_.join();
  }

  std::cout << "Socket server shut down." << std::endl;
}

/**
 * @brief Send a message to a specific client with length prefix
 *
 * Prepends a 4-byte network-order length header to the message before sending.
 *
 * @param client_socket The client socket descriptor
 * @param message The message string to send
 * @return true if the message was sent successfully, false otherwise
 */
bool SocketServer::send(int client_socket, const std::string &message) {
  if (::send(client_socket, message.c_str(), message.length(), 0) < 0) {
    std::error_code ec(errno, std::system_category());
    // Don't print error for broken pipe, it happens normally when client
    // disconnects
    if (ec.value() != EPIPE) {
      std::cerr << "Failed to send message to client " << client_socket << ": "
                << ec.message() << std::endl;
    }
    return false;
  }
  return true;
}

/**
 * @brief Broadcast a message to all connected clients
 *
 * Sends the same message to all currently connected clients.
 * Uses thread-safe access to the client list.
 *
 * @param message The message to broadcast
 */
void SocketServer::broadcast(const std::string &message) {
  std::lock_guard<std::mutex> lock(clients_mutex_);
  for (int client_socket : client_sockets_) {
    send(client_socket, message);
  }
}

/**
 * @brief Main server loop that handles connections and messages
 *
 * This method runs in a separate thread and:
 * 1. Uses select() to monitor multiple socket file descriptors
 * 2. Accepts new client connections
 * 3. Receives and processes messages from existing clients
 * 4. Handles client disconnections
 *
 * The loop continues until the server is shut down.
 */
void SocketServer::serverLoop() {
  fd_set read_fds;
  int max_sd;
  struct timeval tv = {1, 0}; // 1 second timeout for select

  while (running_.load()) {
    FD_ZERO(&read_fds);
    if (server_fd_ == -1) { // Server socket closed during shutdown
      break;
    }
    FD_SET(server_fd_, &read_fds);
    max_sd = server_fd_;

    // Add client sockets to set
    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      for (int sd : client_sockets_) {
        FD_SET(sd, &read_fds);
        if (sd > max_sd)
          max_sd = sd;
      }
    }

    // Reset timeout
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    // Wait for activity on any of the sockets
    int activity = select(max_sd + 1, &read_fds, nullptr, nullptr, &tv);

    if (!running_.load())
      break; // Check again after select

    if (activity < 0) {
      // select error (e.g., EINTR)
      if (errno == EINTR)
        continue;
      std::error_code ec(errno, std::system_category());
      std::cerr << "Select error: " << ec.message() << std::endl;
      // Consider more robust error handling, maybe break the loop
      continue;
    }

    if (activity == 0) {
      // Timeout - useful for periodic tasks if needed
      continue;
    }

    // Check for incoming connections
    if (FD_ISSET(server_fd_, &read_fds)) {
      struct sockaddr_in client_address;
      socklen_t client_len = sizeof(client_address);
      int new_socket =
          accept(server_fd_, (struct sockaddr *)&client_address, &client_len);

      if (!running_.load()) { // Check if shutdown was initiated during accept
        if (new_socket >= 0)
          close(new_socket);
        break;
      }

      if (new_socket < 0) {
        // Accept can fail if server_fd_ was closed
        if (errno == EBADF)
          break;
        std::error_code ec(errno, std::system_category());
        std::cerr << "Failed to accept new connection: " << ec.message()
                  << std::endl;
        continue;
      }

      // Get client IP and port
      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
      int client_port = ntohs(client_address.sin_port);

      std::cout << "New connection from " << client_ip << ":" << client_port
                << ", socket fd is " << new_socket << std::endl;

      // Add new client socket to list if there's room
      {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (client_sockets_.size() < MAX_CLIENTS) {
          client_sockets_.push_back(new_socket);
        } else {
          std::cerr << "Maximum clients reached. Rejecting connection from "
                    << client_ip << std::endl;
          close(new_socket);
          continue;
        }
      }
    }

    // Check for activity on client sockets
    std::vector<int> clients_to_remove;
    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      for (int sd : client_sockets_) {
        if (FD_ISSET(sd, &read_fds)) {
          // Use handleClient to process this client's communication
          handleClient(sd);

          // Check if client is still connected after handling
          char buffer[1];
          if (recv(sd, buffer, 1, MSG_PEEK | MSG_DONTWAIT) <= 0 &&
              errno != EAGAIN && errno != EWOULDBLOCK) {
            // Connection closed or error
            clients_to_remove.push_back(sd);
          }
        }
      }

      // Remove disconnected clients
      for (int sd_to_remove : clients_to_remove) {
        client_sockets_.erase(std::remove(client_sockets_.begin(),
                                          client_sockets_.end(), sd_to_remove),
                              client_sockets_.end());
      }
    }
  }
  std::cout << "Server loop terminated." << std::endl;
}

/**
 * @brief Handle communication with a specific client
 *
 * Receives messages from the client, processes them through the message
 * handler, and sends back any responses. Handles errors and client
 * disconnections.
 *
 * @param client_socket The socket descriptor for the client connection
 */
void SocketServer::handleClient(int client_socket) {
  char buffer[BUFFER_SIZE];
  memset(buffer, 0, BUFFER_SIZE);
  ssize_t valread = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

  if (valread > 0) {
    // Process message
    buffer[valread] = '\0';
    std::string message(buffer);

    // Call the message handler (could be in a separate thread for complex
    // processing)
    if (message_handler_) {
      try {
        std::string response = message_handler_(client_socket, message);
        if (!response.empty()) {
          send(client_socket, response);
        }
      } catch (const std::exception &e) {
        std::cerr << "Error processing message from client " << client_socket
                  << ": " << e.what() << std::endl;
      }
    }
  } else {
    // Client disconnected or error
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(client_socket, (struct sockaddr *)&addr, &addr_len) == 0) {
      inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
      std::cout << "Client disconnected: " << client_ip << " on socket fd "
                << client_socket << std::endl;
    } else {
      std::error_code ec(errno, std::system_category());
      std::cerr << "Error reading from client " << client_socket << ": "
                << ec.message() << std::endl;
    }
    close(client_socket);
  }
}
} // namespace qnx
