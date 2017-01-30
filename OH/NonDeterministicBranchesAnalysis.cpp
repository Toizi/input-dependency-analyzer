#include "NonDeterministicBranchesAnalysis.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace input_slice
{

class NonDeterministicBranchesAnalysis::Impl
{
public:
    Impl(const InputDependencyAnalysis& IDA)
        : m_IDA(IDA)
    {
    }

public:
    const NonDeterministicBranchesAnalysis::blocks& getNonDeterministicBlocks() const
    {
        return m_nonDetBlocks;
    }

    bool isNonDeterministic(BasicBlock* bb) const
    {
        return m_nonDetBlocks.find(bb) != m_nonDetBlocks.end();
    }

    void analize(Function& F);

private:
    void processBranch(llvm::BranchInst* inst);
    void dump() const;

private:
    const InputDependencyAnalysis& m_IDA;
    NonDeterministicBranchesAnalysis::blocks m_nonDetBlocks;
};

void NonDeterministicBranchesAnalysis::Impl::analize(Function& F)
{
    for (auto& B : F) {
        for (auto& I : B) {
            if (auto* branchInst = dyn_cast<BranchInst>(&I)) {
                processBranch(branchInst);
            } else if (auto* phi = dyn_cast<PHINode>(&I)) {
                if (m_IDA.isInputDependent(phi)) {
                    m_nonDetBlocks.insert(&B);
                    continue;
                }
            }
        } 
    }
    dump();
}

void NonDeterministicBranchesAnalysis::Impl::processBranch(BranchInst* inst)
{
    if (m_IDA.isInputDependent(inst)) {
        unsigned numSucc = inst->getNumSuccessors();
        for (unsigned i = 0; i < numSucc; ++i) {
            m_nonDetBlocks.insert(inst->getSuccessor(i));
        }
    }
}

void NonDeterministicBranchesAnalysis::Impl::dump() const
{
    for (auto& B : m_nonDetBlocks) {
        dbgs() << "NON Det block " << *B << "\n";
    }
}


char NonDeterministicBranchesAnalysis::ID = 0;

bool NonDeterministicBranchesAnalysis::runOnFunction(Function& F)
{
    m_analiserImpl.reset(new Impl(getAnalysis<InputDependencyAnalysis>()));
    m_analiserImpl->analize(F);
    return false;
}

const NonDeterministicBranchesAnalysis::blocks& NonDeterministicBranchesAnalysis::getNonDeterministicBlocks() const
{
    return m_analiserImpl->getNonDeterministicBlocks();
}

bool NonDeterministicBranchesAnalysis::isNonDeterministic(BasicBlock* bb) const
{
    return m_analiserImpl->isNonDeterministic(bb);
}

static RegisterPass<NonDeterministicBranchesAnalysis> NDBAP("nondet-branches","Analysis pass to find non deterministic branches");

}


