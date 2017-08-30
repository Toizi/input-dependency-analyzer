#include "FunctionSnippet.h"

#include "Utils.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>

namespace oh {

namespace {

class unique_name_generator
{
public:
    static unique_name_generator& get()
    {
        static unique_name_generator unique_gen;
        return unique_gen;
    }

private:
    unique_name_generator()
        : unique_id(0)
    {
    }

public:
    std::string get_unique(const std::string& name)
    {
        return name + std::to_string(unique_id++);
    }

private:
    unsigned unique_id;
};

using ValueToValueMap = std::unordered_map<llvm::Value*, llvm::Value*>;
using ArgIdxToValueMap = std::unordered_map<int, llvm::Value*>;
using ArgToValueMap = std::unordered_map<llvm::Argument*, llvm::Value*>;

void collect_values(InstructionsSnippet::iterator begin,
                    InstructionsSnippet::iterator end,
                    Snippet::ValueSet& values)
{
    auto it = begin;
    while (it != end) {
        auto instr = &*it;
        ++it;
        if (auto load = llvm::dyn_cast<llvm::LoadInst>(instr)) {
            values.insert(load->getPointerOperand());
        } else if (auto store = llvm::dyn_cast<llvm::StoreInst>(instr)) {
            values.insert(store->getPointerOperand());
            // ValueOperand is either a constant or an instruction that should have been processed before this store
        } else if (auto phi = llvm::dyn_cast<llvm::PHINode>(instr)) {
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
                values.insert(phi->getIncomingValue(i));
            }
        }
    }
}

void setup_function_mappings(llvm::Function* new_F,
                             ArgIdxToValueMap& arg_index_to_value,
                             ValueToValueMap& value_ptr_map,
                             ValueToValueMap& value_map)
{
    // value_ptr_map maps value in original function to value in extracted function
    // value_map maps intermediate value in extracted function to pointer value corresponding to argument in extracted function

    // Create block for new function
    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(new_F->getParent()->getContext(), "entry", new_F);
    llvm::IRBuilder<> builder(entry_block);

    // create mapping from used values to function arguments

    builder.SetInsertPoint(entry_block, ++builder.GetInsertPoint());
    auto arg_it = new_F->arg_begin();
    unsigned i = 0;
    while (arg_it != new_F->arg_end()) {
        const std::string arg_name = "arg" + std::to_string(i);
        arg_it->setName(arg_name);
        llvm::Value* val = arg_index_to_value[i];

        auto ptr_type = llvm::dyn_cast<llvm::PointerType>(arg_it->getType());
        auto val_type = val->getType();
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(val)) {
            val_type = alloca->getAllocatedType();
        }
        assert(ptr_type != nullptr);
        //llvm::dbgs() << "Pointer " << *ptr_type << "\n";

        auto new_ptr_val = builder.CreateAlloca(ptr_type, nullptr,  arg_name + ".ptr");
        builder.CreateStore(&*arg_it, new_ptr_val);
        if (!val_type->isPointerTy()) {
            // otherwise do not create intermediate value
            auto new_val = builder.CreateAlloca(ptr_type->getElementType(), nullptr, arg_name + ".el");

            auto ptr_load = builder.CreateLoad(new_ptr_val);
            auto load = builder.CreateLoad(ptr_load);
            builder.CreateStore(load, new_val);
            value_map[new_ptr_val] = new_val;
            value_ptr_map[val] = new_val;
        } else {
            value_ptr_map[val] = new_ptr_val;
        }
        ++arg_it;
        ++i;
    }
}

void clone_snippet_to_function(llvm::BasicBlock* block,
                               InstructionsSnippet::iterator begin,
                               InstructionsSnippet::iterator end,
                               ValueToValueMap& value_map)
{
    auto& new_function_instructions = block->getInstList();
    auto inst_it = begin;
    end++;
    while (inst_it != end) {
        llvm::Instruction* I = &*inst_it;
        llvm::Instruction* new_I = I->clone();
        //llvm::dbgs() << "Clonned: " << *new_I << "\n";
        new_function_instructions.push_back(new_I);
        value_map.insert(std::make_pair(I, new_I));
        ++inst_it;
    }
}

