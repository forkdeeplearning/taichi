#include "../ir.h"

TLANG_NAMESPACE_BEGIN

class BasicBlockEliminate : public IRVisitor {
 public:
  Block *block;

  int current_stmt_id;

  BasicBlockEliminate(Block *block) : block(block) {
    // allow_undefined_visitor = true;
    // invoke_default_visitor = false;
    run();
  }

  void run() {
    for (int i = 0; i < (int)block->statements.size(); i++) {
      current_stmt_id = i;
      block->statements[i]->accept(this);
    }
  }

  void visit(GlobalPtrStmt *stmt) {
    return;
    TC_NOT_IMPLEMENTED
  }

  void visit(ConstStmt *stmt) {
    for (int i = 0; i < current_stmt_id; i++) {
      auto &bstmt = block->statements[i];
      if (typeid(*bstmt) == typeid(*stmt)) {
        if (stmt->width() == bstmt->width()) {
          auto bstmt_ = bstmt->as<ConstStmt>();
          bool same = true;
          for (int l = 0; l < stmt->width(); l++) {
            if (!stmt->val[l].equal_type_and_value(bstmt_->val[l])) {
              same = false;
              break;
            }
          }
          if (same) {
            stmt->replace_with(bstmt.get());
            stmt->parent->erase(current_stmt_id);
            throw IRModifiedException();
          }
        }
      }
    }
  }

  void visit(AllocaStmt *stmt) {
    return;
  }

  void visit(ElementShuffleStmt *stmt) {
    for (int i = 0; i < current_stmt_id; i++) {
      auto &bstmt = block->statements[i];
      if (stmt->ret_type == bstmt->ret_type) {
        if (typeid(*bstmt) == typeid(*stmt)) {
          auto bstmt_ = bstmt->as<ElementShuffleStmt>();
          bool same = true;
          for (int l = 0; l < stmt->width(); l++) {
            if (stmt->elements[l].stmt != bstmt_->elements[l].stmt ||
                stmt->elements[l].index != bstmt_->elements[l].index) {
                same = false;
                break;
              }
          }
          if (same) {
            stmt->replace_with(bstmt.get());
            stmt->parent->erase(current_stmt_id);
            throw IRModifiedException();
          }
        }
      }
    }
  }

  void visit(LocalLoadStmt *stmt) {
    return;
    TC_NOT_IMPLEMENTED
  }

  void visit(LocalStoreStmt *stmt) {
    return;
  }

  // Do not eliminate global data access
  void visit(GlobalLoadStmt *stmt) {
    return;
  }

  void visit(GlobalStoreStmt *stmt) {
    return;
  }

  void visit(BinaryOpStmt *stmt) {
    return;
    TC_NOT_IMPLEMENTED
  }

  void visit(UnaryOpStmt *stmt) {
    return;
    TC_NOT_IMPLEMENTED
  }

  void visit(PrintStmt *stmt) {
    return;
  }

  void visit(RandStmt *stmt) {
    return;
  }

  void visit(WhileControlStmt *stmt) {
    return;
  }
};

class EliminateDup : public IRVisitor {
 public:
  EliminateDup(IRNode *node) {
    allow_undefined_visitor = true;
    invoke_default_visitor = true;
    node->accept(this);
  }

  void visit(Block *block) {
    if (!block->has_container_statements()) {
      while (true) {
        try {
          BasicBlockEliminate _(block);
        } catch (IRModifiedException) {
          continue;
        }
        break;
      }
    } else {
      for (auto &stmt : block->statements) {
        stmt->accept(this);
      }
    }
  }

  void visit(IfStmt *if_stmt) override {
    if (if_stmt->true_statements)
      if_stmt->true_statements->accept(this);
    if (if_stmt->false_statements) {
      if_stmt->false_statements->accept(this);
    }
  }

  void visit(RangeForStmt *for_stmt) {
    auto old_vectorize = for_stmt->vectorize;
    for_stmt->body->accept(this);
  }

  void visit(WhileStmt *stmt) {
    stmt->body->accept(this);
  }
};

namespace irpass {

void eliminate_dup(IRNode *root) {
  EliminateDup _(root);
}

}  // namespace irpass

TLANG_NAMESPACE_END
