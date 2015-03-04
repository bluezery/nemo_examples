#ifndef	__HASH_HELPER_H__
#define	__HASH_HELPER_H__

#include <stdint.h>

#define	HASH_INITIAL_SIZE			(256)
#define	HASH_CHAIN_LENGTH_MIN	(32)

struct hashnode {
	uint64_t key;
	int used;
	uint64_t value;
};

struct hashcontext {
	int tablesize;
	int chainlength;
	int nodecount;

	struct hashnode *nodes;
};

extern struct hashcontext *hash_create(int chainlength);
extern void hash_destroy(struct hashcontext *context);

extern int hash_length(struct hashcontext *context);
extern int hash_clear(struct hashcontext *context);

extern void hash_iterate(struct hashcontext *context, void (*iterate)(void *data, uint64_t key, uint64_t value), void *data);

extern int hash_set_value(struct hashcontext *context, uint64_t key, uint64_t value);
extern int hash_get_value(struct hashcontext *context, uint64_t key, uint64_t *value);
extern uint64_t hash_get_value_easy(struct hashcontext *context, uint64_t key);
extern int hash_put_value(struct hashcontext *context, uint64_t key);
extern int hash_put_value_all(struct hashcontext *context, uint64_t value);

extern int hash_set_value_with_string(struct hashcontext *context, const char *str, uint64_t value);
extern int hash_get_value_with_string(struct hashcontext *context, const char *str, uint64_t *value);
extern uint64_t hash_get_value_easy_with_string(struct hashcontext *context, const char *str);
extern int hash_put_value_with_string(struct hashcontext *context, const char *str);

#endif
