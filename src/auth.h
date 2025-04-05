#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <filesystem>

namespace Authenticator
{
	const std::filesystem::path LOGIN_FILE = "/etc/rpm_login";

	// Specifies the privileges that users have
	enum UserType {
		VIEWER, // can view running processes
		ADMIN, // can view and suspend running processes
	};

	struct UserEntry {
		std::string username;
		std::string hash;
		std::string salt;
		UserType type;	

		static std::optional<UserEntry> FromString(std::string_view line);
	};


	bool ValidateLogin(std::string_view username, std::string_view password);
	std::string generate_hash(std::string_view password, std::string_view salt);
	std::string generate_salt();
}

