#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <nemotree.h>
#include <mischelper.h>

struct treenode *nemotree_create_node(void)
{
	struct treenode *node;

	node = (struct treenode *)malloc(sizeof(struct treenode));
	if (node == NULL)
		return NULL;
	memset(node, 0, sizeof(struct treenode));

	tree_prepare_one(&node->base);

	return node;
}

void nemotree_destroy_node(struct treenode *node)
{
	tree_finish_one(&node->base);

	free(node);
}

static void nemotree_shift_node(struct treenode *v)
{
	struct treenode *w;
	int shift = 0, change = 0;
	int i;

	for (i = NEMOTREE_CHILDREN_LENGTH(v) - 1; i >= 0; i--) {
		w = NEMOTREE_CHILDREN(v, i);
		w->z = w->z + shift;
		w->m = w->m + shift;

		change = change + w->c;
		shift = shift + w->s + change;
	}
}

static void nemotree_move_node(struct treenode *wm, struct treenode *wp, double shift)
{
	int change = shift / (NEMOTREE_CHILDREN_INDEX(wp) - NEMOTREE_CHILDREN_INDEX(wm));

	wp->c = wp->c - change;
	wp->s = wp->s + shift;
	wm->c = wm->c + change;
	wp->z = wp->z + shift;
	wp->m = wp->m + shift;
}

static int nemotree_separate_node(struct treenode *a, struct treenode *b)
{
	return NEMOTREE_PARENT(a) == NEMOTREE_PARENT(b) ? 1 : 2;
}

static struct treenode *nemotree_ancestor_node(struct treenode *vim, struct treenode *v, struct treenode *a)
{
	return NEMOTREE_PARENT(vim->a) == NEMOTREE_PARENT(v) ? vim->a : a;
}

static struct treenode *nemotree_left_node(struct treenode *v)
{
	return NEMOTREE_CHILDREN_LENGTH(v) > 0 ? NEMOTREE_CHILDREN_FIRST(v) : v->t;
}

static struct treenode *nemotree_right_node(struct treenode *v)
{
	return NEMOTREE_CHILDREN_LENGTH(v) > 0 ? NEMOTREE_CHILDREN_LAST(v) : v->t;
}

static struct treenode *nemotree_apportion_node(struct treenode *v, struct treenode *w, struct treenode *a)
{
	if (w != NULL) {
		struct treenode *vip = v;
		struct treenode *vop = v;
		struct treenode *vim = w;
		struct treenode *vom = NEMOTREE_CHILDREN_FIRST(NEMOTREE_PARENT(vip));
		double sip = vip->m;
		double sop = vop->m;
		double sim = vim->m;
		double som = vom->m;
		double shift;

		vim = nemotree_right_node(vim);
		vip = nemotree_left_node(vip);

		while (vim != NULL && vip != NULL) {
			vom = nemotree_left_node(vom);
			vop = nemotree_right_node(vop);
			vop->a = v;

			shift = vim->z + sim - vip->z - sip + nemotree_separate_node(vim, vip);
			if (shift > 0.0f) {
				nemotree_move_node(nemotree_ancestor_node(vim, v, a), v, shift);

				sip = sip + shift;
				sop = sop + shift;
			}

			sim = sim + vim->m;
			sip = sip + vip->m;
			som = som + vom->m;
			sop = sop + vop->m;

			vim = nemotree_right_node(vim);
			vip = nemotree_left_node(vip);
		}

		if (vim != NULL && nemotree_right_node(vop) == NULL) {
			vop->t = vim;
			vop->m = vop->m + sim - sop;
		}

		if (vip != NULL && nemotree_left_node(vom) == NULL) {
			vom->t = vip;
			vom->m = vom->m + sip - som;
			a = v;
		}
	}

	return a;
}

void nemotree_layout_node(struct treenode *node, int size)
{
	struct treenode *root;
	struct treeone *preorders[size], *postorders[size];
	struct treeone *one;
	struct treenode *v, *w;
	double m;
	int i, npreorders, npostorders;

	root = nemotree_create_node();
	nemotree_attach_node(root, node);

	npreorders = tree_visit_preorder(&node->base, preorders, size);
	npostorders = tree_visit_postorder(&node->base, postorders, size);

	for (i = 0; i < npreorders; i++) {
		v = NEMOTREE_NODE(preorders[i]);

		v->A = NULL;
		v->a = NULL;
		v->t = NULL;
		v->z = 0.0f;
		v->m = 0.0f;
		v->c = 0.0f;
		v->s = 0.0f;
	}

	for (i = 0; i < npostorders; i++) {
		v = NEMOTREE_NODE(postorders[i]);
		w = NEMOTREE_CHILDREN_INDEX(v) > 0 ? NEMOTREE_CHILDREN(NEMOTREE_PARENT(v), NEMOTREE_CHILDREN_INDEX(v) - 1) : NULL;

		if (NEMOTREE_CHILDREN_LENGTH(v) > 0) {
			nemotree_shift_node(v);

			m = (NEMOTREE_CHILDREN_FIRST(v)->z + NEMOTREE_CHILDREN_LAST(v)->z) / 2.0f;
			if (w != NULL) {
				v->z = w->z + nemotree_separate_node(v, w);
				v->m = v->z - m;
			} else {
				v->z = m;
			}
		} else if (w != NULL) {
			v->z = w->z + nemotree_separate_node(v, w);
		}

		NEMOTREE_PARENT(v)->A = nemotree_apportion_node(v, w, NEMOTREE_PARENT(v)->A != NULL ? NEMOTREE_PARENT(v)->A : NEMOTREE_CHILDREN_FIRST(NEMOTREE_PARENT(v)));
	}

	for (i = 0; i < npreorders; i++) {
		v = NEMOTREE_NODE(preorders[i]);

		v->y = v->base.depth;
		v->x = v->z + NEMOTREE_PARENT(v)->m;
		v->m = v->m + NEMOTREE_PARENT(v)->m;
	}

	nemotree_detach_node(root, node);
	nemotree_destroy_node(root);
}
