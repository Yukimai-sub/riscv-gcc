#include <vector>
#include <map>
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "lto-streamer.h"
#include "fold-const.h"
#include "varasm.h"
#include "output.h"
#include "graph.h"
#include "debug.h"
#include "cfgloop.h"
#include "value-prof.h"
#include "tree-cfg.h"
#include "tree-ssa-loop-manip.h"
#include "tree-into-ssa.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "tree-pass.h"
#include "plugin.h"
#include "ipa-utils.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "context.h"
#include "pass_manager.h"
#include "cfgrtl.h"
#include "tree-ssa-live.h"  /* For remove_unused_locals.  */
#include "tree-cfgcleanup.h"
#include "insn-addr.h" /* for INSN_ADDRESSES_ALLOC.  */
#include "diagnostic-core.h" /* for fnotice */
#include "stringpool.h"
#include "attribs.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "cfc.h"

pass_cfcss::pass_cfcss(): gimple_opt_pass({
    GIMPLE_PASS,
    "cfcss",
    OPTGROUP_NONE,
    TV_INTEGRATION,
    0,
    0,
    0,
    0,
    0
  }, new gcc::context) {
    sub = nullptr;
    next = nullptr;
    static_pass_number = 0;
  }

opt_pass *pass_cfcss::clone() {
    return this;
}

bool pass_cfcss::gate(function *fun)
{
    return true;
}

gimple_opt_pass *make_pass_cfcss(gcc::context *ctxt) {
    return new pass_cfcss();
}

static tree attr_handler(tree *node, tree name, tree args, int flags, bool *no_add_attrs) {
    // will never be called
    return NULL_TREE;
}


static attribute_spec attr_cfc = { "cfcheck", 0, 1, false,  false, false, attr_handler };

void cfc_register_attribute()
{
    register_attribute(&attr_cfc);
}

static int get_func_cfc_attr(tree func_decl)
{
    tree a = lookup_attribute("cfcheck", DECL_ATTRIBUTES(func_decl));
    if(a == NULL_TREE) return CFC_ATTR_UNDEFINED;
    a = TREE_VALUE(a);
    //fprintf(stderr, "found cfc attribute on function %s\n", DECL_NAME(func_decl)->identifier.id.str);
    if(a && TREE_CODE(TREE_VALUE(a)) != VOID_TYPE)
    {
        // enable if arg is a true-y value
        if(TREE_INT_CST_ELT(TREE_VALUE(a), 0))
            return CFC_ATTR_ENABLE;
        else 
            return CFC_ATTR_DISABLE;
    }else
    {
        //fprintf(stderr, "Enable by default\n");
        return CFC_ATTR_ENABLE;
    }
}

static bool is_cfc_enabled(function *func)
{
  int attr = get_func_cfc_attr(func->decl);
  bool ret = global_options.x_cfc_enable ? attr != CFC_ATTR_DISABLE : attr == CFC_ATTR_ENABLE;
  if(ret && global_options.x_flag_lto)
    warning(OPT_Wpragmas, "CF Check is disabled because %<-flto%> is present");
  return ret && !global_options.x_flag_lto;
}

