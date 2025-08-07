/*  array_checker.c – minimal out-of-bounds checker
    GCC 4.8 plug-in (GPL v2 compatible)                      */

#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "gimple.h"
#include "context.h"
#include "function.h"
#include "diagnostic-core.h"

int plugin_is_GPL_compatible;

/* -------- helper: is constant out of range? -------- */
static bool
const_index_oob (tree array_ref)
{
    tree index  = TREE_OPERAND (array_ref, 1);
    if (TREE_CODE (index) != INTEGER_CST)         /* not a constant */
        return false;

    /* Get array length from its domain (0 … N-1).           */
    tree array_type = TREE_TYPE (TREE_OPERAND (array_ref, 0));
    if (TREE_CODE (array_type) != ARRAY_TYPE)
        return false; /* happens with VLA, pointer casts … */

    tree domain = TYPE_DOMAIN (array_type);
    if (!domain) return false;                     /* unknown size */

    /* min usually 0 for C arrays                               */
    HOST_WIDE_INT lo = tree_to_shwi (TYPE_MIN_VALUE (domain));
    HOST_WIDE_INT hi = tree_to_shwi (TYPE_MAX_VALUE (domain));

    HOST_WIDE_INT idx = tree_to_shwi (index);
    return idx < lo || idx > hi;
}

/* -------- tree walker callback -------- */
static tree
array_ref_cb (tree *tp, int *walk_subtrees, void *data ATTRIBUTE_UNUSED)
{
    if (TREE_CODE (*tp) == ARRAY_REF && const_index_oob (*tp))
    {
        location_t loc = EXPR_LOCATION (*tp);
        warning_at (loc, 0,
            "possible out-of-bounds array access (index %qE)", TREE_OPERAND (*tp,1));
    }