// TODO: create ValueToValueMapTy and pass to both this function and the one cloning instruction snippet
void clone_blocks_snippet_to_function(llvm::Function* new_F,
                                      BasicBlocksSnippet::iterator begin,
                                      BasicBlocksSnippet::iterator end,
                                      bool clone_begin,
                                      ValueToValueMap& value_ptr_map)
{
    llvm::ValueToValueMapTy value_to_value_map;
    for (auto& entry : value_ptr_map) {
        value_to_value_map.insert(std::make_pair(entry.first, llvm::WeakVH(entry.second)));
    }

    // will clone begin, however it might be replaced later by new entry block, created for start snippet
    llvm::SmallVector<llvm::BasicBlock*, 10> blocks;
    auto block_it = end;
    do {
        if (block_it == begin && !clone_begin) {
            break;
        }
        auto block = llvm::CloneBasicBlock(&*block_it, value_to_value_map, "", new_F);
        value_to_value_map.insert(std::make_pair(&*block_it, llvm::WeakVH(block)));
        value_ptr_map.insert(std::make_pair(&*block_it, block));
        blocks.push_back(block);
        if (block_it == begin) {
            break;
        }
    } while (block_it-- != begin);
    llvm::remapInstructionsInBlocks(blocks, value_to_value_map);
}

void create_new_exit_block(llvm::Function* new_F, llvm::BasicBlock* old_exit_block)
{
    llvm::BasicBlock* new_exit = llvm::BasicBlock::Create(new_F->getParent()->getContext(), "ret", new_F);
    auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
    new_exit->getInstList().push_back(retInst);
    auto pred = pred_begin(old_exit_block);
    while (pred != pred_end(old_exit_block)) {
        llvm::BasicBlock* pred_block = *pred;
        auto term = pred_block->getTerminator();
        llvm::BranchInst* new_branch = llvm::BranchInst::Create(new_exit);
        llvm::ReplaceInstWithInst(term, new_branch);
        ++pred;
    }
    old_exit_block->eraseFromParent();
}

void remap_instructions_in_new_function(llvm::BasicBlock* block,
                                        unsigned skip_instr_count,
                                        const ValueToValueMap& value_ptr_map)
{
    auto& new_function_instructions = block->getInstList();

    llvm::ValueToValueMapTy value_to_value_map;
    for (auto& entry : value_ptr_map) {
        value_to_value_map.insert(std::make_pair(entry.first, llvm::WeakVH(entry.second)));
        //llvm::dbgs() << "Map: " << *entry.first << " to " << *entry.second << "\n";
    }
    llvm::ValueMapper mapper(value_to_value_map);
    //std::vector<llvm::Instruction*> not_mapped_instrs;
    unsigned skip = 0;
    for (auto& instr : new_function_instructions) {
        if (skip++ < skip_instr_count) {
            continue;
        }
        //llvm::dbgs() << "Remap instr: " << instr << "\n";
        mapper.remapInstruction(instr);
        //llvm::dbgs() << "Remaped instr: " << instr << "\n";
    }
}

void create_return_stores(llvm::BasicBlock* block,
                          const ValueToValueMap& value_map)
{
    llvm::IRBuilder<> builder(block);
    // first - pointer, second - value
    for (auto& ret_entry : value_map) {
        //llvm::dbgs() << "store: " << *ret_entry.first << "  " << *ret_entry.second << "\n";
        auto load_ptr = builder.CreateLoad(ret_entry.first);
        auto load_val = builder.CreateLoad(ret_entry.second);
        builder.CreateStore(load_val, load_ptr);
    }
}

llvm::CallInst* create_call_to_snippet_function(llvm::Function* F,
                                                llvm::Instruction* insertion_point,
                                                bool insert_before,
                                                const ArgIdxToValueMap& arg_index_to_value,
                                                const ValueToValueMap& value_ptr_map)
{
    std::vector<llvm::Value*> arguments(arg_index_to_value.size());
    llvm::IRBuilder<> builder(insertion_point);
    if (insert_before) {
        builder.SetInsertPoint(insertion_point->getParent(), builder.GetInsertPoint());
    } else {
        builder.SetInsertPoint(insertion_point->getParent(), ++builder.GetInsertPoint());
    }
    for (auto& arg_entry : arg_index_to_value) {
        auto val_type = arg_entry.second->getType();
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(arg_entry.second)) {
            val_type = alloca->getAllocatedType();
        }
        if (val_type->isPointerTy()) {
            auto load = builder.CreateLoad(arg_entry.second);
            arguments[arg_entry.first] = load;
        } else {
            arguments[arg_entry.first] = arg_entry.second;
        }
    }
    llvm::ArrayRef<llvm::Value*> args_array(arguments);
    llvm::CallInst* callInst = builder.CreateCall(F, args_array);
    return callInst;
}

