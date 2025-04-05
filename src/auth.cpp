#define _DEFAULT_SOURCE

#include "auth.h"

#include <ctime>
#include <unistd.h>
#include <fstream>
#include <string_view>
#include <charconv>

namespace Authenticator
{


std::optional<UserEntry> UserEntry::FromString(std::string_view line)
{
	// get username
	std::size_t field_start = 0;
	std::size_t field_end = line.find(":");		
	if (field_end == std::string::npos)
		return {};
	const std::string_view username = line.substr(0, field_end);

	// get hash
	field_start = field_end + 1;
	field_end = line.find(":", field_start);
	if (field_end == std::string::npos)
		return {};
	const std::string_view hash = line.substr(field_start, field_end - field_start);

	// get salt
	field_start = field_end + 1;
	field_end = line.find(":", field_start);
	if (field_end == std::string::npos)
		return {};
	const std::string_view salt = line.substr(field_start, field_end - field_start);

	// get user type (viewer or admin)
	field_start = field_end + 1;
	if (field_end == std::string::npos)
		return {};
	const std::string_view type_str = line.substr(field_start);
	int type_value{};
	std::from_chars(type_str.data(), type_str.data() + type_str.size(), type_value);
	if (0 > type_value || type_value > 1)
		return {};
	UserType type = static_cast<UserType>(type_value);

	return UserEntry{
		std::string(username),
		std::string(hash),
		std::string(salt),
		type
	};
}

bool ValidateLogin(std::string_view username, std::string_view password)
{
	if (not std::filesystem::exists(LOGIN_FILE))
		return false;

	std::ifstream fstream(LOGIN_FILE);
	if (not fstream)
		return false;

	std::string line;
	while (std::getline(fstream, line)) {
		auto user_entry = UserEntry::FromString(line);		
		if (not user_entry.has_value())
			continue;
		
		if (generate_hash(password, user_entry->salt) == user_entry->hash)
			return true;
	}
	return false;
}


std::string generate_hash(std::string_view password, std::string_view salt)
{
	constexpr int SHA256_HASH_LENGTH = 43;

	// crypt requires that salts are prefixed with '$5$' to hash with SHA256
	std::string sha256_salt = "$5$";
	sha256_salt += salt;
	std::string result(crypt(std::string(password).c_str(), sha256_salt.c_str()));

	// we need to get the last 43 characters of the string that contains only the SHA256 hash.
	return result.substr(result.size() - SHA256_HASH_LENGTH);
}


std::string generate_salt()
{
	constexpr int SALT_LENGTH = sizeof(unsigned long) * 8 / 6;
	const char * const VALID_CHARS = "0123456789"
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"./";

	std::string salt(SALT_LENGTH, '\0');

	unsigned long current_time = time(nullptr);
	for (int i = salt.size() - 1; i >= 0 ; i--)
	{
		salt[i] = VALID_CHARS[current_time & 0b111111];
		current_time >>= 6;
	}
	return salt;
}

}
