#include "InputDependencyAnalysis.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>
#include <unordered_set>
#include <unordered_map>

using namespace llvm;

namespace input_slice {


class InputDependencyAnalysis::Impl
{
private:
    enum VariableState {
        INPUT_DEP,
        INPUT_INDEP,
        MAY_DEP,
        UNKNOWN
    };

public:
    typedef std::unordered_set<llvm::Instruction*> InstrSet;

public:
    Impl(llvm::AliasAnalysis& AAR)
        : m_AAR(AAR)
    {
    }

public:
    bool isInputDependent(llvm::Instruction* instr) const;

    void analize(Function& F);
    void dump() const;

private:
    void processMainArguments(llvm::Function& F);
    void processReturnInstr(llvm::ReturnInst* retInst);
    void processBranchInst(llvm::BranchInst* branchInst);
    void processStoreInst(llvm::StoreInst* storeInst);
    void processInstr(llvm::Instruction* instr);

    Value* getMemoryValue(Value* instrOp) const;
    Value* simplifyConditionInstruction(Instruction* instr) const;

    // TODO: change names
    bool dependsOnConstInstructions(llvm::Instruction* instr);
    bool checkDependency(llvm::LoadInst* instr);
    bool isInput(llvm::Value* val) const;

private:
    llvm::AliasAnalysis& m_AAR;
    InstrSet m_inputIndependentInstrs; // may be deleted later
    InstrSet m_inputDependentInstrs;
    std::unordered_set<Value*> m_inputs;
    std::unordered_map<Value*, VariableState> m_variableState;
};

bool InputDependencyAnalysis::Impl::isInputDependent(Instruction* inst) const
{
    return m_inputDependentInstrs.find(inst) != m_inputIndependentInstrs.end();
}


void InputDependencyAnalysis::Impl::analize(Function& F)
{
    if (F.getName() == "main") {
        processMainArguments(F);
    }

    dbgs() << "In function " << F.getName() << "\n";
    for (auto& B : F) {
        for (auto& I : B) {
            if (auto* allocInst = dyn_cast<AllocaInst>(&I)) {
                // Note alloc instructions are at the begining of the function
                // Here just collect them with unknown state
                m_variableState[allocInst] = UNKNOWN;
            } else if (auto* retInst = dyn_cast<ReturnInst>(&I)) {
                processReturnInstr(retInst);
            }  else if (auto* branchInst = dyn_cast<BranchInst>(&I)) {
                processBranchInst(branchInst);
            } else if (auto* storeInst = dyn_cast<StoreInst>(&I)) {
                processStoreInst(storeInst);
            } else {
                processInstr(&I);
            }
        }
    }
}

void InputDependencyAnalysis::Impl::dump() const
{
    for (auto& constInst : m_inputIndependentInstrs) {
        dbgs() << "Const instr: " << *constInst << "\n";
    }
    dbgs() << "***************************************\n";
    for (auto& nonConstInst : m_inputDependentInstrs) {
        dbgs() << "Input Dependent instr: " << *nonConstInst << "\n";
    }
    dbgs() << "***************************************\n";
    for (auto& inputInst : m_inputs) {
        dbgs() << "Input instr: " << *inputInst << "\n";
    }
}

void InputDependencyAnalysis::Impl::processMainArguments(Function& F)
{
    auto& arguments = F.getArgumentList();
    std::for_each(arguments.begin(), arguments.end(),
                  [this] (Argument& arg) {
                        if (auto* argVal = dyn_cast<Value>(&arg)) {
                            this->m_inputs.insert(argVal);
                        }
                });
}

void InputDependencyAnalysis::Impl::processReturnInstr(ReturnInst* retInst)
{
    auto retValue = retInst->getReturnValue();
    if (!retValue) {
        return;
    } else if (auto* constVal = dyn_cast<Constant>(retValue)) {
        m_inputIndependentInstrs.insert(retInst);
    } else if (auto* retVal = dyn_cast<Value>(retValue)) {
        if (isInput(retVal)) {
            m_inputDependentInstrs.insert(retInst);
        }
    } else if (auto* retValInst = dyn_cast<Instruction>(retValue)) {
        if (!dependsOnConstInstructions(retValInst)) {
            m_inputDependentInstrs.insert(retInst);
        }
    } else {
        m_inputIndependentInstrs.insert(retInst);
    }
}

void InputDependencyAnalysis::Impl::processBranchInst(BranchInst* branchInst)
{
    if (branchInst->isUnconditional()) {
        m_inputIndependentInstrs.insert(branchInst);
        return;
    }

    auto condition = branchInst->getCondition();
    if (auto* constCond = dyn_cast<Constant>(condition)) {
        m_inputIndependentInstrs.insert(branchInst);
    } else if (auto* condInstr = dyn_cast<Instruction>(condition)) {
        Value* simplified = simplifyConditionInstruction(condInstr);
        if (simplified) {
            // Later can check for true or false. Now Constant is enough to determine input dependency.
            if (auto* constVal = dyn_cast<Constant>(simplified)) {
                // Investigations showed sometimes returns false, when should be null. For true works correctly.
                if (constVal == ConstantInt::getTrue(constVal->getType())) {
                    m_inputIndependentInstrs.insert(branchInst);
                    return;
                }
            } else {
                condInstr = dyn_cast<Instruction>(simplified);
            }
        }
        assert(condInstr);
        if (dependsOnConstInstructions(condInstr)) {
            m_inputIndependentInstrs.insert(branchInst);
        } else {
            m_inputDependentInstrs.insert(branchInst);
        }
    } else if (auto* condVal = dyn_cast<Value>(condition)) {
        if (isInput(condVal)) {
            m_inputDependentInstrs.insert(branchInst);
        } else {
            m_inputIndependentInstrs.insert(branchInst);
        }
    } else {
        m_inputIndependentInstrs.insert(branchInst);
    }
}

void InputDependencyAnalysis::Impl::processStoreInst(StoreInst* storeInst)
{
    Value* storedValue = getMemoryValue(storeInst->getPointerOperand());
    assert(storedValue);
    auto op = storeInst->getOperand(0);
    if (auto* constOp = dyn_cast<Constant>(op)) {
        m_inputIndependentInstrs.insert(storeInst);
        m_variableState[storedValue] = INPUT_INDEP;
    } else if (isInput(op) || !dependsOnConstInstructions(storeInst)) {
        m_inputDependentInstrs.insert(storeInst);
        m_variableState[storedValue] = INPUT_DEP;
    } else {
        m_inputIndependentInstrs.insert(storeInst);
        m_variableState[storedValue] = INPUT_INDEP;
    }
}

void InputDependencyAnalysis::Impl::processInstr(Instruction* inst)
{
    dbgs() << "Process inst " << *inst << "\n";
    if (dependsOnConstInstructions(inst)) {
        m_inputIndependentInstrs.insert(inst);
    } else {
        m_inputDependentInstrs.insert(inst);
    }
}

// This function is called either for pointerOperand of storeInst or loadInst.
// It retrives underlying Values which will be affected by these instructions.
// These values are either allocaInstructions of global values (at least for now).
// TODO: see what happens for pointers and references
Value* InputDependencyAnalysis::Impl::getMemoryValue(Value* instrOp) const
{
    AllocaInst* alloca = dyn_cast<AllocaInst>(instrOp);
    if (alloca) {
        return alloca;
    }
    GlobalValue* global = nullptr;
    bool clean = false;
    GetElementPtrInst* elPtrInst = dyn_cast<GetElementPtrInst>(instrOp);
    if (elPtrInst == nullptr) {
        // This is the case when array element is accessed with constant index.
        if (auto* constGetEl = dyn_cast<ConstantExpr>(instrOp)) {
            // This instruction won't be added to any basic block. It is just to get underlying array.
            elPtrInst = dyn_cast<GetElementPtrInst>(constGetEl->getAsInstruction());
            clean = true;
        }
    }
    // See if this is assert. What other instrs can be operand for storeInstr
    assert(elPtrInst);
    auto* op = elPtrInst->getPointerOperand();
    global = dyn_cast<GlobalValue>(op);
    if (clean) {
        // Deleting as does not belong to any basic block. 
        delete elPtrInst;
    }
    if (global == nullptr) {
        return dyn_cast<AllocaInst>(op);
    }
    return global;
}

// Later can use instruction simplification for other instructions too.
// Say add instruction x - x can be simplified to 0 etc.
// For the case x = y if (x == y) gives false.
Value* InputDependencyAnalysis::Impl::simplifyConditionInstruction(Instruction* instr) const
{
    //dbgs() << "simplifying instruction " << *instr << "\n";
    auto* module = instr->getParent()->getParent()->getParent();
    if (auto* cmpInstr = dyn_cast<CmpInst>(instr)) {
        // For now the possible types for these operands are loadInstruction or getElementPtr
        // TODO: may need to process constant getElementPtr separately
        auto* leftOp = cmpInstr->getOperand(0);
        Value* leftVal = nullptr;
        if (auto* leftInst = dyn_cast<Instruction>(leftOp)) {
            // for instruction like load is the same as getPointerOperand.
            leftVal = leftInst->getOperand(0);
        } else {
            leftVal = leftOp;
        }
        //dbgs() << "Left instruction " << *leftOp << "\n";
        //dbgs() << "Left Value " << *leftVal << "\n";
        auto* rightOp = cmpInstr->getOperand(1);
        Value* rightVal = nullptr;
        if (auto* rightInst = dyn_cast<Instruction>(rightOp)) {
            rightVal = rightInst->getOperand(0);
        } else {
            rightVal = rightOp;
        }
        //dbgs() << "Rigth instruction " << *rightOp << "\n";
        //dbgs() << "Right Value " << *rightVal << "\n";

        Value* val = SimplifyCmpInst(cmpInstr->getPredicate(),
                                     leftVal,
                                     rightVal,
                                     new DataLayout(module->getDataLayout()));
        if (val) {
            //dbgs() << "Simplified cmp instr to " << *val << "\n";
            return val;
        }
    }
    return SimplifyInstruction(instr, new DataLayout(module->getDataLayout()));
}

// If at least one of operands is input dependent, than whole instruction is input dependent.
bool InputDependencyAnalysis::Impl::dependsOnConstInstructions(Instruction* inst)
{
    if (auto* loadInst = dyn_cast<LoadInst>(inst)) {
        return checkDependency(loadInst);
    }
    for (auto op = inst->op_begin(); op != inst->op_end(); ++op) {
        if (auto* opInst = dyn_cast<Instruction>(op)) {
            if (!dependsOnConstInstructions(opInst)) {
                return false;
            }
        } else if (auto* opVal = dyn_cast<Value>(op)) {
            if (isInput(opVal)) {
                return false;
            }
        }
    }
    return true;
}

// true if depends on const, false otherwise
bool InputDependencyAnalysis::Impl::checkDependency(LoadInst* inst)
{
    auto* loadOp = inst->getPointerOperand();
    Value* loadedValue = getMemoryValue(loadOp);
    if (loadedValue == nullptr) {
        return dependsOnConstInstructions(dyn_cast<Instruction>(loadOp));
    }
    if (m_variableState[loadedValue] == INPUT_DEP) {
        return false;
    } else if (m_variableState[loadedValue] == INPUT_INDEP) {
        return true;
    }
    // it is not determined
    return true;
}

bool InputDependencyAnalysis::Impl::isInput(Value* val) const
{
    if (m_inputs.find(val) != m_inputs.end()) {
        return true;
    }
    for (auto input : m_inputs) {
        auto valArg = dyn_cast<Argument>(val);
        auto inputArg = dyn_cast<Argument>(input);
        if (valArg && inputArg && valArg->getParent() == inputArg->getParent()) {
            if (m_AAR.alias(val, input)) {
                return true;
            }
        }
    }
    return false;
}

char InputDependencyAnalysis::ID = 0;


/*
   Uses da data dependency analysis pass. Not sure is the best way.

   Collect constant store instrs as base instruction not dependent on input.
   For each instruction check if depends on any of constants. If no add to input dependent instructions set.
   If function is not main, and one of its arguments depends on input, function call will be considered as input dependent instruction.
   TODO: function is sliced itself with respect to that argument.

   Is not very effective. For each instruction iterates through constant instructions known
   so far, and for each non const instruction iterates through all arguments of the function.
 */
bool InputDependencyAnalysis::runOnFunction(Function& F)
{
    m_analiserImpl.reset(new Impl(getAnalysis<AliasAnalysis>()));
    m_analiserImpl->analize(F);
    m_analiserImpl->dump();
    return false;
}

bool InputDependencyAnalysis::isInputDependent(llvm::Instruction* instr) const
{
    return m_analiserImpl->isInputDependent(instr);
}

static RegisterPass<InputDependencyAnalysis> INDEPA("input-dependency","Analysis pass to find non deterministic branches");

} // namespace input_slice

