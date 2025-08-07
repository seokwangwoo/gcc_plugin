/*  array_checker.c – GCC 4.8 plug-in that warns on
    constant out-of-bounds array indices                */

#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "gimple.h"
#include "function.h"
#include "diagnostic-core.h"

int plugin_is_GPL_compatible;

/* ─── helper ─────────────────────────────────────────── */
static bool
const_index_oob (tree array_ref)
{
    tree idx_tree = TREE_OPERAND (array_ref, 1);
    if (TREE_CODE (idx_tree) != INTEGER_CST)
        return false;                              /* not a constant */

    tree arr_type = TREE_TYPE (TREE_OPERAND (array_ref, 0));
    if (TREE_CODE (arr_type) != ARRAY_TYPE)
        return false;

    tree dom = TYPE_DOMAIN (arr_type);
    if (!dom) return false;                        /* unknown size */

    HOST_WIDE_INT lo = tree_to_shwi (TYPE_MIN_VALUE (dom));
    HOST_WIDE_INT hi = tree_to_shwi (TYPE_MAX_VALUE (dom));
    HOST_WIDE_INT idx = tree_to_shwi (idx_tree);

    return idx < lo || idx > hi;
}

/* ─── tree-walker callback ───────────────────────────── */
static tree
array_ref_cb (tree *tp, int *walk_subtrees, void *data ATTRIBUTE_UNUSED)
{
    if (TREE_CODE (*tp) == ARRAY_REF && const_index_oob (*tp))
    {
        warning_at (EXPR_LOCATION (*tp), 0,
                    "possible out-of-bounds array access (index %qE)",
                    TREE_OPERAND (*tp, 1));
    }
    *walk_subtrees = 1;          /* keep walking */
    return NULL_TREE;
}

/* ─── pass execute function ─────────────────────────── */
static unsigned int
array_pass_execute (void)
{
    basic_block bb;
    FOR_EACH_BB_FN (bb, cfun)
    {
        gimple_stmt_iterator gsi;
        for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
            walk_gimple_stmt (gsi_stmt (gsi), array_ref_cb, NULL);
    }
    return 0;
}

/* ─── pass data (old style) ─────────────────────────── */
static struct opt_pass array_pass =
{
    .type                     = GIMPLE_PASS,
    .name                     = "arrchk",
    .gate                     = NULL,          /* always run   */
    .execute                  = array_pass_execute,
    .sub                      = NULL,
    .next                     = NULL,
    .static_pass_number       = 0,
    .tv_id                    = TV_NONE,
    .properties_required      = 0,
    .properties_provided      = 0,
    .properties_destroyed     = 0,
    .todo_flags_start         = 0,
    .todo_flags_finish        = 0
};

/* ─── plug-in entry ─────────────────────────────────── */
int
plugin_init (struct plugin_name_args *info,
             struct plugin_gcc_version *ver)
{
    if (!plugin_default_version_check (ver, &gcc_version))
    {
        fprintf (stderr, "%s: incompatible GCC version\n", info->base_name);
        return 1;
    }

    struct register_pass_info pass_info = {
        .pass                     = &array_pass,
        .reference_pass_name      = "ssa",
        .ref_pass_instance_number = 1,
        .pos_op                   = PASS_POS_INSERT_AFTER
    };

    register_callback (info->base_name,
                       PLUGIN_PASS_MANAGER_SETUP,
                       NULL, &pass_info);

    return 0;   /* success */
}
