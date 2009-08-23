/* #pragma	lib	"libavl.a" */
/* #pragma src "/sys/src/libavl" */

AUTOLIB(avl)

typedef struct Avl	Avl;
typedef struct Avltree	Avltree;
typedef struct Avlwalk	Avlwalk;

/* #pragma incomplete Avltree */
/* #pragma incomplete Avlwalk */

struct Avl
{
	Avl	*p;		/* parent */
	Avl	*n[2];		/* children */
	int	bal;		/* balance bits */
};

Avl	*avlnext(Avlwalk *walk);
Avl	*avlprev(Avlwalk *walk);
Avlwalk	*avlwalk(Avltree *tree);
void	deleteavl(Avltree *tree, Avl *key, Avl **oldp);
void	endwalk(Avlwalk *walk);
void	insertavl(Avltree *tree, Avl *new, Avl **oldp);
Avl	*lookupavl(Avltree *tree, Avl *key);
Avltree	*mkavltree(int(*cmp)(Avl*, Avl*));