void erase_snippet(llvm::BasicBlock* block,
                   llvm::BasicBlock::iterator begin,
                   llvm::BasicBlock::iterator end)
{
    assert(InstructionsSnippet::is_valid_snippet(begin, end, block));
    while (end != begin) {
        auto inst = &*end;
        --end;
        if (!inst->user_empty()) {
            //for (auto user : inst->users()) {
            //    if (auto instr = llvm::dyn_cast<llvm::Instruction>(inst)) {
            //        llvm::dbgs() << "User : " << *instr << "\n";
            //    }
            //}
            llvm::dbgs() << "Instruction has uses: do not erase " << *inst << "\n";
            continue;
        }
        inst->eraseFromParent();
    }
    if (begin->user_empty()) {
        begin->eraseFromParent();
    }
}

void erase_snippet(llvm::Function* function,
                   bool erase_begin,
                   llvm::Function::iterator begin,
                   llvm::Function::iterator end)
{
    assert(BasicBlocksSnippet::is_valid_snippet(begin, end, function));
    if (!erase_begin) {
        ++begin;
    }
    while (begin != end) {
        auto block = &*begin;
        llvm::dbgs() << "Erasing block " << block->getName() << "\n";
        ++begin;
        if (!pred_empty(block)) {
            llvm::dbgs() << "Basic block has predecessors: do not erase " << block->getName() << "\n"; 
            continue;
        }
        block->eraseFromParent();
    }
    // end is not removed
}

llvm::FunctionType* create_function_type(llvm::LLVMContext& Ctx,
                                         const Snippet::ValueSet& used_values,
                                         ArgIdxToValueMap& arg_values)
{
    std::vector<llvm::Type*> arg_types;
    int i = 0;
    for (auto& val : used_values) {
        arg_values[i] = val;
        ++i;
        auto type = val->getType();
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(val)) {
            type = alloca->getAllocatedType();
        }
        if (type->isPointerTy()) {
            arg_types.push_back(type);
        } else {
            arg_types.push_back(type->getPointerTo());
        }
    }
    llvm::ArrayRef<llvm::Type*> params(arg_types);
    llvm::FunctionType* f_type = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), params, false);
    return f_type;
}


}

InstructionsSnippet::InstructionsSnippet(llvm::BasicBlock* block,
                                         iterator begin,
                                         iterator end)
    : m_block(block)
    , m_begin(begin)
    , m_end(end)
    , m_begin_idx(-1)
    , m_end_idx(-1)
{
    m_begin_idx = Utils::get_instruction_index(&*m_begin);
    m_end_idx = Utils::get_instruction_index(&*m_end);
}

bool InstructionsSnippet::is_valid_snippet() const
{
    return m_block && InstructionsSnippet::is_valid_snippet(m_begin, m_end, m_block);
}

bool InstructionsSnippet::intersects(const Snippet& snippet) const
{
    assert(snippet.is_valid_snippet());
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (instr_snippet) {
        if (m_block != instr_snippet->get_block()) {
            return false;
        }
        return instr_snippet->get_begin_index() <= m_end_idx && m_begin_idx <= instr_snippet->get_end_index();
    }
    // redirect to block snippet function
    return snippet.intersects(*this);
}

void InstructionsSnippet::expand()
{
    InstructionSet instructions;
    snippet_instructions(instructions);
    auto it = m_end;
    do {
        llvm::Instruction* instr = &*it;
        expand_for_instruction(instr, instructions);
        // not to decrement begin
        if (it == m_begin) {
            break;
        }
    } while (it-- != m_begin);
}

void InstructionsSnippet::collect_used_values()
{
    if (!m_used_values.empty()) {
        // already collected
        return;
    }
    collect_values(m_begin, m_end++, m_used_values);
}

