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
#include "JsonHandler.hpp" // Include the new handler

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

// For chrono literals like 500ms
using namespace std::chrono_literals;

/**
 * @brief Flag to control the server's main loop
 */
std::atomic<bool> running(true);

/**
 * @brief Signal handler for graceful termination
 */
void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        std::cout << "\nReceived signal " << signal << ", initiating shutdown..." << std::endl;
        running = false;
    }
}

/**
 * @brief Background thread for updating process statistics and history
 */
void statsUpdateLoop()
{
    using namespace std::chrono_literals;
    auto &proc_core = qnx::ProcessCore::getInstance();    // Get ProcessCore instance
    auto &proc_hist = qnx::ProcessHistory::getInstance(); // Get ProcessHistory instance
    auto &proc_group = qnx::ProcessGroup::getInstance();  // Get ProcessGroup instance

    while (running.load())
    {
        // Collect fresh process info
        if (auto count_opt = proc_core.collectInfo())
        {
            // Update group statistics (uses qnx::exists and qnx::getProcessInfo internally)
            proc_group.updateGroupStats();

            // Update process history
            const auto &process_list = proc_core.getProcessList();
            for (const auto &pinfo : process_list)
            {
                // Call addEntry with individual values
                proc_hist.addEntry(pinfo.getPid(), pinfo.getCpuUsage(), pinfo.getMemoryUsage());
            }
        }
        else
        {
            std::cerr << "Error collecting process info in stats loop." << std::endl;
        }

        // Sleep for the update interval
        std::this_thread::sleep_for(1s); // Use chrono literal
    }
    std::cout << "Stats update loop exiting." << std::endl;
}

/**
 * @brief Main entry point for the application
 */
int main(int argc, char *argv[])
{
    std::cout << "QNX Remote Process Monitor Server Starting..." << std::endl;

    // Setup signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Singletons auto-initialize upon first access (no manual init() needed)

    // Start the background statistics update thread
    std::thread stats_thread(statsUpdateLoop);

    // Initialize and start the socket server (using updated namespace and handler)
    if (!qnx::SocketServer::getInstance().init(8080, qnx::handleMessage))
    {
        std::cerr << "Failed to initialize socket server. Exiting." << std::endl;
        running = false; // Signal stats thread to stop
        if (stats_thread.joinable())
            stats_thread.join();
        // Singletons auto-cleanup on program exit (no manual shutdown())
        return 1;
    }

    std::cout << "Server is running. Waiting for connections..." << std::endl;

    // Wait for shutdown signal
    while (running.load())
    {
        std::this_thread::sleep_for(500ms); // Check more often
    }

    std::cout << "Shutting down server..." << std::endl;

    // Perform clean shutdown (using updated namespaces)
    qnx::SocketServer::getInstance().shutdown();

    // Wait for the stats update thread to finish (ensure running is false)
    if (stats_thread.joinable())
    {
        stats_thread.join();
    }

    // Singletons auto-cleanup on program exit (no manual shutdown())

    std::cout << "Server shut down successfully." << std::endl;

    return 0;
}