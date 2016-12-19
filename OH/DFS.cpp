#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopIterator.h"
#include <vector>
using namespace llvm;


// Not used right now

namespace { 
	struct DFSPass : public LoopPass{
		static char ID;
		DFSPass() : LoopPass(ID){}
		virtual bool runOnLoop(Loop *L, LPPassManager &LPM){
			LoopInfo &LI=getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
			LoopBlocksDFS DFS(L);
			DFS.perform(&LI);

			// Stash the DFS iterators before adding blocks to the loop.
			LoopBlocksDFS::RPOIterator BlockBegin = DFS.beginRPO();
			LoopBlocksDFS::RPOIterator BlockEnd = DFS.endRPO();

			for (unsigned It = 1; It != Count; ++It) {
			    std::vector<BasicBlock*> NewBlocks;

			    for (LoopBlocksDFS::RPOIterator BB = BlockBegin; BB != BlockEnd; ++BB) {
			    }
			}
		}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<LoopInfoWrapperPass>();
		}

	};
}