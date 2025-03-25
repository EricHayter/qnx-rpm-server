#define _DEFAULT_SOURCE

#include "auth.h"

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

char *generate_salt()
{
    char *result = (char*)malloc(SALT_LENGTH + 1); // Allocate memory for the result
	if(result == NULL)
		return NULL;

	unsigned long current_time = time(NULL);
	for (int i = SALT_LENGTH - 1; i >= 0 ; i--)
	{
		result[i] = VALID_CHARS[current_time & 0b111111];
		current_time >>= 6;
	}

	result[SALT_LENGTH] = '\0';
    return result;
}


char *generate_hash(char * const password, char * const salt)
{
	// create the buffer
	char *salt_buffer = (char*)malloc(strlen(salt) + SHA_256_PREFIX_LENGTH + 1);
	if (salt_buffer == NULL)
		return NULL;

	// copy over the prefix
	strcpy(salt_buffer, SHA_256_PREFIX);
	strcpy(salt_buffer + SHA_256_PREFIX_LENGTH, salt);

	char *result = crypt(password, salt_buffer);
	free(salt_buffer);
	return result;
}

bool validate_password(char * const password, char * const salt, char * const hash)
{
	// allocate a buffer for the size.	
	return false;
}