void InstructionsSnippet::merge(const Snippet& snippet)
{
    // expand this to include given snippet
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (instr_snippet) {
        if (m_begin_idx > instr_snippet->get_begin_index()) {
            m_begin = instr_snippet->get_begin();
            m_begin_idx = instr_snippet->get_begin_index();
        }
        if (m_end_idx < instr_snippet->get_end_index()) {
            m_end = instr_snippet->get_end();
            m_end_idx = instr_snippet->get_end_index();
        }
        return;
    }
    assert(false);
    // do not merge instruction snippet with block snippet,
    // as block snippet should be turn into instruction snippet
}

llvm::Function* InstructionsSnippet::to_function()
{
    // create function type
    llvm::LLVMContext& Ctx = m_block->getModule()->getContext();
    // maps argument index to corresponding value
    ArgIdxToValueMap arg_index_to_value;
    llvm::FunctionType* type = create_function_type(Ctx, m_used_values, arg_index_to_value);
    std::string f_name = unique_name_generator::get().get_unique(m_block->getParent()->getName());
    llvm::Function* new_F = llvm::Function::Create(type,
                                                   llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                                                   f_name,
                                                   m_block->getModule());
    // Maps the values in original function to local values in extracted function.
    // Further adds instruction mapping as well
    ValueToValueMap value_ptr_map;

    // maps element type value to corresponding pointer value. all operations will be done over this value.
    // Maps local values mapped in previous map, to pointers passed as argument. This is done for convenience,
    // not to use pointers in operations.
    // e.g. if c = a + b is extracted, this instruction will be replaced by
    // c_local = a_local + b_local, where c_local, a_local and b_local are of the same type as a and b. 
    // Further c_local will be stored into corresponding c_ptr pointer value to be returned from function.
    // this map contains mapping from c_ptr to c_local
    ValueToValueMap value_map;

    setup_function_mappings(new_F, arg_index_to_value, value_ptr_map, value_map);
    llvm::BasicBlock* entry_block = &new_F->getEntryBlock();

    unsigned setup_size = entry_block->size();
    clone_snippet_to_function(entry_block, m_begin, m_end, value_ptr_map);
    remap_instructions_in_new_function(entry_block, setup_size, value_ptr_map);
    create_return_stores(entry_block, value_map);
    auto retInst = llvm::ReturnInst::Create(new_F->getParent()->getContext());
    entry_block->getInstList().push_back(retInst);

    //llvm::dbgs() << "START\n";
    //llvm::dbgs() << *entry_block << "\n";
    //llvm::dbgs() << "END\n";

    auto insert_before = m_end;
    ++insert_before;
    auto callInst = create_call_to_snippet_function(new_F, &*insert_before, true, arg_index_to_value, value_ptr_map);
    erase_snippet(m_block, m_begin, m_end);
    return new_F;
}

void InstructionsSnippet::dump() const
{
    llvm::dbgs() << "****Instructions snippet****\n";
    auto it = m_begin;
    while (it != m_end) {
        llvm::dbgs() << *it << "\n";
        ++it;
    }
    if (m_end != m_begin->getParent()->end()) {
        llvm::dbgs() << *it << "\n";
    }
    llvm::dbgs() << "*********\n";
}

InstructionsSnippet* InstructionsSnippet::to_instrSnippet()
{
    return this;
}

InstructionsSnippet::iterator InstructionsSnippet::get_begin() const
{
    return m_begin;
}

InstructionsSnippet::iterator InstructionsSnippet::get_end() const
{
    return m_end;
}

llvm::Instruction* InstructionsSnippet::get_begin_instr() const
{
    return &*m_begin;
}

llvm::Instruction* InstructionsSnippet::get_end_instr() const
{
    return &*m_end;
}

int InstructionsSnippet::get_begin_index() const
{
    return m_begin_idx;
}

int InstructionsSnippet::get_end_index() const
{
    return m_end_idx;
}

bool InstructionsSnippet::is_block() const
{
    return (m_begin == m_block->begin() && m_end == --m_block->end());
}

llvm::BasicBlock* InstructionsSnippet::get_block() const
{
    return m_block;
}

void InstructionsSnippet::clear()
{
    if (!is_valid_snippet()) {
        return;
    }
    m_end = m_block->end();
    m_begin = m_end;
    m_begin_idx = -1;
    m_end_idx = -1;
    m_block = nullptr;
}

