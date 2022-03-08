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

pass_cfcss::pass_cfcss(): rtl_opt_pass({
    RTL_PASS,
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

rtl_opt_pass *make_pass_cfcss(gcc::context *ctxt) {
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
  return global_options.x_cfc_enable ? attr != CFC_ATTR_DISABLE : attr == CFC_ATTR_ENABLE;
}

unsigned int pass_cfcss::execute(function *fun) {
  // The signatures of basic blocks.
  std::map<basic_block, cfcss_sig_t> sig;

  // Signature differences.
  std::map<basic_block, cfcss_sig_t> diff;

  // Call sites.
  std::vector<std::pair<rtx_call_insn *, basic_block>> call_sites;

  // Conditional branches before adjusting signature assignments for
  // fall-through multi-fan-in successors.
  std::vector<edge> fall_thru_sigs;
  // std::vector<gimple *> fall_thru_sigs;

  // A temporary accumulator.
  cfcss_sig_t acc = 0;

  // Adjusting signature values.
  std::map<basic_block, cfcss_sig_t> dmap;

  // Adjusting signature values for fall-through multi-fan-in successors.
  std::map<basic_block, cfcss_sig_t> dmap_fall_thru;

  // Basic block.
  basic_block bb;

  // Instruction string buffer.
  char asm_str_buf[64];

  // Heap-allocated instruction C-style string pointer.
  char *asm_str;

  // Whether a tail call is encountered.
  bool is_tail_call;

  if(!is_cfc_enabled(fun)) return 0;
  // Instruction.
  rtx_insn *insn;

  // Assembly expression (ASM_OPERANDS).
  rtx asm_expr;


  push_cfun(fun);

  FOR_EACH_BB_FN (bb, fun) {
    // Find all the call statements. The basic blocks are to be split after
    // those statements because subroutine calls can bring changes to the CRC
    // signature.
    bool is_last = true;
    FOR_BB_INSNS_REVERSE (bb, insn) {
      if (CALL_P(insn) && !is_last && !SIBLING_CALL_P(insn))
        call_sites.push_back(std::make_pair(as_a<rtx_call_insn *>(insn), bb));  
      if (NONDEBUG_INSN_P(insn))
        is_last = false;
    }
  }

  for (auto &pair : call_sites) {
    auto stmt = pair.first;
    auto bb = pair.second;
    auto next_bb = split_block(bb, stmt);
    if (dump_file != nullptr)
      fprintf(dump_file, "new block %d due to call site %d\n",
          next_bb->dest->index, INSN_UID(stmt));
  }

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
      // Here, we simply assume the 1-indexed one is the fallthru edge. It does
      // not really matter.
      fall_thru_sigs.push_back((*bb->succs)[1]);
      auto br_target = (*bb->succs)[0]->dest;
      dmap[bb] = sig[bb] ^ sig[(*br_target->preds)[0]->src];
    }
  }

 for (auto orig_edge : fall_thru_sigs) {
    auto pred_bb = orig_edge->src;
    auto succ_bb = orig_edge->dest;
    if (dump_file)
      fprintf(dump_file, "edge <bb %d>-><bb %d> split due to special case\n",
          pred_bb->index, succ_bb->index);
    cfcss_sig_t dmap_val = sig[pred_bb] ^ sig[(*succ_bb->preds)[0]->src];
    if ((orig_edge->flags & EDGE_ABNORMAL) != 0)
      return 0;
    bb = split_edge(orig_edge);
    sig[bb] = sig[pred_bb];
    diff[bb] = 0;
    dmap[bb] = dmap_val;
  }


  FOR_EACH_BB_FN (bb, fun) {
    if (dump_file)
      fprintf(dump_file, "bb %d:\n", bb->index);
    auto insert_ptr = BB_HEAD(bb);
    while (insert_ptr && insert_ptr != NEXT_INSN(BB_END(bb))
           && !NONDEBUG_INSN_P(insert_ptr))
      insert_ptr = NEXT_INSN(insert_ptr);

    cfcss_sig_t cur_sig = sig[bb];
    cfcss_sig_t cur_diff = diff[bb];
    cfcss_sig_t cur_adj = dmap.find(bb) != dmap.end() ? dmap[bb] : 0;

    if (EDGE_COUNT(bb->preds) >= 2) {
      sprintf(asm_str_buf, "ctrlsig_m %d,%d,%d", cur_diff, cur_sig, cur_adj, bb->index);
      asm_str = new char[strlen(asm_str_buf) + 1];
      strcpy(asm_str, asm_str_buf);
      asm_expr = gen_rtx_ASM_OPERANDS (VOIDmode, asm_str, "", 0,
          rtvec_alloc (0), rtvec_alloc (0),
          rtvec_alloc (0), UNKNOWN_LOCATION);
    } else {
      sprintf(asm_str_buf, "ctrlsig_s %d,%d,%d", cur_diff, cur_sig, cur_adj, bb->index);
      asm_str = new char[strlen(asm_str_buf) + 1];
      strcpy(asm_str, asm_str_buf);
      asm_expr = gen_rtx_ASM_OPERANDS (VOIDmode, asm_str, "", 0,
          rtvec_alloc (0), rtvec_alloc (0),
          rtvec_alloc (0), UNKNOWN_LOCATION);
    }
    MEM_VOLATILE_P(asm_expr) = true;
    insn = make_insn_raw(asm_expr);
    if (dump_file)
      fprintf(dump_file, "inserting ctrlsig before uid %d\n",
          INSN_UID(insert_ptr));
    add_insn_before(insn, insert_ptr, bb);
    insert_ptr = insn;
    if (insert_ptr == NEXT_INSN(BB_END(bb)))
      BB_END(bb) = insn;

    if (EDGE_COUNT(bb->preds) > 0
        && (*bb->preds)[0]->src == fun->cfg->x_entry_block_ptr) {
      if (dump_file)
        fprintf(dump_file, "inserting pushsig before uid %d\n",
            INSN_UID(insert_ptr));
      asm_expr = gen_rtx_ASM_OPERANDS (VOIDmode, "pushsig", "", 0,
          rtvec_alloc (0), rtvec_alloc (0),
          rtvec_alloc (0), UNKNOWN_LOCATION);
      MEM_VOLATILE_P(asm_expr) = true;
      add_insn_before(make_insn_raw(asm_expr), insert_ptr, bb);
    }

    insert_ptr = BB_END(bb);
    while (insert_ptr && !NONDEBUG_INSN_P(insert_ptr))
      insert_ptr = PREV_INSN(insert_ptr);
    is_tail_call = false;
    if (CALL_P(insert_ptr) && SIBLING_CALL_P(insert_ptr))
      is_tail_call = true;
    if (CALL_P(insert_ptr) || JUMP_P(insert_ptr))
      do insert_ptr = PREV_INSN(insert_ptr);
      while (insert_ptr && !NONDEBUG_INSN_P(insert_ptr));
    if (CALL_P(insert_ptr) && SIBLING_CALL_P(insert_ptr)) {
      is_tail_call = true;
      do insert_ptr = PREV_INSN(insert_ptr);
      while (insert_ptr && !NONDEBUG_INSN_P(insert_ptr));
    }
    sprintf(asm_str_buf, "crcsig 0x%x",
            ((fun->funcdef_no << 8) + bb->index) & 0xffff, bb->index);
    asm_str = new char[strlen(asm_str_buf) + 1];
    strcpy(asm_str, asm_str_buf);
    asm_expr = gen_rtx_ASM_OPERANDS (VOIDmode, asm_str, "", 0,
        rtvec_alloc (0), rtvec_alloc (0),
        rtvec_alloc (0), UNKNOWN_LOCATION);
    MEM_VOLATILE_P(asm_expr) = true;
    insn = make_insn_raw(asm_expr);
    if (dump_file)
      fprintf(dump_file, "inserting crcsig after uid %d\n",
          INSN_UID(insert_ptr));
    add_insn_after(insn, insert_ptr, bb);
    insert_ptr = insn;


    if ((EDGE_COUNT(bb->succs) > 0
         && (*bb->succs)[0]->dest == fun->cfg->x_exit_block_ptr)
        || EDGE_COUNT(bb->succs) == 0
        || is_tail_call) {
      asm_expr = gen_rtx_ASM_OPERANDS (VOIDmode, "popsig", "", 0,
          rtvec_alloc (0), rtvec_alloc (0),
          rtvec_alloc (0), UNKNOWN_LOCATION);
      MEM_VOLATILE_P(asm_expr) = true;
      insn = make_insn_raw(asm_expr);
      if (dump_file)
        fprintf(dump_file, "inserting popsig after uid %d\n",
            INSN_UID(insert_ptr));
      add_insn_after(insn, insert_ptr, bb);
    }
  }

  pop_cfun();

  return 0;
}
