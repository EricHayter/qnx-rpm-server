/**
 * socket_server.h - Simple socket server for process data
 */

#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

// Message types
#define MSG_GET_PROCESSES "GetProcesses"
#define MSG_GET_SIMPLE_DETAILS "GetSimpleProcessDetails"
#define MSG_GET_DETAILED_DETAILS "GetDetailedProcessDetails"
#define MSG_SUSPEND_PROCESS "SuspendProcess"

/**
 * Initialize and start the socket server
 * @param port The port to listen on
 * @return 0 on success, -1 on error
 */
int socket_server_init(int port);

/**
 * Broadcast a message to all connected clients
 * @param message The message to broadcast
 */
void socket_server_broadcast(const char* message);

/**
 * Send a message to a specific client
 * @param client_socket The client socket to send to
 * @param message The message to send
 * @return 0 on success, -1 on error
 */
int socket_server_send(int client_socket, const char* message);

/**
 * Shut down the socket server
 */
void socket_server_shutdown(void);

#endif /* SOCKET_SERVER_H */