unsigned int pass_cfcss::execute(function *fun) {
  // The signatures of basic blocks.
  std::map<basic_block, cfcss_sig_t> sig;

  // Signature differences.
  std::map<basic_block, cfcss_sig_t> diff;

  // Call statements.
  std::vector<gimple *> call_stmts;

  // Conditional branches before adjusting signature assignments for
  // fall-through multi-fan-in successors.
  std::vector<gimple *> fall_thru_sigs;

  // A temporary accumulator.
  cfcss_sig_t acc = 0;

  // Adjusting signature values.
  std::map<basic_block, cfcss_sig_t> dmap;

  // Adjusting signature values for fall-through multi-fan-in successors.
  std::map<basic_block, cfcss_sig_t> dmap_fall_thru;

  // Basic block.
  basic_block bb;

  // Instruction string buffer.
  char inst[64];

  // Whether a tail call is encountered.
  bool is_tail_call;

  if(!is_cfc_enabled(fun)) return 0;

  push_cfun(fun);

  FOR_EACH_BB_FN (bb, fun) {
    // Find all the call statements. The basic blocks are to be split after
    // those statements because subroutine calls can bring changes to the CRC
    // signature.
    for (auto gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi))
      if (gimple_code(gsi_stmt(gsi)) == GIMPLE_CALL
          && gsi.ptr != gsi_last_nondebug_bb(bb).ptr
          && !gimple_call_tail_p((const gcall *)gsi_stmt(gsi)))
        call_stmts.push_back(gsi_stmt(gsi));
  }

  for (auto &stmt : call_stmts)
      split_block(stmt->bb, stmt);

  FOR_EACH_BB_FN (bb, fun) {
    // Naïve approach to assign signatures.
    sig[bb] = acc++;
  }


  FOR_EACH_BB_FN (bb, fun) {
    if (EDGE_COUNT(bb->preds) == 1) {
      diff[bb] = sig[(*bb->preds)[0]->src] ^ sig[bb];
    } else if (EDGE_COUNT(bb->preds) >= 2) {
      basic_block base_pred = nullptr;
      for (edge pred_edge : *bb->preds) {
        base_pred = pred_edge->src;
        break;
      }

      diff[bb] = sig[base_pred] ^ sig[bb];
    
      for (edge pred_edge : *bb->preds) {
        // D[i, m] = s[i, 1] XOR s[i, m]
        dmap[pred_edge->src] = sig[pred_edge->src] ^ sig[base_pred];
      }
    } else {
      diff[bb] = sig[bb];
    }
  }

  FOR_EACH_BB_FN (bb, fun) {
    // A second adjusting signature has to be assigned when
    // (a) Both successors are multi-fan-in basic blocks, and
    // (b) The base predecessor of each successor is different.
    if (EDGE_COUNT(bb->succs) == 2
        && EDGE_COUNT((*bb->succs)[0]->dest->preds) > 1
        && EDGE_COUNT((*bb->succs)[1]->dest->preds) > 1
        && (*(*bb->succs)[0]->dest->preds)[0]->src
          != (*(*bb->succs)[1]->dest->preds)[0]->src) {
      auto gsi = gsi_last_bb(bb);

      // No fallthru edges are allowed according to the semantics of
      // gimple_cond. Here, we simply assume the 1-indexed one is the
      // fallthru edge.
      basic_block fallthru_succ = (*bb->succs)[1]->dest;
      fall_thru_sigs.push_back(gsi_stmt(gsi));
      auto br_target = (*bb->succs)[0]->dest;
      dmap[bb] = sig[bb] ^ sig[(*br_target->preds)[0]->src];
    }
  }

  for (auto stmt : fall_thru_sigs) {
    fprintf(stderr, "Control flow checking note: SPECIAL CASE\n");
    auto pred_bb = stmt->bb;
    auto orig_edge = (*pred_bb->succs)[1];
    auto succ_bb = orig_edge->dest;
    cfcss_sig_t dmap_val = sig[pred_bb] ^ sig[(*succ_bb->preds)[0]->src];
    bb = split_edge(orig_edge);
    sig[bb] = sig[pred_bb];
    diff[bb] = 0;
    dmap[bb] = dmap_val;
  }


  FOR_EACH_BB_FN (bb, fun) {
    auto gsi = gsi_start_nondebug_after_labels_bb(bb);
    gasm *stmt = nullptr;

    cfcss_sig_t cur_sig = sig[bb];
    cfcss_sig_t cur_diff = diff[bb];
    cfcss_sig_t cur_adj = dmap.find(bb) != dmap.end() ? dmap[bb] : 0;

    if (EDGE_COUNT(bb->preds) >= 2) {
      sprintf(inst, "ctrlsig_m %d,%d,%d", cur_diff, cur_sig, cur_adj);
      stmt = gimple_build_asm_vec(inst,
                                  nullptr, nullptr, nullptr, nullptr);
    } else {
      sprintf(inst, "ctrlsig_s %d,%d,%d", cur_diff, cur_sig, cur_adj);
      stmt = gimple_build_asm_vec(inst,
                                  nullptr, nullptr, nullptr, nullptr);
    }
    gimple_asm_set_volatile(stmt, true);
    gsi_insert_before(&gsi, stmt, GSI_NEW_STMT);

    if (EDGE_COUNT(bb->preds) > 0
        && (*bb->preds)[0]->src == fun->cfg->x_entry_block_ptr) {
      stmt = gimple_build_asm_vec(
        "pushsig",
        nullptr, nullptr, nullptr, nullptr
      );
      gsi_insert_before(&gsi, stmt, GSI_SAME_STMT);
      gimple_asm_set_volatile(stmt, true);
      gimple_set_modified(stmt, false);
    }

    gsi = gsi_last_nondebug_bb(bb);
    is_tail_call = false;
    if (gimple_code(gsi_stmt(gsi)) == GIMPLE_CALL
        && gimple_call_tail_p((const gcall *)gsi_stmt(gsi)))
      is_tail_call = true;
    if (gimple_code(gsi_stmt(gsi)) == GIMPLE_COND
        || gimple_code(gsi_stmt(gsi)) == GIMPLE_CALL
        || gimple_code(gsi_stmt(gsi)) == GIMPLE_RETURN
        || gimple_code(gsi_stmt(gsi)) == GIMPLE_GOTO
        || gimple_code(gsi_stmt(gsi)) == GIMPLE_SWITCH)
      gsi_prev_nondebug(&gsi);
    if (gimple_code(gsi_stmt(gsi)) == GIMPLE_CALL
        && gimple_call_tail_p((const gcall *)gsi_stmt(gsi))) {
      is_tail_call = true;
      gsi_prev_nondebug(&gsi);
    }
    sprintf(inst, "crcsig 0x%x # <bb %d>",
            ((uint64_t)fun + bb->index) & 0xffff, bb->index);
    stmt = gimple_build_asm_vec(
      inst,
      nullptr, nullptr, nullptr, nullptr
    );
    gsi_insert_after(&gsi, stmt, GSI_NEW_STMT);
    gimple_asm_set_volatile(stmt, true);
    gimple_set_modified(stmt, false);

    if ((EDGE_COUNT(bb->succs) > 0
          && (*bb->succs)[0]->dest == fun->cfg->x_exit_block_ptr)
        || is_tail_call) {
      stmt = gimple_build_asm_vec(
        "popsig",
        nullptr, nullptr, nullptr, nullptr
      );
      gsi_insert_after(&gsi, stmt, GSI_SAME_STMT);
      gimple_asm_set_volatile(stmt, true);
      gimple_set_modified(stmt, false);
    }
  }

  pop_cfun();

  return 0;
}
