#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "Graph.h"
#include <vector>
#include <algorithm>
using namespace llvm;

namespace {
	struct CutVerticesPass : public FunctionPass{
		static char ID;
		CutVerticesPass() : FunctionPass(ID){}

		virtual bool runOnFunction(Function &F){
			int id=0;
			Graph graph=Graph(0);
			for(auto& B : F){
				const TerminatorInst *TInst = B.getTerminator();
				(&B)->setName(std::to_string(graph.addnewBB(B)).c_str());
				for (unsigned I = 0, NSucc = TInst->getNumSuccessors(); I < NSucc; ++I) {
					BasicBlock *Succ = TInst->getSuccessor(I);
					Succ->setName(std::to_string(graph.addnewBB(*Succ)).c_str());
					graph.addEdge(graph.addnewBB(B),graph.addnewBB(*Succ));
					//errs() << F.getName() << " - Added: " << (&B)->getName() << " + " << Succ->getName()<< "\n";
				}
			}
			ids=graph.AP();
			//Push back entryBB
			ids.push_back(std::stoi(F.getEntryBlock().getName()));
			//Push back returnBBs
			for(auto& B : F){
				for (auto& I : B) {
					if (ReturnInst *RI = dyn_cast<ReturnInst>(&I)){
						ids.push_back(std::stoi(B.getName()));
					}		
				}
			}
            return false;
		}

		std::vector<int> getArray(){
			return ids;
		}

		std::vector<int> ids={};
	};
}
char CutVerticesPass::ID = 1;
// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
//static void registerCutVerticesPass(const PassManagerBuilder &,
//                           legacy::PassManagerBase &PM) {
//    PM.add(new CutVerticesPass());
//}
//static RegisterStandardPasses
//RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
//               registerCutVerticesPass);
