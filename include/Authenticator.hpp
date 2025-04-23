/**
 * @file Authenticator.hpp
 * @brief Authentication system for the QNX Remote Process Monitor
 *
 * This file defines the authentication framework for the RPM server,
 * including user types, credential storage, and validation functions.
 * The system uses a file-based authentication method with salted password
 * hashing for security.
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

/**
 * @namespace qnx
 * @brief Contains all authentication-related functionality for the RPM server
 */
namespace qnx::Authentication {
/**
 * @brief Path to the login credentials file (constructed via filesystem::path /
 * operator)
 */
inline static const std::filesystem::path LOGIN_FILE =
    std::filesystem::path("/etc") / "rpm_login";

/**
 * @enum UserType
 * @brief Defines the privilege levels for users of the system
 */
enum UserType {
  VIEWER, ///< User can only view running processes
  ADMIN,  ///< User can view, suspend, resume, and terminate processes
};

/**
 * @struct UserEntry
 * @brief Represents a single user's credentials and permissions
 *
 * This structure stores the parsed information from a line in the
 * login file, including username, password hash, salt, and user type.
 */
struct UserEntry {
  std::string username; ///< User's login name
  std::string hash;     ///< Hashed password
  std::string salt;     ///< Salt used for password hashing
  UserType type;        ///< User's permission level

  /**
   * @brief Parse a line from the login file into a UserEntry
   * @param line A string view containing username:hash:salt:type
   * @return An optional containing the UserEntry if parsing was successful
   */
  static std::optional<UserEntry> FromString(std::string_view line);
};

/**
 * @brief Authenticate user login credentials
 * @param username The username to check
 * @param password The password to validate
 * @return An optional containing the UserType on success, or std::nullopt if
 * authentication fails
 */
std::optional<UserType> ValidateLogin(std::string_view username,
                                      std::string_view password);

/**
 * @brief Generate a password hash using the provided salt
 * @param password The plain-text password to hash
 * @param salt The salt to use for hashing
 * @return An optional containing the hashed password string if successful,
 *         std::nullopt otherwise (e.g., if crypt() fails).
 */
std::optional<std::string> generate_hash(std::string_view password,
                                         std::string_view salt);

/**
 * @brief Generate a new random salt for password hashing
 *
 * Creates a salt suitable for use with the crypt() function (SHA-512 with
 * PBKDF2 format). Relies on time() and rand() for pseudo-randomness. Does not
 * have internal error checking for these functions.
 * @return A string containing the generated salt.
 */
std::string generate_salt();
} // namespace qnx::Authentication
