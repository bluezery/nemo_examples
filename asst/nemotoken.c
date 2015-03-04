#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <nemotoken.h>

struct nemotoken *nemotoken_create(const char *str, int length)
{
	struct nemotoken *token;

	token = (struct nemotoken *)malloc(sizeof(struct nemotoken));
	if (token == NULL)
		return NULL;
	memset(token, 0, sizeof(struct nemotoken));

	token->contents = (char *)malloc(length + 1);
	if (token->contents == NULL)
		goto err1;
	memcpy(token->contents, str, length);

	token->contents[length] = '\0';

	token->length = length;
	token->tokens = NULL;
	token->token_count = 0;

	return token;

err1:
	free(token);

	return NULL;
}

void nemotoken_destroy(struct nemotoken *token)
{
	if (token->tokens != NULL)
		free(token->tokens);

	free(token->contents);
	free(token);
}

void nemotoken_divide(struct nemotoken *token, char div)
{
	int i;

	for (i = 0; i < token->length; i++) {
		if (token->contents[i] == div) {
			token->contents[i] = '\0';
		}
	}
}

int nemotoken_update(struct nemotoken *token)
{
	int i, state, index;

	if (token->tokens != NULL)
		free(token->tokens);

	token->token_count = 0;

	state = 0;

	for (i = 0; i < token->length + 1; i++) {
		if (token->contents[i] == '\0') {
			if (state == 1) {
				token->token_count++;
			}

			state = 0;
		} else {
			state = 1;
		}
	}

	token->tokens = (char **)malloc(sizeof(char *) * (token->token_count + 1));
	if (token->tokens == NULL)
		return -1;
	memset(token->tokens, 0, sizeof(char *) * (token->token_count + 1));

	state = 0;
	index = 0;

	for (i = 0; i < token->length + 1; i++) {
		if (token->contents[i] == '\0') {
			state = 0;
		} else {
			if (state == 0) {
				token->tokens[index++] = &token->contents[i];
			}

			state = 1;
		}
	}

	return 0;
}

int nemotoken_get_token_count(struct nemotoken *token)
{
	return token->token_count;
}

char **nemotoken_get_tokens(struct nemotoken *token)
{
	return token->tokens;
}

char *nemotoken_get_token(struct nemotoken *token, int index)
{
	return token->tokens[index];
}

int nemotoken_parse_decimal(const char *str, int offset, int length)
{
	uint32_t number = 0;
	int sign = 1;
	int has_number = 0;
	int i;

	for (i = offset; i < offset + length; i++) {
		switch (str[i]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				number = (number * 10) + str[i] - '0';
				has_number = 1;
				break;

			case '-':
				if (has_number != 0)
					goto out;
				sign = -1;
				break;

			default:
				if (has_number != 0)
					goto out;
				break;
		}
	}

out:
	if (has_number != 0)
		return sign * number;

	return 0;
}

int nemotoken_parse_decimal_with_endptr(const char *str, int offset, int length, const char **endptr)
{
	uint32_t number = 0;
	int sign = 1;
	int has_number = 0;
	int i;

	for (i = offset; i < offset + length; i++) {
		switch (str[i]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				number = (number * 10) + str[i] - '0';
				has_number = 1;
				break;

			case '-':
				if (has_number != 0)
					goto out;
				sign = -1;
				break;

			default:
				if (has_number != 0)
					goto out;
				break;
		}
	}

out:
	if (has_number != 0) {
		*endptr = &str[i];

		return sign * number;
	}

	return 0;
}

int nemotoken_parse_hexadecimal(const char *str, int offset, int length)
{
	uint32_t number = 0;
	int sign = 1;
	int has_number = 0;
	int i;

	for (i = offset; i < offset + length; i++) {
		switch (str[i]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				number = (number * 16) + str[i] - '0';
				has_number = 1;
				break;

			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
				number = (number * 16) + str[i] - 'a' + 10;
				has_number = 1;
				break;

			case '-':
				if (has_number != 0)
					goto out;
				sign = -1;
				break;

			default:
				if (has_number != 0)
					goto out;
				break;
		}
	}

out:
	if (has_number != 0)
		return sign * number;

	return 0;
}

double nemotoken_parse_float(const char *str, int offset, int length)
{
	uint32_t number = 0;
	uint32_t fraction = 0;
	int sign = 1;
	int has_number = 0;
	int has_fraction = 0;
	int fraction_cipher = 1;
	int i;

	for (i = offset; i < offset + length; i++) {
		switch (str[i]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (has_fraction == 0) {
					number = (number * 10) + str[i] - '0';
				} else {
					fraction = (fraction * 10) + str[i] - '0';
					fraction_cipher = fraction_cipher * 10;
				}
				has_number = 1;
				break;

			case '.':
				has_fraction = 1;
				break;

			case '-':
				if (has_number != 0)
					goto out;
				sign = -1;
				break;

			default:
				if (has_number != 0)
					goto out;
				break;
		}
	}

out:
	if (has_number != 0)
		return sign * ((double)number + (double)fraction / (double)fraction_cipher);

	return 0.0f;
}

double nemotoken_parse_float_with_endptr(const char *str, int offset, int length, const char **endptr)
{
	uint32_t number = 0;
	uint32_t fraction = 0;
	int sign = 1;
	int has_number = 0;
	int has_fraction = 0;
	int fraction_cipher = 1;
	int i;

	for (i = offset; i < offset + length; i++) {
		switch (str[i]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (has_fraction == 0) {
					number = (number * 10) + str[i] - '0';
				} else {
					fraction = (fraction * 10) + str[i] - '0';
					fraction_cipher = fraction_cipher * 10;
				}
				has_number = 1;
				break;

			case '.':
				has_fraction = 1;
				break;

			case '-':
				if (has_number != 0)
					goto out;
				sign = -1;
				break;

			default:
				if (has_number != 0)
					goto out;
				break;
		}
	}

out:
	if (has_number != 0) {
		*endptr = &str[i];

		return sign * ((double)number + (double)fraction / (double)fraction_cipher);
	}

	return 0.0f;
}

const char *nemotoken_find_alphabet(const char *str, int offset, int length)
{
	int i;

	for (i = offset; i < offset + length; i++) {
		if (('a' <= str[i] && str[i] <= 'z') ||
				('A' <= str[i] && str[i] <= 'Z'))
			return &str[i];
	}

	return NULL;
}

const char *nemotoken_find_number(const char *str, int offset, int length)
{
	int i;

	for (i = offset; i < offset + length; i++) {
		if ('0' <= str[i] && str[i] <= '9')
			return &str[i];
	}

	return NULL;
}
