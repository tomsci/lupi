#ifndef CTYPE_H
#define CTYPE_H

#include <stddef.h>

static inline bool isdigit(char c) {
	return c >= '0' && c <= '9';
}

static inline bool islower(char c) {
	return (c >= 'a' && c <= 'z');
}

static inline bool isupper(char c) {
	return (c >= 'A' && c <= 'Z');
}

static inline bool isalpha(char c) {
	return islower(c) || isupper(c);
}

static inline bool isalnum(char c) {
	return isalpha(c) || isdigit(c);
}

static inline bool isgraph(char c) {
	return c > ' ' && c <= '~';
}

static inline bool ispunct(char c) {
	return isgraph(c) && !isalnum(c);
}

static inline bool isspace(char c) {
	switch (c) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '\v':
		case '\f':
			return true;
		default:
			return false;
	}
}

static inline bool iscntrl(char c) {
	return (c < ' ' || c == 127);
}

static inline bool isxdigit(char c) {
	return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline char toupper(char c) {
	if (islower(c)) return (c - 'a') + 'A';
	else return c;
}

static inline char tolower(char c) {
	if (isupper(c)) return (c - 'A') + 'a';
	else return c;
}


#endif