bool InstructionsSnippet::is_valid_snippet(iterator begin,
                                           iterator end,
                                           llvm::BasicBlock* block)
{
    bool valid = (begin != block->end());
    valid &= (begin->getParent() == block);
    if (end != block->end()) {
        valid &= (end->getParent() == block);
    }
    return valid;
}

bool InstructionsSnippet::is_single_instr_snippet() const
{
    return m_begin == m_end;
}

void InstructionsSnippet::snippet_instructions(InstructionSet& instrs) const
{
    std::for_each(m_begin, m_end, [&instrs] (llvm::Instruction& instr) { instrs.insert(&instr); });
    instrs.insert(&*m_end);
}

void InstructionsSnippet::expand_for_instruction(llvm::Instruction* instr,
                                                 InstructionSet& instructions)
{
    if (auto load = llvm::dyn_cast<llvm::LoadInst>(instr)) {
        assert(instructions.find(instr) != instructions.end());
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(load->getPointerOperand())) {
            m_used_values.insert(alloca);
        }
        return;
    }
    if (auto store = llvm::dyn_cast<llvm::StoreInst>(instr)) {
        auto value_op = store->getValueOperand();
        if (llvm::dyn_cast<llvm::AllocaInst>(value_op)) {
            m_used_values.insert(value_op);
            return;
        }
        expand_for_instruction_operand(value_op, instructions);
        auto storeTo = store->getPointerOperand();
        // e.g. for pointer loadInst will be pointer operand, but it should not be used as value
        if (llvm::dyn_cast<llvm::AllocaInst>(storeTo)) {
            m_used_values.insert(storeTo);
        }
    } else {
        for (unsigned i = 0; i < instr->getNumOperands(); ++i) {
            expand_for_instruction_operand(instr->getOperand(i), instructions);
        }
    }
}

void InstructionsSnippet::expand_for_instruction_operand(llvm::Value* val,
                                                         InstructionSet& instructions)
{
    auto instr = llvm::dyn_cast<llvm::Instruction>(val);
    if (!instr) {
        return;
    }
    auto res = instructions.insert(instr);
    if (!res.second) {
        return;
    }
    //llvm::dbgs() << "Expand: add " << *instr << "\n";
    auto new_begin = instr->getIterator();
    auto new_begin_idx = Utils::get_instruction_index(&*new_begin);
    if (m_begin_idx > new_begin_idx) {
        m_begin = new_begin;
        m_begin_idx = new_begin_idx;
    }
}

BasicBlocksSnippet::BasicBlocksSnippet(llvm::Function* function,
                                       iterator begin,
                                       iterator end,
                                       InstructionsSnippet start)
    : m_function(function)
    , m_begin(begin)
    , m_end(end)
    , m_start(start)
{
}

bool BasicBlocksSnippet::is_valid_snippet() const
{
    return m_function && BasicBlocksSnippet::is_valid_snippet(m_begin, m_end, m_function);
}

bool BasicBlocksSnippet::intersects(const Snippet& snippet) const
{
    if (!m_start.is_valid_snippet()) {
        return false;
    }
    return m_start.intersects(snippet);
}

void BasicBlocksSnippet::expand()
{
    m_start.expand();
    // can include block in snippet
    if (m_start.is_block()) {
        //m_begin = Utils::get_block_pos(m_start.get_block());
        m_begin = m_start.get_block()->getIterator();
        m_start.clear();
    }
}

void BasicBlocksSnippet::collect_used_values()
{
    if (!m_used_values.empty()) {
        return;
    }
    m_start.collect_used_values();
    auto used_in_start = m_start.get_used_values();
    m_used_values.insert(used_in_start.begin(), used_in_start.end());
    auto block_it = m_begin;
    while (block_it != m_end) {
        collect_values(block_it->begin(), block_it->end(), m_used_values);
        ++block_it;
    }
}

void BasicBlocksSnippet::merge(const Snippet& snippet)
{
    auto instr_snippet = const_cast<Snippet&>(snippet).to_instrSnippet();
    if (instr_snippet) {
        m_start.merge(snippet);
    }
    // do not merge block snippets for now
}

