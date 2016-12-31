#ifndef _LLVM_DG_CONTROL_EXPRESSION_H_
#define _LLVM_DG_CONTROL_EXPRESSION_H_

#include <cassert>
#include <llvm/IR/Module.h>

#include <llvm/Config/llvm-config.h>

#include <llvm/IR/CFG.h>

#include "analysis/ControlExpression/CFA.h"
#include "analysis/ControlExpression/ControlExpression.h"

namespace dg {

typedef CFA<llvm::BasicBlock *> LLVMCFA;
typedef CFANode<llvm::BasicBlock *> LLVMCFANode;

class LLVMCFABuilder {

public:
    LLVMCFA build(llvm::Function& F)
    {
        std::map<llvm::BasicBlock *, LLVMCFANode *> mapping;
        LLVMCFA cfa;

        // create nodes for all basic blocks
        for (llvm::BasicBlock& B : F) {
            mapping[&B] = new LLVMCFANode(&B);
        }

        // add successors for all basic blocks
        for (llvm::BasicBlock& B : F) {
            LLVMCFANode *node = mapping[&B];
            assert(node);

            // iterate over all successors of the basic block
            for (llvm::succ_iterator
                 S = succ_begin(&B), E = succ_end(&B); S != E; ++S) {
                LLVMCFANode *succ = mapping[*S];

                // add the successor
                node->addSuccessor(succ);
            }

            // add the node into CFA -- we
            // must do it now, after the node is fully
            // initialized (when it has successors)
            cfa.addNode(node);
        }

        return cfa;
    }

};


} // namespace dg

#endif // _LLVM_DG_CONTROL_EXPRESSION_H_
