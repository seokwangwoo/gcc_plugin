/* array_checker.c — GCC 4.8 plug-in
   Warns about constant out-of-bounds array indices               */

#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "gimple.h"
#include "function.h"
#include "diagnostic-core.h"
#include "tree-vrp.h"      /* get_range_info */
#include "gimple-iterator.h"
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

/* Return true if the access is surely out-of-bounds */
static bool
ssa_chain_oob (tree idx_ssa, tree array_type)
{
    HOST_WIDE_INT k = 0;           /* cumulative constant offset   */
    tree ssa = idx_ssa;

    /* ── A. Walk back through SSA definitions ── */
    while (TREE_CODE (ssa) == SSA_NAME)
    {
        gimple *def = SSA_NAME_DEF_STMT (ssa);

        if (!def || !is_gimple_assign (def))
            break;

        enum tree_code rhs_code = gimple_assign_rhs_code (def);

        /* i_2 = i_1 ± C  ? */
        if ((rhs_code == PLUS_EXPR || rhs_code == MINUS_EXPR) &&
            TREE_CODE (gimple_assign_rhs2 (def)) == INTEGER_CST)
        {
            HOST_WIDE_INT c =
                tree_low_cst (gimple_assign_rhs2 (def), /*sign*/1);

            k += (rhs_code == PLUS_EXPR ? c : -c);
            ssa = gimple_assign_rhs1 (def);   /* older SSA - continue */
            continue;
        }
        break;          /* definition not of the form we can fold */
    }

    /* ── B. Combine VRP range of *ssa* with offset k ── */
    value_range_type vrtype;
    tree vr_lo, vr_hi;
    vrtype = get_range_info (ssa, &vr_lo, &vr_hi);
    if (vrtype != VR_RANGE)
        return false;              /* range unknown → give up */

    HOST_WIDE_INT lo = tree_low_cst (vr_lo,1) + k;
    HOST_WIDE_INT hi = tree_low_cst (vr_hi,1) + k;

    tree dom = TYPE_DOMAIN (array_type);
    if (!dom) return false;        /* unknown array size */

    HOST_WIDE_INT alo = tree_low_cst (TYPE_MIN_VALUE (dom),1);
    HOST_WIDE_INT ahi = tree_low_cst (TYPE_MAX_VALUE (dom),1);

    return (hi < alo || lo > ahi); /* definitely o-o-b */
}


/* ───── tree-walker callback ───────────────────────────── */
static tree
array_ref_cb (tree *tp, int *walk_subtrees,
              void *data ATTRIBUTE_UNUSED)
{
    if (TREE_CODE (*tp) == ARRAY_REF )
    {
        if (const_index_oob (*tp)) {
            warning_at (EXPR_LOCATION (*tp), 0,
                        "possible out-of-bounds array access (index %qE)",
                        TREE_OPERAND (*tp, 1));
        }

        tree arr = TREE_OPERAND (*tp, 0);
        tree idx = TREE_OPERAND (*tp, 1);

        if (TREE_CODE (idx) == SSA_NAME &&      /* variable index */
            ssa_chain_oob (idx, TREE_TYPE (arr)))
        {
            warning_at (EXPR_LOCATION (*tp), 0,
                        "index may be out of bounds after previous subtraction");
        }
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
    pass_info.reference_pass_name      = "cfg";
    pass_info.ref_pass_instance_number = 0;
    pass_info.pos_op                   = PASS_POS_INSERT_AFTER;

    register_callback (info->base_name,
                       PLUGIN_PASS_MANAGER_SETUP,
                       NULL, &pass_info);

    return 0;   /* success */
}
