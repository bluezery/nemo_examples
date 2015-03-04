#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <treehelper.h>

int tree_prepare_one(struct treeone *one)
{
	one->parent = NULL;
	one->ichildren = 0;

	one->children = (struct treeone **)malloc(sizeof(struct treeone *) * TREE_CHILDREN_DEFAULT_SIZE);
	if (one->children == NULL)
		return -1;
	memset(one->children, 0, sizeof(struct treeone *) * TREE_CHILDREN_DEFAULT_SIZE);

	one->nchildren = 0;
	one->schildren = TREE_CHILDREN_DEFAULT_SIZE;

	one->id = 0;

	return 0;
}

void tree_finish_one(struct treeone *one)
{
	free(one->children);
}

struct treeone *tree_create_one(void)
{
	struct treeone *one;

	one = (struct treeone *)malloc(sizeof(struct treeone));
	if (one == NULL)
		return NULL;

	tree_prepare_one(one);

	return one;
}

void tree_destroy_one(struct treeone *one)
{
	tree_finish_one(one);

	free(one);
}

int tree_visit_preorder(struct treeone *one, struct treeone **ones, int size)
{
	struct treeone *child;
	int count = 0, index = size;
	int i;

	ones[--index] = one;

	while ((index < size) && (one = ones[index++]) != NULL) {
		ones[count++] = one;

		for (i = one->nchildren - 1; i >= 0; i--) {
			ones[--index] = one->children[i];
		}
	}

	return count;
}

int tree_visit_postorder(struct treeone *one, struct treeone **ones, int size)
{
	struct treeone *child;
	int count = size, index = 0;
	int i;

	ones[index++] = one;

	while ((index > 0) && (one = ones[--index]) != NULL) {
		ones[--count] = one;

		for (i = 0; i < one->nchildren; i++) {
			ones[index++] = one->children[i];
		}
	}

	for (i = count; i < size; i++) {
		ones[i - count] = ones[i];
	}

	return size - count;
}
