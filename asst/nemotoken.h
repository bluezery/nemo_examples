#ifndef	__NEMO_TOKEN_H__
#define	__NEMO_TOKEN_H__

#include <stdint.h>

struct nemotoken {
	char *contents;
	int length;

	char **tokens;
	int token_count;
};

extern struct nemotoken *nemotoken_create(const char *str, int length);
extern void nemotoken_destroy(struct nemotoken *token);

extern void nemotoken_divide(struct nemotoken *token, char div);
extern int nemotoken_update(struct nemotoken *token);

extern int nemotoken_get_token_count(struct nemotoken *token);
extern char **nemotoken_get_tokens(struct nemotoken *token);
extern char *nemotoken_get_token(struct nemotoken *token, int index);

extern int nemotoken_parse_decimal(const char *str, int offset, int length);
extern int nemotoken_parse_decimal_with_endptr(const char *str, int offset, int length, const char **endptr);
extern int nemotoken_parse_hexadecimal(const char *str, int offset, int length);
extern double nemotoken_parse_float(const char *str, int offset, int length);
extern double nemotoken_parse_float_with_endptr(const char *str, int offset, int length, const char **endptr);

extern const char *nemotoken_find_alphabet(const char *str, int offset, int length);
extern const char *nemotoken_find_number(const char *str, int offset, int length);

static inline int nemotoken_is_alphabet(const char *str, int offset, int length)
{
	int i;

	for (i = offset; i < offset + length; i++) {
		if (('a' <= str[i] && str[i] <= 'z') ||
				('A' <= str[i] && str[i] <= 'Z'))
			continue;

		return 0;
	}

	return 1;
}

static inline int nemotoken_is_number(const char *str, int offset, int length)
{
	int i;

	for (i = offset; i < offset + length; i++) {
		if ('0' <= str[i] && str[i] <= '9')
			continue;

		return 0;
	}

	return 1;
}

#endif
