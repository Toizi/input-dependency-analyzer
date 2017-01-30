#pragma once

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include <memory>

namespace llvm {

class Instruction;
}

namespace input_slice {

class InputDependencyAnalysis : public llvm::FunctionPass {
public:
    static char ID;

public:
    InputDependencyAnalysis()
        : llvm::FunctionPass(ID)
    {
    }

public:
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const
    {
        AU.addRequiredTransitive<llvm::AliasAnalysis>();
        AU.setPreservesAll();
    }

    bool runOnFunction(llvm::Function& F);

public:
    bool isInputDependent(llvm::Instruction* instr) const;

private:
    class Impl;
    std::shared_ptr<Impl> m_analiserImpl;
};

}

