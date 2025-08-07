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
    /* continue walking */
    *walk_subtrees = 1;
    return NULL_TREE;
}

/* -------- the actual pass -------- */
static unsigned int
execute_array_pass (void)
{
    basic_block bb;
    FOR_EACH_BB_FN (bb, cfun)
    {
        gimple_stmt_iterator gsi;
        for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
        {
            gimple *stmt = gsi_stmt (gsi);
            walk_gimple_stmt (stmt, array_ref_cb, NULL /* data */);
        }
    }
    return 0;
}

/* -------- pass definition -------- */
namespace {

const pass_data array_pass_data =
{
    GIMPLE_PASS, /* type */
    "arrchk",    /* name */
    OPTGROUP_NONE,
    TV_NONE,
    0,           /* properties required  */
    0,           /* properties provided */
    0,           /* properties destroyed */
    0,           /* todo flags start     */
    0            /* todo flags finish    */
};

struct array_bounds_pass : gimple_opt_pass
{
    array_bounds_pass (gcc::context *ctx)
        : gimple_opt_pass (array_pass_data, ctx)  {}

    unsigned int execute (function *) override
    { return execute_array_pass (); }
}; // struct

} // anonymous namespace

/* -------- plug-in entry point -------- */
int
plugin_init (struct plugin_name_args *info, struct plugin_gcc_version *ver)
{
    /* very small version guard */
    if (!plugin_default_version_check (ver, &gcc_version))
    {
        fprintf (stderr, "array_checker: wrong GCC version\n");
        return 1;
    }

    struct register_pass_info pass_info;
    pass_info.pass                     = new array_bounds_pass (g);
    pass_info.reference_pass_name      = "ssa"; /* after SSA-construction */
    pass_info.ref_pass_instance_number = 1;
    pass_info.pos_op                  = PASS_POS_INSERT_AFTER;

    register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP,
                       NULL, &pass_info);

    return 0; /* success */
}
