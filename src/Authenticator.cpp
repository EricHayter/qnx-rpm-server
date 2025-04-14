/**
 * @file Authenticator.cpp
 * @brief Implementation of the authentication system for the QNX Remote Process Monitor
 *
 * This file implements the authentication functions declared in Authenticator.hpp,
 * providing user login validation, password hashing, and salt generation for
 * securing the RPM server.
 */

#define _DEFAULT_SOURCE

#include "Authenticator.hpp"

#include <ctime>
#include <unistd.h>
#include <fstream>
#include <string_view>
#include <charconv>
#include <numeric>	 // For std::accumulate
#include <algorithm> // For std::transform
#include <sstream>	 // For stringstream
#include <iomanip>	 // For setw, setfill

// Forward declaration for crypt
extern "C" char *crypt(const char *key, const char *salt);

namespace Authenticator
{

	/**
	 * @brief Parse a user entry from a line in the login file
	 *
	 * Parses a colon-delimited line from the login file with format:
	 * username:hash:salt:type
	 *
	 * @param line A string_view containing the line to parse
	 * @return An optional containing the UserEntry if parsing was successful
	 */
	std::optional<UserEntry> UserEntry::FromString(std::string_view line)
	{
		// Extract username (field 1)
		std::size_t field_start = 0;
		std::size_t field_end = line.find(":");
		if (field_end == std::string::npos)
			return std::nullopt;
		const std::string_view username = line.substr(0, field_end);

		// Extract password hash (field 2)
		field_start = field_end + 1;
		field_end = line.find(":", field_start);
		if (field_end == std::string::npos)
			return std::nullopt;
		const std::string_view hash = line.substr(field_start, field_end - field_start);

		// Extract salt (field 3)
		field_start = field_end + 1;
		field_end = line.find(":", field_start);
		if (field_end == std::string::npos)
			return std::nullopt;
		const std::string_view salt = line.substr(field_start, field_end - field_start);

		// Extract user type: 0=VIEWER, 1=ADMIN (field 4)
		field_start = field_end + 1;
		if (field_end == std::string::npos)
			return std::nullopt;
		const std::string_view type_str = line.substr(field_start);
		int type_value{};
		std::from_chars(type_str.data(), type_str.data() + type_str.size(), type_value);
		if (0 > type_value || type_value > 1)
			return std::nullopt;
		UserType type = static_cast<UserType>(type_value);

		// Create the UserEntry and return it wrapped in optional
		std::optional<UserEntry> result;
		result.emplace(); // Create an empty UserEntry inside the optional
		result->username = std::string(username.data(), username.size());
		result->hash = std::string(hash.data(), hash.size());
		result->salt = std::string(salt.data(), salt.size());
		result->type = type;
		return result;
	}

	/**
	 * @brief Validate a user's login credentials
	 *
	 * Opens the login file and compares the provided username and password
	 * against stored entries. The password is hashed with the stored salt
	 * before comparison.
	 *
	 * @param username The username to validate
	 * @param password The password to validate
	 * @return true if credentials match an entry, false otherwise
	 */
	bool ValidateLogin(std::string_view username, std::string_view password)
	{
		// Check if the login file exists
		if (not std::filesystem::exists(LOGIN_FILE))
			return false;

		// Open the login file
		std::ifstream fstream(LOGIN_FILE);
		if (not fstream)
			return false;

		// Check each line in the file for matching credentials
		std::string line;
		while (std::getline(fstream, line))
		{
			auto user_entry = UserEntry::FromString(line);
			if (not user_entry.has_value())
				continue;

			// Generate hash from provided password and compare to stored hash
			if (generate_hash(password, user_entry->salt) == user_entry->hash)
				return true;
		}
		return false;
	}

	/**
	 * @brief Generate a password hash using the provided salt
	 *
	 * Uses the system's crypt() function to hash a password with the
	 * provided salt. This creates a secure, one-way hash suitable for
	 * password storage.
	 *
	 * @param password The password to hash
	 * @param salt The salt to use in hashing
	 * @return The generated hash as a string
	 */
	std::string generate_hash(std::string_view password, std::string_view salt)
	{
		// Convert string_views to C-style strings for crypt()
		std::string pwd_str(password);
		std::string salt_str(salt);

		// Use crypt function from liblogin
		char *result = crypt(pwd_str.c_str(), salt_str.c_str());
		if (!result)
		{
			// Fallback in case crypt fails
			return std::string();
		}

		return std::string(result);
	}

	/**
	 * @brief Generate a random salt for password hashing
	 *
	 * Creates a salt suitable for use with the crypt() function.
	 * The salt starts with "@S@X@" which specifies SHA-512 with PBKDF2,
	 * followed by random characters.
	 *
	 * @return A randomly generated salt as a string
	 */
	std::string generate_salt()
	{
		constexpr int SALT_LENGTH = 16; // Sufficient for SHA-256/SHA-512
		const char *const VALID_CHARS = "0123456789"
										"abcdefghijklmnopqrstuvwxyz"
										"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
										"./";

		std::string salt = "@S@X@"; // Format for SHA-512 with PBKDF2

		// Use time as a seed, then mix with random values
		unsigned long current_time = time(nullptr);
		for (int i = 0; i < SALT_LENGTH; i++)
		{
			salt.push_back(VALID_CHARS[current_time & 0b111111]);
			current_time = (current_time >> 6) | (rand() << 10);
		}

		return salt;
	}

}
