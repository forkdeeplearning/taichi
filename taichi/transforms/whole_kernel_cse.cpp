#include "taichi/ir/ir.h"
#include "taichi/ir/transforms.h"
#include "taichi/ir/analysis.h"
#include "taichi/ir/visitors.h"

TLANG_NAMESPACE_BEGIN

// Whole Kernel Common Subexpression Elimination
class WholeKernelCSE : public BasicStmtVisitor {
 private:
  std::unordered_set<int> visited;
  // each scope corresponds to an unordered_set
  std::vector<std::unordered_set<Stmt *>> visible_stmts;

 public:
  using BasicStmtVisitor::visit;

  WholeKernelCSE() {
  }

  bool is_done(Stmt *stmt) {
    return visited.find(stmt->instance_id) != visited.end();
  }

  void set_done(Stmt *stmt) {
    visited.insert(stmt->instance_id);
  }

  void visit(Stmt *stmt) override {
    if (stmt->has_global_side_effect())
      return;
    // Generic visitor for all non-container statements that don't have global
    // side effect.
    if (is_done(stmt)) {
      visible_stmts.back().insert(stmt);
      return;
    }
    for (auto &scope : visible_stmts) {
      for (auto &prev_stmt : scope) {
        if (irpass::analysis::same_statements(stmt, prev_stmt)) {
          stmt->replace_with(prev_stmt);
          stmt->parent->erase(stmt);
          throw IRModified();
        }
      }
    }
    visible_stmts.back().insert(stmt);
    set_done(stmt);
  }

  void visit(Block *stmt_list) override {
    visible_stmts.emplace_back();
    for (auto &stmt : stmt_list->statements) {
      stmt->accept(this);
    }
    visible_stmts.pop_back();
  }

  void visit(IfStmt *if_stmt) override {
    if (if_stmt->true_statements) {
      if (if_stmt->true_statements->statements.empty()) {
        if_stmt->true_statements = nullptr;
        throw IRModified();
      }
    }

    if (if_stmt->false_statements) {
      if (if_stmt->false_statements->statements.empty()) {
        if_stmt->false_statements = nullptr;
        throw IRModified();
      }
    }

    // Move common statements at the beginning or the end of both branches
    // outside.
    if (if_stmt->true_statements && if_stmt->false_statements) {
      auto &true_clause = if_stmt->true_statements;
      auto &false_clause = if_stmt->false_statements;
      if (irpass::analysis::same_statements(
              true_clause->statements[0].get(),
              false_clause->statements[0].get())) {
        auto common_stmt = true_clause->extract(0);
        irpass::replace_all_usages_with(false_clause.get(),
                                        false_clause->statements[0].get(),
                                        common_stmt.get());
        if_stmt->insert_before_me(std::move(common_stmt));
        false_clause->erase(0);
        throw IRModified();
      }
      if (irpass::analysis::same_statements(
              true_clause->statements.back().get(),
              false_clause->statements.back().get())) {
        auto common_stmt = true_clause->extract((int)true_clause->size() - 1);
        irpass::replace_all_usages_with(false_clause.get(),
                                        false_clause->statements.back().get(),
                                        common_stmt.get());
        if_stmt->insert_before_me(std::move(common_stmt));
        false_clause->erase((int)false_clause->size() - 1);
        throw IRModified();
      }
    }

    if (if_stmt->true_statements)
      if_stmt->true_statements->accept(this);
    if (if_stmt->false_statements)
      if_stmt->false_statements->accept(this);
  }

  static void run(IRNode *node) {
    WholeKernelCSE eliminator;
    while (true) {
      bool modified = false;
      try {
        node->accept(&eliminator);
      } catch (IRModified) {
        modified = true;
      }
      if (!modified)
        break;
    }
  }
};

namespace irpass {

void whole_kernel_cse(IRNode *root) {
  WholeKernelCSE::run(root);
}

}  // namespace irpass

TLANG_NAMESPACE_END
