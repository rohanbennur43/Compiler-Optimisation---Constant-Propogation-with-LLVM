#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/CFG.h"
#include <string>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "HelloPass"

namespace {
  struct HelloPass : public FunctionPass {
    static char ID;
    HelloPass() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      errs() << "HelloPass runOnFunction: " << F.getName() << "\n";

      for (auto &basic_block : F) {
        errs() << "Basic Block: " << basic_block.getName() << "\n";

        int predecessorCount = 0;
        int successorCount = 0;

        for (auto *pred : predecessors(&basic_block)) {
          predecessorCount++;
        }
        errs() << "Number of predecessors for block: " << predecessorCount << "\n";

        for (auto *succ : successors(&basic_block)) {
          successorCount++;
        }
        errs() << "Number of successors for block: " << successorCount << "\n";

        errs() << "\n";

        /*
        // Analyze each instruction in the basic block
        for (auto &inst : basic_block) {
          errs() << inst << "\n"; // Print the instruction

          // Check for load and store instructions
          if (inst.getOpcode() == Instruction::Load) {
            errs() << "This is Load\n";
          } else if (inst.getOpcode() == Instruction::Store) {
            errs() << "This is Store\n";
          }

          // Check for binary operations
          if (inst.isBinaryOp()) {
            errs() << "Op Code: " << inst.getOpcodeName() << "\n"; 
            switch (inst.getOpcode()) {
              case Instruction::Add:
                errs() << "This is Addition\n";
                break;
              case Instruction::Sub:
                errs() << "This is Subtraction\n";
                break;
              case Instruction::Mul:
                errs() << "This is Multiplication\n";
                break;
              case Instruction::SDiv:
                errs() << "This is Division\n";
                break;
              default:
                break;
            }

            // Print operands of the instruction
            auto *ptr = dyn_cast<User>(&inst);
            for (auto it = ptr->op_begin(); it != ptr->op_end(); ++it) {
              errs() << "\tOperand: " << *(*it) << "\n";
            }
          }
        }
        */
      }
      return false; // Indicates that the function was not modified
    }
  }; // end of HelloPass
} // end of anonymous namespace

char HelloPass::ID = 0;
static RegisterPass<HelloPass> X("Hello", "Hello Pass",
                                   false /* Only looks at CFG */,
                                   false /* Analysis Pass */);

