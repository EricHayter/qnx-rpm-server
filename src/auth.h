#pragma once

#include <stdbool.h>

static const int SALT_LENGTH = sizeof(unsigned long) * 8 / 6;

// These are the allowable character for crypt(). There are 64 in total
// or 2^6.
static const char * const VALID_CHARS = "0123456789"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"./";

// crypt may optionally allow a prefix on the salt parameter to specifiy
// the hashing method instead of DES. 5 in this case specifies SHA-256
static const char * const SHA_256_PREFIX = "$5$";
static const int SHA_256_PREFIX_LENGTH = 3;

// Length of the resultant hash
static const int SHA_256_HASH_LENGTH = 43;

char *generate_salt();

char *generate_hash(char * const password, char * const salt);

bool validate_password(char * const password, char * const salt, char * const hash);
