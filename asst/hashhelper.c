#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <hashhelper.h>
#include <codehelper.h>

static uint64_t hash_get_tag(struct hashcontext *context, uint64_t key)
{
	key += (key << 12);
	key ^= (key >> 22);
	key += (key << 4);
	key ^= (key >> 9);
	key += (key << 10);
	key ^= (key >> 2);
	key += (key << 7);
	key ^= (key >> 12);

	key = (key >> 3) * 2654435761;

	return key % context->tablesize;
}

struct hashcontext *hash_create(int chainlength)
{
	struct hashcontext *context;

	context = (struct hashcontext *)malloc(sizeof(struct hashcontext));
	if (context == NULL)
		return NULL;

	context->tablesize = HASH_INITIAL_SIZE;
	context->chainlength = HASH_CHAIN_LENGTH_MIN > chainlength ? HASH_CHAIN_LENGTH_MIN : chainlength;
	context->nodecount = 0;

	context->nodes = (struct hashnode *)malloc(sizeof(struct hashnode) * context->tablesize);
	if (context->nodes == NULL)
		goto err1;
	memset(context->nodes, 0, sizeof(struct hashnode) * context->tablesize);

	return context;

err1:
	free(context);

	return NULL;
}

void hash_destroy(struct hashcontext *context)
{
	free(context->nodes);
	free(context);
}

int hash_length(struct hashcontext *context)
{
	return context->nodecount;
}

int hash_clear(struct hashcontext *context)
{
	free(context->nodes);

	context->nodes = (struct hashnode *)malloc(sizeof(struct hashnode) * context->tablesize);
	if (context->nodes == NULL)
		return -1;
	memset(context->nodes, 0, sizeof(struct hashnode) * context->tablesize);

	context->nodecount = 0;

	return 0;
}

static int hash_expand(struct hashcontext *context)
{
	struct hashnode *cnodes;
	struct hashnode *tnodes;
	struct hashnode *node;
	int oldsize;
	int i;

	tnodes = (struct hashnode *)malloc(sizeof(struct hashnode) * context->tablesize * 2);
	if (tnodes == NULL)
		return -1;
	memset(tnodes, 0, sizeof(struct hashnode) * context->tablesize * 2);

	cnodes = context->nodes;
	context->nodes = tnodes;

	oldsize = context->tablesize;
	context->tablesize = context->tablesize * 2;
	context->nodecount = 0;

	for (i = 0; i < oldsize; i++) {
		node = &cnodes[i];

		if (node->used != 0) {
			hash_set_value(context, node->key, node->value);
		}
	}

	free(cnodes);

	return 0;
}

void hash_iterate(struct hashcontext *context, void (*iterate)(void *data, uint64_t key, uint64_t value), void *data)
{
	int i;

	for (i = 0; i < context->tablesize; i++) {
		if (context->nodes[i].used != 0) {
			iterate(data, context->nodes[i].key, context->nodes[i].value);
		}
	}
}

int hash_set_value(struct hashcontext *context, uint64_t key, uint64_t value)
{
	struct hashnode *node;
	uint64_t tag;
	int i;

retry:
	if (context->nodecount >= context->tablesize / 2)
		hash_expand(context);

	tag = hash_get_tag(context, key);

	for (i = 0; i < context->chainlength; i++) {
		node = &context->nodes[tag];

		if ((node->used == 0) ||
				(node->used != 0 && node->key == key)) {
			node->key = key;
			node->value = value;
			node->used = 1;

			context->nodecount++;

			return 1;
		}

		tag = (tag + 1) % context->tablesize;
	}

	if (hash_expand(context) == 0)
		goto retry;

	return 0;
}

int hash_get_value(struct hashcontext *context, uint64_t key, uint64_t *value)
{
	uint64_t tag;
	int i;

	tag = hash_get_tag(context, key);

	for (i = 0; i < context->chainlength; i++) {
		if (context->nodes[tag].used != 0 && context->nodes[tag].key == key) {
			*value = context->nodes[tag].value;
			return 1;
		}

		tag = (tag + 1) % context->tablesize;
	}

	return 0;
}

uint64_t hash_get_value_easy(struct hashcontext *context, uint64_t key)
{
	uint64_t tag;
	int i;

	tag = hash_get_tag(context, key);

	for (i = 0; i < context->chainlength; i++) {
		if (context->nodes[tag].used != 0 && context->nodes[tag].key == key) {
			return context->nodes[tag].value;
		}

		tag = (tag + 1) % context->tablesize;
	}

	return 0;
}

int hash_put_value(struct hashcontext *context, uint64_t key)
{
	struct hashnode *node;
	uint64_t tag;
	int i;

	tag = hash_get_tag(context, key);

	for (i = 0; i < context->chainlength; i++) {
		node = &context->nodes[tag];

		if (node->used != 0 && node->key == key) {
			node->key = 0;
			node->value = 0;
			node->used = 0;

			context->nodecount--;

			return 1;
		}

		tag = (tag + 1) % context->tablesize;
	}

	return 0;
}

int hash_put_value_all(struct hashcontext *context, uint64_t value)
{
	int i;

	for (i = 0; i < context->tablesize; i++) {
		if (context->nodes[i].used != 0 && context->nodes[i].value == value) {
			struct hashnode *node = &context->nodes[i];

			node->key = 0;
			node->value = 0;
			node->used = 0;

			context->nodecount--;
		}
	}

	return 0;
}

int hash_set_value_with_string(struct hashcontext *context, const char *str, uint64_t value)
{
	uint64_t key = crc32_from_string(str);

	return hash_set_value(context, key, value);
}

int hash_get_value_with_string(struct hashcontext *context, const char *str, uint64_t *value)
{
	uint64_t key = crc32_from_string(str);

	return hash_get_value(context, key, value);
}

uint64_t hash_get_value_easy_with_string(struct hashcontext *context, const char *str)
{
	if (str != NULL) {
		uint64_t key = crc32_from_string(str);

		return hash_get_value_easy(context, key);
	}

	return 0;
}

int hash_put_value_with_string(struct hashcontext *context, const char *str)
{
	uint64_t key = crc32_from_string(str);

	return hash_put_value(context, key);
}
