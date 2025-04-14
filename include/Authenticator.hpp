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

#include <string>
#include <string_view>
#include <optional>
#include <filesystem>

/**
 * @namespace Authenticator
 * @brief Contains all authentication-related functionality for the RPM server
 */
namespace Authenticator
{
	/**
	 * @brief Path to the login credentials file
	 *
	 * This file stores user entries in the format:
	 * username:hash:salt:type
	 */
	const std::filesystem::path LOGIN_FILE = "/etc/rpm_login";

	/**
	 * @enum UserType
	 * @brief Defines the privilege levels for users of the system
	 */
	enum UserType
	{
		VIEWER, ///< User can only view running processes
		ADMIN,	///< User can view, suspend, resume, and terminate processes
	};

	/**
	 * @struct UserEntry
	 * @brief Represents a single user's credentials and permissions
	 *
	 * This structure stores the parsed information from a line in the
	 * login file, including username, password hash, salt, and user type.
	 */
	struct UserEntry
	{
		std::string username; ///< User's login name
		std::string hash;	  ///< Hashed password
		std::string salt;	  ///< Salt used for password hashing
		UserType type;		  ///< User's permission level

		/**
		 * @brief Parse a line from the login file into a UserEntry
		 * @param line A string view containing username:hash:salt:type
		 * @return An optional containing the UserEntry if parsing was successful
		 */
		static std::optional<UserEntry> FromString(std::string_view line);
	};

	/**
	 * @brief Validate user login credentials
	 * @param username The username to check
	 * @param password The password to validate
	 * @return true if credentials are valid, false otherwise
	 */
	bool ValidateLogin(std::string_view username, std::string_view password);

	/**
	 * @brief Generate a password hash using the provided salt
	 * @param password The plain-text password to hash
	 * @param salt The salt to use for hashing
	 * @return A string containing the hashed password
	 */
	std::string generate_hash(std::string_view password, std::string_view salt);

	/**
	 * @brief Generate a new random salt for password hashing
	 * @return A string containing the generated salt
	 */
	std::string generate_salt();
}
