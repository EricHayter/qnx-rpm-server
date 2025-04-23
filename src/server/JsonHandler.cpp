#include "JsonHandler.hpp"
#include "Authenticator.hpp"
#include "ProcessControl.hpp"
#include "ProcessCore.hpp" // Added for ProcessCore & ProcessInfo
#include "ProcessGroup.hpp"
#include "ProcessHistory.hpp"
#include "SocketServer.hpp" // Include for message type constants
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/json.h> // QNX native JSON library
#include <utility>    // For std::make_pair

namespace qnx {
using CommandHandler = std::function<void(json_decoder_t *, json_encoder_t *)>;

// --- Command Handler Functions ---

void handleGetProcesses(json_decoder_t *decoder, json_encoder_t *encoder) {
  json_encoder_add_string(encoder, "status", "success");
  json_encoder_start_array(encoder, "pids");
  // TODO: Populate PIDs from ProcessCore
  const auto &processList = ProcessCore::getInstance().getProcessList();
  for (const auto &pinfo : processList) {
    json_encoder_add_int(encoder, NULL, pinfo.pid); // Add PID to array
  }
  json_encoder_end_array(encoder);
}

void handleGetProcessInfo(json_decoder_t *decoder, json_encoder_t *encoder) {
  int pid_int = 0;
  if (json_decoder_get_int(decoder, "pid", &pid_int, false) !=
      JSON_DECODER_OK) {
    json_encoder_add_string(encoder, "status", "error");
    json_encoder_add_string(encoder, "message", "Missing or invalid 'pid'");
    return;
  }
  pid_t pid = static_cast<pid_t>(pid_int); // Cast to pid_t
  json_encoder_add_int(encoder, "pid", pid);

  // Use ProcessCore to get info
  if (auto info_opt = qnx::ProcessCore::getInstance().getProcessById(pid)) {
    const auto &info = *info_opt; // Dereference optional
    json_encoder_add_string(encoder, "status", "success");
    json_encoder_start_object(encoder, "info");
    // Use members from ProcessInfo struct
    json_encoder_add_string(encoder, "name", info.name.c_str());
    json_encoder_add_double(encoder, "cpu_usage", info.cpu_usage);
    json_encoder_add_int_ll(encoder, "memory_usage_kb",
                            info.memory_usage /
                                1024); // Use memory_usage (bytes) and convert
    json_encoder_add_int(encoder, "threads", info.num_threads);
    json_encoder_add_int(encoder, "priority", info.priority);
    json_encoder_add_int(encoder, "state", info.state); // Raw state code
    // Parent PID, UID, GID are not currently in ProcessInfo struct
    // json_encoder_add_int(encoder, "parent_pid", info.parent_pid);
    // json_encoder_add_int(encoder, "uid", info.uid);
    // json_encoder_add_int(encoder, "gid", info.gid);
    json_encoder_end_object(encoder);
  } else {
    json_encoder_add_string(encoder, "status", "error");
    json_encoder_add_string(encoder, "message", "Process not found");
  }
}

void handleSuspendProcess(json_decoder_t *decoder, json_encoder_t *encoder) {
  int pid = 0;
  if (json_decoder_get_int(decoder, "pid", &pid, false) != JSON_DECODER_OK) {
    json_encoder_add_string(encoder, "status", "error");
    json_encoder_add_string(encoder, "message", "Missing or invalid 'pid'");
    return;
  }
  json_encoder_add_int(encoder, "pid", pid);
  bool result = suspend(pid);
  json_encoder_add_string(encoder, "status", result ? "success" : "error");
  if (!result)
    json_encoder_add_string(encoder, "message", "Failed to suspend process");
}

void handleResumeProcess(json_decoder_t *decoder, json_encoder_t *encoder) {
  int pid = 0;
  if (json_decoder_get_int(decoder, "pid", &pid, false) != JSON_DECODER_OK) {
    json_encoder_add_string(encoder, "status", "error");
    json_encoder_add_string(encoder, "message", "Missing or invalid 'pid'");
    return;
  }
  json_encoder_add_int(encoder, "pid", pid);
  bool result = resume(pid);
  json_encoder_add_string(encoder, "status", result ? "success" : "error");
  if (!result)
    json_encoder_add_string(encoder, "message", "Failed to resume process");
}

void handleTerminateProcess(json_decoder_t *decoder, json_encoder_t *encoder) {
  int pid = 0;
  if (json_decoder_get_int(decoder, "pid", &pid, false) != JSON_DECODER_OK) {
    json_encoder_add_string(encoder, "status", "error");
    json_encoder_add_string(encoder, "message", "Missing or invalid 'pid'");
    return;
  }
  json_encoder_add_int(encoder, "pid", pid);
  bool result = terminate(pid);
  json_encoder_add_string(encoder, "status", result ? "success" : "error");
  if (!result)
    json_encoder_add_string(encoder, "message", "Failed to terminate process");
}

// --- End Command Handler Functions ---

// Function to initialize the command handlers map
std::map<std::string, CommandHandler> initializeCommandHandlers() {
  std::map<std::string, CommandHandler> handlers;

  // Assign function pointers to the map
  handlers["get_processes"] = handleGetProcesses;
  handlers["get_process_info"] = handleGetProcessInfo;
  handlers["suspend_process"] = handleSuspendProcess;
  handlers["resume_process"] = handleResumeProcess;
  handlers["terminate_process"] = handleTerminateProcess;

  return handlers;
}

// Global map of command handlers - initialized by function
static const std::map<std::string, CommandHandler> commandHandlers =
    initializeCommandHandlers();

// Helper function to create a JSON error response using QNX JSON library
std::string createJsonError(const std::string &error,
                            const std::string &details) {
  json_encoder_t *enc = json_encoder_create();
  json_encoder_start_object(enc, NULL);
  json_encoder_add_string(enc, "status", "error");
  json_encoder_add_string(enc, "message", error.c_str());
  if (!details.empty()) {
    json_encoder_add_string(enc, "details", details.c_str());
  }
  json_encoder_end_object(enc);
  const char *json_str = json_encoder_buffer(enc);
  std::string response(
      json_str ? json_str
               : "{\"status\":\"error\",\"message\":\"Encoder error\"}");
  json_encoder_destroy(enc);
  return response;
}

// Main message handler using QNX JSON library
std::string handleMessage(int client_socket, const std::string &message) {
  json_decoder_t *decoder = json_decoder_create();
  json_decoder_error_t status =
      json_decoder_parse_json_str(decoder, message.c_str());

  if (status != JSON_DECODER_OK) {
    int err_pos;
    const char *err_str;
    json_decoder_get_parse_error(decoder, &err_pos, &err_str);
    json_decoder_destroy(decoder);
    return createJsonError("Invalid JSON format", err_str);
  }

  json_decoder_push_object(decoder, NULL, false);

  const char *req_type_ptr = NULL;
  if (json_decoder_get_string(decoder, "command", &req_type_ptr, false) !=
          JSON_DECODER_OK ||
      req_type_ptr == NULL) {
    json_decoder_destroy(decoder);
    return createJsonError("Missing or invalid 'command'",
                           "Command must be a string");
  }
  std::string command(req_type_ptr);

  json_encoder_t *encoder = json_encoder_create();
  std::string response = processCommand(command, message, encoder);

  json_decoder_destroy(decoder);
  json_encoder_destroy(encoder);

  return response;
}

// Validation function (simple parse check)
bool validateJson(const std::string &json_str) {
  json_decoder_t *decoder = json_decoder_create();
  bool success = (json_decoder_parse_json_str(decoder, json_str.c_str()) ==
                  JSON_DECODER_OK);
  json_decoder_destroy(decoder);
  return success;
}

// toJson (less relevant now, but kept for potential internal use)
std::string toJson(const std::string &data) {
  json_encoder_t *enc = json_encoder_create();
  json_encoder_start_object(enc, NULL);
  json_encoder_add_string(enc, "data", data.c_str());
  json_encoder_end_object(enc);
  const char *json_str = json_encoder_buffer(enc);
  std::string response(
      json_str ? json_str
               : "{\"status\":\"error\",\"message\":\"Encoder error\"}");
  json_encoder_destroy(enc);
  return response;
}

// Command processing using QNX JSON library
std::string processCommand(const std::string &command,
                           const std::string &raw_params_json,
                           json_encoder_t *encoder) {
  json_decoder_t *decoder = json_decoder_create();
  json_decoder_error_t parse_status =
      json_decoder_parse_json_str(decoder, raw_params_json.c_str());

  if (parse_status != JSON_DECODER_OK) {
    json_decoder_destroy(decoder);
    json_encoder_start_object(encoder, NULL);
    json_encoder_add_string(encoder, "command", command.c_str());
    json_encoder_add_string(encoder, "status", "error");
    json_encoder_add_string(encoder, "message",
                            "Failed to parse parameters JSON");
    json_encoder_end_object(encoder);
    const char *json_response = json_encoder_buffer(encoder);
    json_encoder_destroy(encoder);
    return std::string(
        json_response ? json_response
                      : "{\"status\":\"error\",\"message\":\"Encoder error\"}");
  }

  json_decoder_push_object(decoder, NULL, false);

  json_encoder_start_object(encoder, NULL);

  try {
    // Dispatch using global commandHandlers map
    auto it = commandHandlers.find(command);
    if (it != commandHandlers.end()) {
      it->second(decoder, encoder);
    } else {
      json_encoder_add_string(encoder, "status", "error");
      json_encoder_add_string(
          encoder, "message",
          (std::string("Unknown command: ") + command).c_str());
    }
  } catch (const std::exception &e) {
    if (json_encoder_nesting_level(encoder) > 0) {
      json_encoder_add_string(encoder, "status", "error");
      json_encoder_add_string(
          encoder, "message",
          (std::string("Error processing command: ") + e.what()).c_str());
    } else {
      json_encoder_reset(encoder, 0);
      json_encoder_start_object(encoder, NULL);
      json_encoder_add_string(encoder, "command", command.c_str());
      json_encoder_add_string(encoder, "status", "error");
      json_encoder_add_string(
          encoder, "message",
          (std::string("Error processing command: ") + e.what()).c_str());
    }
  }

  json_decoder_destroy(decoder);
  json_encoder_end_object(encoder);
  const char *json_response = json_encoder_buffer(encoder);
  std::string final_response = std::string(
      json_response ? json_response
                    : "{\"status\":\"error\",\"message\":\"Encoder error\"}");
  json_encoder_destroy(encoder);
  return final_response;
}
} // namespace qnx