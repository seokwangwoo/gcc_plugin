/* array_checker.c — GCC 4.8 plug-in
   Warns about constant out-of-bounds array indices               */

#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "gimple.h"
#include "function.h"
#include "diagnostic-core.h"
#include <cstring>          /* memset */

int plugin_is_GPL_compatible;

/* ───── helper ─────────────────────────────────────────── */
static inline HOST_WIDE_INT
hwi_from_tree (tree t)
{
    /* tree_low_cst(T,1) ⇒ signed (부호 있는; 符号付き) value */
    return (HOST_WIDE_INT) tree_low_cst (t, /*sign=*/1);
}

static bool
const_index_oob (tree array_ref)
{
    tree idx_t = TREE_OPERAND (array_ref, 1);
    if (TREE_CODE (idx_t) != INTEGER_CST)
        return false;                           /* not constant */

    tree arr_type = TREE_TYPE (TREE_OPERAND (array_ref, 0));
    if (TREE_CODE (arr_type) != ARRAY_TYPE)
        return false;                           /* pointer etc. */

    tree dom = TYPE_DOMAIN (arr_type);
    if (!dom) return false;                     /* VLA / unknown */

    HOST_WIDE_INT lo = hwi_from_tree (TYPE_MIN_VALUE (dom));
    HOST_WIDE_INT hi = hwi_from_tree (TYPE_MAX_VALUE (dom));
    HOST_WIDE_INT idx = hwi_from_tree (idx_t);

    return idx < lo || idx > hi;
}

/* ───── tree-walker callback ───────────────────────────── */
static tree
array_ref_cb (tree *tp, int *walk_subtrees,
              void *data ATTRIBUTE_UNUSED)
{
    if (TREE_CODE (*tp) == ARRAY_REF && const_index_oob (*tp))
    {
        warning_at (EXPR_LOCATION (*tp), 0,
                    "possible out-of-bounds array access (index %qE)",
                    TREE_OPERAND (*tp, 1));
    }
    *walk_subtrees = 1;   /* keep walking */
    return NULL_TREE;
}

/* ───── pass body ─────────────────────────────────────── */
static unsigned int
array_pass_execute (void)
{
    basic_block bb;
    FOR_EACH_BB_FN (bb, cfun)
    {
        gimple_stmt_iterator gsi;
        for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
            walk_gimple_stmt (&gsi, array_ref_cb, NULL, NULL); /* &gsi ! */
    }
    return 0;
}

/* ───── opt_pass (old-style) ──────────────────────────── */
static struct opt_pass array_pass;

static void
init_array_pass (void)
{
    std::memset (&array_pass, 0, sizeof array_pass);
    array_pass.type                 = GIMPLE_PASS;
    array_pass.name                 = "arrchk";
    array_pass.execute              = array_pass_execute;
    /* gate==NULL → always run */
}

/* ───── plug-in entry ─────────────────────────────────── */
int
plugin_init (struct plugin_name_args *info,
             struct plugin_gcc_version *ver)
{
    if (!plugin_default_version_check (ver, &gcc_version))
    {
        fprintf (stderr, "%s: incompatible GCC version\n", info->base_name);
        return 1;
    }

    init_array_pass ();

    struct register_pass_info pass_info;
    pass_info.pass                     = &array_pass;
    pass_info.reference_pass_name      = "ssa";
    pass_info.ref_pass_instance_number = 1;
    pass_info.pos_op                   = PASS_POS_INSERT_AFTER;

    register_callback (info->base_name,
                       PLUGIN_PASS_MANAGER_SETUP,
                       NULL, &pass_info);

    return 0;   /* success */
}
