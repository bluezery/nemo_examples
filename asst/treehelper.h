#ifndef	__TREE_HELPER_H__
#define	__TREE_HELPER_H__

#include <stdint.h>

#include <mischelper.h>

#define TREE_CHILDREN_DEFAULT_SIZE		(8)

struct treeone {
	struct treeone *parent;
	int ichildren;

	struct treeone **children;
	int nchildren, schildren;

	uint32_t depth;
	uint32_t id;
};

extern int tree_prepare_one(struct treeone *one);
extern void tree_finish_one(struct treeone *one);

extern struct treeone *tree_create_one(void);
extern void tree_destroy_one(struct treeone *one);

extern int tree_visit_preorder(struct treeone *one, struct treeone **ones, int size);
extern int tree_visit_postorder(struct treeone *one, struct treeone **ones, int size);

static inline void tree_attach_one(struct treeone *one, struct treeone *child)
{
	child->parent = one;
	child->ichildren = one->nchildren;
	child->depth = one->depth + 1;

	ARRAY_APPEND(one->children, one->schildren, one->nchildren, child);
}

static inline void tree_detach_one(struct treeone *one, struct treeone *child)
{
	child->parent = NULL;

	ARRAY_REMOVE(one->children, one->nchildren, child->ichildren);
}

static inline struct treeone *tree_search_one(struct treeone *one, uint32_t id)
{
	int i;

	for (i = 0; i < one->nchildren; i++) {
		if (one->children[i]->id == id)
			return one->children[i];
	}

	return NULL;
}

#endif
