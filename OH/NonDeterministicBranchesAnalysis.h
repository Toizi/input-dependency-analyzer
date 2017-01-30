#pragma once

#include "llvm/Pass.h"
#include "InputDependencyAnalysis.h"

#include <memory>
#include <unordered_set>

namespace llvm {

class BasicBlock;
class Function;

}

namespace input_slice {

class NonDeterministicBranchesAnalysis : public llvm::FunctionPass
{
public:
    typedef std::unordered_set<llvm::BasicBlock*> blocks;

public:
    static char ID;

    NonDeterministicBranchesAnalysis()
        : llvm::FunctionPass(ID)
    {
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const
    {
        AU.addRequired<InputDependencyAnalysis>();
        AU.setPreservesAll();
    }

    bool runOnFunction(llvm::Function& F);

public:
    const blocks& getNonDeterministicBlocks() const;
    bool isNonDeterministic(llvm::BasicBlock* bb) const;

private:
    class Impl;
    std::shared_ptr<Impl> m_analiserImpl;
};

}


