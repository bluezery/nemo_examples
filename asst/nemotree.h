#ifndef	__NEMO_TREE_H__
#define	__NEMO_TREE_H__

#include <treehelper.h>
#include <mischelper.h>

struct treenode {
	struct treeone base;

	double x, y;
	void *userdata;

	struct treenode *p, *A, *a, *t;
	double z, m, c, s;
};

#define NEMOTREE_NODE(one)							((struct treenode *)container_of(one, struct treenode, base))
#define NEMOTREE_PARENT(node)						(node != NULL && node->base.parent != NULL ? (struct treenode *)container_of(node->base.parent, struct treenode, base) : NULL)
#define NEMOTREE_CHILDREN(node, index)	((struct treenode *)container_of(node->base.children[index], struct treenode, base))
#define NEMOTREE_CHILDREN_FIRST(node)		((struct treenode *)container_of(node->base.children[0], struct treenode, base))
#define NEMOTREE_CHILDREN_LAST(node)		((struct treenode *)container_of(node->base.children[node->base.nchildren - 1], struct treenode, base))
#define	NEMOTREE_CHILDREN_INDEX(node)		(node->base.ichildren)
#define NEMOTREE_CHILDREN_LENGTH(node)	(node->base.nchildren)

extern struct treenode *nemotree_create_node(void);
extern void nemotree_destroy_node(struct treenode *node);

extern void nemotree_layout_node(struct treenode *node, int size);

static inline void nemotree_set_node_depth(struct treenode *node, uint32_t depth)
{
	node->base.depth = depth;
}

static inline uint32_t nemotree_get_node_depth(struct treenode *node)
{
	return node->base.depth;
}

static inline void nemotree_set_node_id(struct treenode *node, uint32_t id)
{
	node->base.id = id;
}

static inline uint32_t nemotree_get_node_id(struct treenode *node)
{
	return node->base.id;
}

static inline void nemotree_attach_node(struct treenode *node, struct treenode *child)
{
	tree_attach_one(&node->base, &child->base);
}

static inline void nemotree_detach_node(struct treenode *node, struct treenode *child)
{
	tree_detach_one(&node->base, &child->base);
}

static inline struct treenode *nemotree_search_node(struct treenode *node, uint32_t id)
{
	struct treeone *one;

	one = tree_search_one(&node->base, id);
	if (one != NULL)
		return (struct treenode *)container_of(one, struct treenode, base);

	return NULL;
}

#endif
