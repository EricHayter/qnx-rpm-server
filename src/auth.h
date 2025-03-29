#pragma once

#include <string>

// These are the allowable character for crypt(). There are 64 in total
// or 2^6.
static const char * const VALID_CHARS = "0123456789"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"./";


std::string generate_salt();


bool validate_password(char * const password, char * const salt, char * const hash);
