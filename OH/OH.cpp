#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h" 
#include "../CutVertice/CutVerticesPass.h"

using namespace llvm;
namespace {
	struct OHPass : public FunctionPass {
		static char ID;
		OHPass() : FunctionPass(ID) {}
		virtual bool runOnFunction(Function &F){
			bool didModify = false;
			dbgs()<<"In function:" << F.getName()<<"\n";
			//TODO: make sure hashMe and logHash are not shipped with the binary
			if (F.getName().equals("hashMe")||F.getName().equals("logHash")) return false;
			for (auto &B : F) {
				for (auto &I : B) {			
					//dbgs() << I << I.getOpcodeName() << "\n";
					if (auto* op = dyn_cast<BinaryOperator>(&I)) {
					// Insert *after* `op`.
					dbgs()<<"Binay operator\n";
					updateHash(&B, &I, op, false);
					didModify =true;
					} else if (CmpInst* cmpInst = dyn_cast<CmpInst>(&I)){
						didModify = handleCmp(cmpInst,&B);
					} else if (StoreInst* storeInst = dyn_cast<StoreInst>(&I)){
						didModify = handleStore(storeInst, &B);
					} //TODO: else if (handle switch case and other conditions)
				//terminator indicates the last block
				else if(ReturnInst *RI = dyn_cast<ReturnInst>(&I)){
					if (!F.getName().equals("main")) continue;
					// Insert *before* ret
					dbgs() << "**returnInst**\n";
					//printHash(&B, RI, true);	
					//didModify = true;
					// Slicer adds multiple return instrustions to a function, thus we need to return to avoid adding multiple prints
					return true;
				} else if (LoadInst *loadInst = dyn_cast<LoadInst>(&I)){
					didModify = handleLoad(loadInst,&B);
				}
				}
			}
			return didModify;
		}

		/*virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		  AU.addRequired<CutVerticesPass>();
		  AU.setPreservesAll();
		  }*/
		bool handleLoad(LoadInst *loadInst, BasicBlock *BB){
			dbgs() << "**HandleLoad**\n";
			loadInst->print(dbgs());
			Value *val = loadInst -> getOperand(0);
			dbgs() <<val->getName();
			//skip the store instruction if its referring to a pointer
			//TODO: make sure this does not have harmful side effects
			//if(val->getType()->isIntOrIntVectorTy()){
			loadInst->print(dbgs());
			dbgs()<<"\n";
			//Insert *after* cmp;
			updateHash(BB, loadInst, loadInst, false);
			//std::string type_str;
			//llvm::raw_string_ostream rso(type_str);
			//val->print(rso);
			//dbgs() << "Handled  Type:"<<val->getType()->isPointerTy()<<" "<<rso.str()<<"\n";
			return true;
			//} else {
			//	dbgs()<<"skipped load pointer:";
			//	val->getType()->print(dbgs());
			//	dbgs()<<"\n";
			//}

			return false;
		}

		bool handleStore(StoreInst *storeInst, BasicBlock *BB){
			dbgs() << "**HandleStore**\n";
			//if(storeInst->getNumOperands()>0){
			Value *val = storeInst -> getOperand(1);
			//skip the store instruction if its referring to a pointer
			//TODO: make sure this does not have harmful side effects
			//if(val->getType()->isIntegerTy()){
			storeInst->print(dbgs());
			dbgs()<<"\n";
			//Insert *after* cmp;
			updateHash(BB, storeInst, val, false);
			//std::string type_str;
			//llvm::raw_string_ostream rso(type_str);
			//val->print(rso);
			//dbgs() << "Handled  Type:"<<val->getType()->isPointerTy()<<" "<<rso.str()<<"\n";
			return true;
			//}

			//}
			return false;
		}
		bool handleCmp(CmpInst *cmpInst, BasicBlock *BB){
			//dbgs() << "**HandleCmp**\n";
			//first check whether cmp has two operands 
			if(cmpInst->getNumOperands() >=2){
				//get the left hand operand, i.e. condition value
				Value *secondOperand = cmpInst->getOperand(0);
				//Insert *before* cmp;
				updateHash(BB, cmpInst, secondOperand, false);
				return true;
			}
			return false;
		}
		void updateHash(BasicBlock *BB, Instruction *I, 
				Value *value, bool insertBeforeInstruction){
			//Make sure that the argument is int32
			if((value->getType()->isIntOrIntVectorTy() && 
						value->getType()->getIntegerBitWidth() == 32) /*||
												value->getType()->isDoubleTy()*/){

				LLVMContext& Ctx = BB->getParent()->getContext();
				// get BB parent -> Function -> get parent -> Module	
				Constant* hashFunc = BB->getParent()->getParent()->getOrInsertFunction(
						"hashMe", Type::getVoidTy(Ctx), Type::getInt32Ty(Ctx), NULL
						);
				IRBuilder <> builder(I);
				//auto insertPoint = ++builder.GetInsertPoint();

				if(insertBeforeInstruction){
					//dbgs()<<"inserting before\n";
					builder.SetInsertPoint(I);
				} else {
					//dbgs()<<"inserting after\n";
					//dbgs() << "FuncName: "<<BB->getParent()->getName()<<"\n";
					builder.SetInsertPoint(I->getNextNode());
				}

				Value *args = {value};
				//printArg(BB, &builder, value->getName());
				builder.CreateCall(hashFunc, args);
			} else { dbgs()<<"Cannot add to hash\n";
				value->print(dbgs());
				dbgs()<<"\n";
			}
		}
		void printArg(BasicBlock *BB, IRBuilder<> *builder, std::string valueName){
			LLVMContext &context = BB->getParent()->getContext();;
			std::vector<llvm::Type *> args;
			args.push_back(llvm::Type::getInt8PtrTy(context));
			// accepts a char*, is vararg, and returns int
			FunctionType *printfType =
				llvm::FunctionType::get(builder->getInt32Ty(), args, true);
			Constant *printfFunc =
				BB->getParent()->getParent()->getOrInsertFunction("printf", printfType);
			Value *formatStr = builder->CreateGlobalStringPtr("arg = %s\n");
			Value *argument = builder->CreateGlobalStringPtr(valueName);
			std::vector<llvm::Value *> values;
			values.push_back(formatStr);
			values.push_back(argument);
			builder->CreateCall(printfFunc, values);
		}
		void printHash(BasicBlock *BB, Instruction *I, bool insertBeforeInstruction){
			LLVMContext& Ctx = BB->getParent()->getContext();
			// get BB parent -> Function -> get parent -> Module 
			Constant* logHashFunc = BB->getParent()->getParent()->getOrInsertFunction(
					"logHash", Type::getVoidTy(Ctx),NULL);
			IRBuilder <> builder(I);
			//auto insertPoint = ++builder.GetInsertPoint();
			if(insertBeforeInstruction){
				//insertPoint--;
				//insertPoint--;
				builder.SetInsertPoint(I);
			} else {
				//dbgs() << "FuncName: "<<BB->getParent()->getName()<<"\n";
				builder.SetInsertPoint(I->getNextNode());
			}
			builder.CreateCall(logHashFunc);	
		}
	};
}

char OHPass::ID = 0;
static RegisterPass<OHPass> X("oh","runs oblivious hash transformation");
// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerOHPass(const PassManagerBuilder &,
		legacy::PassManagerBase &PM) {
	//PM.add(new CutVerticesPass());
	PM.add(new OHPass());
}
static RegisterStandardPasses
RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
		registerOHPass);
