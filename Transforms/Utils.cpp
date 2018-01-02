#include "Utils.h"

#include "llvm/IR/CFG.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"


#include <list>

namespace oh {

namespace {

void blocks_in_range(llvm::BasicBlock* block,
                     llvm::BasicBlock* stop_block,
                     std::unordered_set<llvm::BasicBlock*>& blocks)
{
    if (block == stop_block) {
        return;
    }
    auto res = blocks.insert(block);
    if (!res.second) {
        return; // block is added, hence successors are also processed
    }
    auto it = succ_begin(block);
    while (it != succ_end(block)) {
        blocks_in_range(*it, stop_block, blocks);
        ++it;
    }
}

}

llvm::BasicBlock::iterator Utils::get_instruction_pos(llvm::Instruction* I)
{
    auto it = I->getParent()->begin();
    while (&*it != I && it != I->getParent()->end()) {
        ++it;
    }
    assert(it != I->getParent()->end());
    return it;
}

llvm::Function::iterator Utils::get_block_pos(llvm::BasicBlock* block)
{
    auto it = block->getParent()->begin();
    while (&*it != block && it != block->getParent()->end()) {
        ++it;
    }
    return it;
}

unsigned Utils::get_instruction_index(llvm::Instruction* I)
{
    unsigned idx = 0;
    auto it = I->getParent()->begin();
    while (&*it != I && it != I->getParent()->end()) {
        ++it;
        ++idx;
    }
    return idx;
}

std::unordered_set<llvm::BasicBlock*> Utils::get_blocks_in_range(llvm::Function::iterator begin,
                                                                 llvm::Function::iterator end)
{
    std::unordered_set<llvm::BasicBlock*> blocks;
    blocks_in_range(&*begin, &*end, blocks);
    return blocks;
}

std::vector<llvm::BasicBlock*> Utils::get_blocks_in_bfs(llvm::Function::iterator begin, llvm::Function::iterator end)
{
    std::unordered_set<llvm::BasicBlock*> processed_blocks;
    std::list<llvm::BasicBlock*> work_list;
    std::vector<llvm::BasicBlock*> blocks;
    if (&*begin == &*end) {
        return blocks;
    }
    work_list.push_back(&*begin);
    while (!work_list.empty()) {
        llvm::BasicBlock* block = work_list.front();
        work_list.pop_front();
        if (!processed_blocks.insert(block).second) {
            continue;
        }
        if (block == &*end) {
            continue;
        }
        blocks.insert(blocks.begin(), block);
        auto it = succ_begin(block);
        while (it != succ_end(block)) {
            llvm::dbgs() << "succ of " << block->getName() << " " << (*it)->getName() << "\n";
            work_list.push_back(*it);
            ++it;
        }
    }
    return blocks;
}

unsigned Utils::get_function_instrs_count(llvm::Function& F)
{
    unsigned count = 0;
    for (auto& B : F) {
        count += B.getInstList().size();
    }
    return count;
}

}

;
