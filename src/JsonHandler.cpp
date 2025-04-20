#include "JsonHandler.hpp"
#include "ProcessControl.hpp"
#include "ProcessGroup.hpp"
#include "ProcessHistory.hpp"
#include "Authenticator.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <sys/json.h> // QNX native JSON library
#include "SocketServer.hpp" // Include for message type constants
#include <functional>
#include <unordered_map>

namespace qnx {

    // Helper function to create a JSON error response using QNX JSON library
    std::string createJsonError(const std::string &error, const std::string &details) {
        json_encoder_t *enc = json_encoder_create();
        json_encoder_start_object(enc, NULL);
        json_encoder_add_string(enc, "status", "error");
        json_encoder_add_string(enc, "message", error.c_str());
        if (!details.empty()) {
            json_encoder_add_string(enc, "details", details.c_str());
        }
        json_encoder_end_object(enc);
        const char *json_str = json_encoder_buffer(enc);
        std::string response(json_str ? json_str : "{\"status\":\"error\",\"message\":\"Encoder error\"}");
        json_encoder_destroy(enc);
        return response;
    }

    // Main message handler using QNX JSON library
    std::string handleMessage(int client_socket, const std::string &message) {
        json_decoder_t *decoder = json_decoder_create();
        json_decoder_error_t status = json_decoder_parse_json_str(decoder, message.c_str());

        if (status != JSON_DECODER_OK) {
            json_decoder_destroy(decoder);
            return createJsonError("Invalid JSON format", json_decoder_error_string(decoder));
        }

        // Helper function to create a JSON error response using QNX JSON library
        std::string createJsonError(const std::string &error, const std::string &details) {
            json_encoder_t *enc = json_encoder_create();
            json_encoder_start_object(enc, NULL);
            json_encoder_add_string(enc, "status", "error");
            json_encoder_add_string(enc, "message", error.c_str());
            if (!details.empty()) {
                json_encoder_add_string(enc, "details", details.c_str());
            }
            json_encoder_end_object(enc);
            const char *json_str = json_encoder_buffer(enc);
            std::string response(json_str ? json_str : "{\"status\":\"error\",\"message\":\"Encoder error\"}");
            json_encoder_destroy(enc);
            return response;
        }

        // Main message handler using QNX JSON library
        std::string handleMessage(int client_socket, const std::string &message) {
            json_decoder_t *decoder = json_decoder_create();
            json_decoder_error_t status = json_decoder_parse_json_str(decoder, message.c_str());

            if (status != JSON_DECODER_OK) {
                json_decoder_destroy(decoder);
                return createJsonError("Invalid JSON format", json_decoder_error_string(decoder));
            }

            json_decoder_push_object(decoder, NULL, false);

            const char *req_type_ptr = NULL;
            if (json_decoder_get_string(decoder, "command", &req_type_ptr, false) != JSON_DECODER_OK || req_type_ptr == NULL) {
                json_decoder_destroy(decoder);
                return createJsonError("Missing or invalid 'command'", "Command must be a string");
            }
            std::string command(req_type_ptr);

            // Parameters are handled within processCommand for QNX JSON
            json_encoder_t *encoder = json_encoder_create(); // Encoder for the response
            std::string response = processCommand(command, message, encoder); // Pass full message and encoder

            json_decoder_destroy(decoder);
            json_encoder_destroy(encoder); // Encoder buffer is managed within processCommand

            return response;
        }

        // Validation function (simple parse check)
        bool validateJson(const std::string &json_str) {
            json_decoder_t *decoder = json_decoder_create();
            bool success = (json_decoder_parse_json_str(decoder, json_str.c_str()) == JSON_DECODER_OK);
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
             std::string response(json_str ? json_str : "{\"status\":\"error\",\"message\":\"Encoder error\"}");
             json_encoder_destroy(enc);
             return response;
        }

        // Command processing using QNX JSON library
        std::string processCommand(const std::string &command, const std::string &raw_params_json, json_encoder_t *encoder) {
             json_decoder_t *decoder = json_decoder_create();
             json_decoder_parse_json_str(decoder, raw_params_json.c_str()); // Parse again to access params
             json_decoder_push_object(decoder, NULL, false);

             json_encoder_start_object(encoder, NULL);
             json_encoder_add_string(encoder, "command", command.c_str());

             try {
                 static const std::unordered_map<std::string, std::function<void(json_decoder_t*, json_encoder_t*)>> handlers = {
                     {"get_processes", [](json_decoder_t* d, json_encoder_t* e){
                         json_encoder_add_string(e, "status", "success");
                         json_encoder_start_array(e, "pids");
                         json_encoder_end_array(e);
                     }},
                     {"get_process_info", [](json_decoder_t* d, json_encoder_t* e){
                         int pid = 0;
                         if (json_decoder_get_int(d, "pid", &pid, false) != JSON_DECODER_OK) {
                             json_encoder_add_string(e, "status", "error");
                             json_encoder_add_string(e, "message", "Missing or invalid 'pid'");
                             return;
                         }
                         json_encoder_add_int(e, "pid", pid);
                         auto info = qnx::getProcessInfo(pid);
                         if (info) {
                             json_encoder_add_string(e, "status", "success");
                             json_encoder_start_object(e, "info");
                             json_encoder_add_double(e, "cpu_usage", info->cpu_usage);
                             json_encoder_add_int_ll(e, "memory_usage", info->memory_usage);
                             json_encoder_end_object(e);
                         } else {
                             json_encoder_add_string(e, "status", "error");
                             json_encoder_add_string(e, "message", "Process not found");
                         }
                     }},
                     {"suspend_process", [](json_decoder_t* d, json_encoder_t* e){ /* suspend handler */ }},
                     {"resume_process",  [](json_decoder_t* d, json_encoder_t* e){ /* resume handler  */ }},
                     {"terminate_process", [](json_decoder_t* d, json_encoder_t* e){ /* terminate handler */ }}
                 };

                 auto it = handlers.find(command);
                 if (it != handlers.end()) {
                     it->second(decoder, encoder);
                 } else {
                     json_encoder_add_string(encoder, "status", "error");
                     json_encoder_add_string(encoder, "message", (std::string("Unknown command: ") + command).c_str());
                 }
             }
             catch (const std::exception &e) {
                 json_encoder_add_string(encoder, "status", "error");
                 json_encoder_add_string(encoder, "message", (std::string("Error processing command: ") + e.what()).c_str());
             }
             
             json_decoder_destroy(decoder);
             json_encoder_end_object(encoder); // End main response object
             const char *json_response = json_encoder_buffer(encoder);
             return std::string(json_response ? json_response : "{\"status\":\"error\",\"message\":\"Encoder error\"}");
        }

        using CommandHandler = std::function<void(json_decoder_t*, json_encoder_t*)>;
        static const std::unordered_map<std::string, CommandHandler> commandHandlers = {
            {"get_processes", [](json_decoder_t* decoder, json_encoder_t* encoder){
                // get_processes handler
                json_encoder_add_string(encoder, "status", "success");
                json_encoder_start_array(encoder, "pids");
                json_encoder_end_array(encoder);
            }},
            {"get_process_info", [](json_decoder_t* decoder, json_encoder_t* encoder){
                int pid = 0;
                if (json_decoder_get_int(decoder, "pid", &pid, false) != JSON_DECODER_OK) {
                    json_encoder_add_string(encoder, "status", "error");
                    json_encoder_add_string(encoder, "message", "Missing or invalid 'pid'");
                    return;
                }
                json_encoder_add_int(encoder, "pid", pid);
                auto process_info = qnx::getProcessInfo(pid);
                if (process_info) {
                    json_encoder_add_string(encoder, "status", "success");
                    json_encoder_start_object(encoder, "info");
                    json_encoder_add_double(encoder, "cpu_usage", process_info->cpu_usage);
                    json_encoder_add_int_ll(encoder, "memory_usage", process_info->memory_usage);
                    json_encoder_end_object(encoder);
                } else {
                    json_encoder_add_string(encoder, "status", "error");
                    json_encoder_add_string(encoder, "message", "Process not found");
                }
            }},
            {"suspend_process", [](json_decoder_t* decoder, json_encoder_t* encoder){
                int pid = 0;
                if (json_decoder_get_int(decoder, "pid", &pid, false) != JSON_DECODER_OK) {
                    json_encoder_add_string(encoder, "status", "error");
                    json_encoder_add_string(encoder, "message", "Missing or invalid 'pid'");
                    return;
                }
                json_encoder_add_int(encoder, "pid", pid);
                bool result = qnx::suspend(pid);
                json_encoder_add_string(encoder, "status", result ? "success" : "error");
                if (!result) json_encoder_add_string(encoder, "message", "Failed to suspend process");
            }},
            {"resume_process", [](json_decoder_t* decoder, json_encoder_t* encoder){
                int pid = 0;
                if (json_decoder_get_int(decoder, "pid", &pid, false) != JSON_DECODER_OK) {
                    json_encoder_add_string(encoder, "status", "error");
                    json_encoder_add_string(encoder, "message", "Missing or invalid 'pid'");
                    return;
                }
                json_encoder_add_int(encoder, "pid", pid);
                bool result = qnx::resume(pid);
                json_encoder_add_string(encoder, "status", result ? "success" : "error");
                if (!result) json_encoder_add_string(encoder, "message", "Failed to resume process");
            }},
            {"terminate_process", [](json_decoder_t* decoder, json_encoder_t* encoder){
                int pid = 0;
                if (json_decoder_get_int(decoder, "pid", &pid, false) != JSON_DECODER_OK) {
                    json_encoder_add_string(encoder, "status", "error");
                    json_encoder_add_string(encoder, "message", "Missing or invalid 'pid'");
                    return;
                }
                json_encoder_add_int(encoder, "pid", pid);
                bool result = qnx::terminate(pid);
                json_encoder_add_string(encoder, "status", result ? "success" : "error");
                if (!result) json_encoder_add_string(encoder, "message", "Failed to terminate process");
            }}
        };
    }
}