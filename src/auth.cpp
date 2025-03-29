#define _DEFAULT_SOURCE


#include "auth.h"

#include <ctime>
#include <unistd.h>
#include <sqlite_orm/sqlite_orm.h>
#include <string_view>

std::string generate_salt()
{
	constexpr int SALT_LENGTH = sizeof(unsigned long) * 8 / 6;
	std::string salt(SALT_LENGTH, '\0');

	unsigned long current_time = time(NULL);
	for (int i = salt.size() - 1; i >= 0 ; i--)
	{
		salt[i] = VALID_CHARS[current_time & 0b111111];
		current_time >>= 6;
	}
	return salt;
}


std::string generate_hash(const std::string &password, const std::string &salt)
{
	const std::string SHA_256_PREFIX = "$5$";
	std::string sha256_salt = SHA_256_PREFIX + salt;	
	return std::string(crypt(password.c_str(), sha256_salt.c_str()));
}