// create function type and corresponding function
// need to create entry block for start snippet in new function,
// clone all blocks in snippet to new function
// create new exit block in new function
// replace all edges to the end block to new exit block.
// Remove end block from new function
// Remove [m_begin, m_end) blocks from original functions
// Remove start snippet from old function.
// create call instruction
// create unconditional jump to m_end after call instruction
// TODO: extract the common code with instruction snippet
llvm::Function* BasicBlocksSnippet::to_function()
{
    collect_used_values();
    llvm::LLVMContext& Ctx = m_function->getParent()->getContext();
    // maps argument index to corresponding value
    ArgIdxToValueMap arg_index_to_value;
    llvm::FunctionType* type = create_function_type(Ctx, m_used_values, arg_index_to_value);
    std::string f_name = unique_name_generator::get().get_unique(m_begin->getParent()->getName());
    llvm::Function* new_F = llvm::Function::Create(type,
                                                   llvm::GlobalValue::LinkageTypes::ExternalLinkage,
                                                   f_name,
                                                   m_function->getParent());
    llvm::dbgs() << "Function type " << *type << "\n";

    ValueToValueMap value_ptr_map;
    ValueToValueMap value_map;
    setup_function_mappings(new_F, arg_index_to_value, value_ptr_map, value_map);
    llvm::BasicBlock* entry_block = &new_F->getEntryBlock();

    const bool has_start_snippet = m_start.is_valid_snippet();

    // this function will also create new exit block
    clone_blocks_snippet_to_function(new_F, m_begin, m_end, !has_start_snippet, value_ptr_map);
    unsigned setup_size = entry_block->size();
    if (has_start_snippet) {
        // begining will go to entry block
        clone_snippet_to_function(entry_block, m_start.get_begin(), m_start.get_end(), value_ptr_map);
        remap_instructions_in_new_function(entry_block, setup_size, value_ptr_map);
    }
    // create new exit block
    auto exit_block_entry = value_ptr_map.find(&*m_end);
    assert(exit_block_entry != value_ptr_map.end());
    llvm::BasicBlock* exit_block = llvm::dyn_cast<llvm::BasicBlock>(exit_block_entry->second);
    assert(exit_block);
    create_new_exit_block(new_F, exit_block);


    if (has_start_snippet) {
        auto insert_after = m_start.get_end();
        if (insert_after == m_start.get_block()->end()) {
            --insert_after;
        }
        create_call_to_snippet_function(new_F, &*insert_after, false, arg_index_to_value, value_ptr_map);
        auto branch_inst = llvm::BranchInst::Create(&*m_end);
        m_start.get_block()->getInstList().push_back(branch_inst);
        erase_snippet(m_start.get_block(), m_start.get_begin(), m_start.get_end());
    }  else {
        // insert at the end of each predecessor
        // Is it safe to assume there is only one predecessor?
        auto pred_it = pred_begin(&*m_begin);
        while (pred_it != pred_end(&*m_end)) {
            auto pred = *pred_it;
            auto insert_after = pred->end();
            --insert_after;
            create_call_to_snippet_function(new_F, &*insert_after, false, arg_index_to_value, value_ptr_map);
            auto pred_term = pred->getTerminator();
            auto new_term = llvm::BranchInst::Create(&*m_end);
            llvm::ReplaceInstWithInst(pred_term, new_term);
            ++pred_it;
        }
    }
    erase_snippet(m_function, !has_start_snippet, m_begin, m_end);
    return new_F;
}

BasicBlocksSnippet::iterator BasicBlocksSnippet::get_begin() const
{
    return m_begin;
}

BasicBlocksSnippet::iterator BasicBlocksSnippet::get_end() const
{
    return m_end;
}

BasicBlocksSnippet* BasicBlocksSnippet::to_blockSnippet()
{
    return this;
}

void BasicBlocksSnippet::dump() const
{
    llvm::dbgs() << "****Block snippet*****\n";
    m_start.dump();
    auto it = m_begin;
    while (it != m_end) {
        llvm::dbgs() << it->getName() << "\n";
        ++it;
    }
    if (m_end != m_begin->getParent()->end()) {
        llvm::dbgs() << it->getName() << "\n";
    }
    llvm::dbgs() << "*********\n";
}

bool BasicBlocksSnippet::is_valid_snippet(iterator begin,
                                          iterator end,
                                          llvm::Function* parent)
{
    return (begin != parent->end() && begin != end);
}

}
