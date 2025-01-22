#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <queue>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "ConstantPropagation"

namespace
{

    struct ConstantPropagation : public FunctionPass
    {
        static char ID;
        ConstantPropagation() : FunctionPass(ID) {}

        // Extracts the register name from an instruction
        std::string getRegisterNameFromInstruction(const llvm::Instruction &ins)
        {
            std::string instrStr;
            llvm::raw_string_ostream rso(instrStr);
            ins.print(rso);
            rso.flush();
            std::string lhsRegisterName;
            std::istringstream stream(instrStr);
            stream >> lhsRegisterName;
            return lhsRegisterName;
        }

        // Extracts the register name from a value
        std::string getRegisterNameFromValue(Value *valueIns)
        {
            std::string registerName;
            llvm::raw_string_ostream rso(registerName);
            valueIns->printAsOperand(rso, false);
            rso.flush();
            return registerName;
        }

        // Outputs the final results of constant propagation
        void printFinalOutput(map<string, int> &insToVal, map<string, int> &insToLine, map<int, int> &lineToConstantVal)
        {
            map<int, int> lineToVal;
            for (auto &mp1 : insToVal)
            {
                lineToVal[insToLine[mp1.first]] = mp1.second;
            }
            for (auto &mp2 : lineToVal)
            {
                if (mp2.second != INT_MIN && mp2.second != INT_MAX)
                {
                    lineToConstantVal[mp2.first] = mp2.second;
                }
            }
        }

        // Checks if an instruction is not compare, store, or alloca
        bool isNotCompareStoreAlloca(llvm::Instruction &inst)
        {
            return !isa<ICmpInst>(&inst) && !isa<FCmpInst>(&inst) && !isa<StoreInst>(&inst) && !isa<AllocaInst>(&inst);
        }

        // Compares two maps to check for equality
        bool compareMaps(const map<string, int> &map1, const map<string, int> &map2)
        {
            return map1 == map2;
        }

        bool runOnFunction(Function &F) override
        {
            std::map<BasicBlock *, std::map<string, int>> IN, OUT;
            map<string, int> insToLine;
            map<int, Instruction *> lineToIns;
            std::map<string, int> outM;
            int line = 0;

            // Initialize instruction-to-line mappings and OUT map
            for (auto &BB : F)
            {
                for (auto &ins : BB)
                {
                    ++line;
                    std::string lhsRegisterName = getRegisterNameFromInstruction(ins);
                    if (insToLine.find(lhsRegisterName) == insToLine.end())
                    {
                        insToLine[lhsRegisterName] = line;
                        lineToIns[line] = &ins;
                    }

                    if (isa<LoadInst>(&ins) || isa<AllocaInst>(ins) || isa<BinaryOperator>(ins) || isa<ICmpInst>(&ins))
                    {
                        outM[lhsRegisterName] = INT_MAX;
                    }
                }
            }

            // Initialize IN and OUT maps for all blocks
            for (auto &BB : F)
            {
                OUT[&BB] = outM;
                IN[&BB] = outM;
            }

            // Set the IN map for the entry block to INT_MIN
            BasicBlock &startBlock = F.getEntryBlock();
            for (auto &mp : OUT[&startBlock])
            {
                IN[&startBlock][mp.first] = INT_MIN;
            }

            queue<BasicBlock *> q;
            q.push(&startBlock);

            // Worklist algorithm
            while (!q.empty())
            {
                BasicBlock *block = q.front();
                q.pop();

                map<string, int> inMapTemp = IN[block];

                // Perform the meet operation for predecessor blocks
                for (auto predBB : predecessors(block))
                {
                    for (auto &mp : OUT[predBB])
                    {
                        if (mp.second == INT_MIN || inMapTemp[mp.first] == INT_MIN)
                        {
                            inMapTemp[mp.first] = INT_MIN;
                        }
                        else if (inMapTemp[mp.first] == INT_MAX)
                        {
                            inMapTemp[mp.first] = mp.second;
                        }
                        else if (mp.second != INT_MAX && inMapTemp[mp.first] != mp.second)
                        {
                            inMapTemp[mp.first] = INT_MIN;
                        }
                    }
                }

                IN[block] = inMapTemp;
                map<string, int> outMapTemp = inMapTemp;

                // Analyze instructions in the block
                for (auto &ins : *block)
                {
                    std::string lhsRegisterName = getRegisterNameFromInstruction(ins);

                    if (auto *storeInst = dyn_cast<StoreInst>(&ins))
                    {
                        Value *value = storeInst->getValueOperand();
                        auto *constValue = dyn_cast<ConstantInt>(value);
                        Value *ptr = storeInst->getPointerOperand();
                        std::string ptrName = getRegisterNameFromValue(ptr);

                        if (constValue)
                        {
                            outMapTemp[ptrName] = constValue->getZExtValue();
                        }
                        else
                        {
                            std::string valueVarName = getRegisterNameFromValue(value);
                            outMapTemp[ptrName] = outMapTemp[valueVarName];
                        }
                    }
                    else if (auto *loadInst = dyn_cast<LoadInst>(&ins))
                    {
                        Value *ptr = loadInst->getPointerOperand();
                        std::string rhsVarName = getRegisterNameFromValue(ptr);
                        outMapTemp[lhsRegisterName] = outMapTemp[rhsVarName];
                    }
                    else if (ins.isBinaryOp())
                    {
                        Value *opr1 = ins.getOperand(0);
                        Value *opr2 = ins.getOperand(1);
                        std::string operand1 = getRegisterNameFromValue(opr1);
                        std::string operand2 = getRegisterNameFromValue(opr2);

                        int opr1Val = isa<ConstantInt>(opr1) ? cast<ConstantInt>(opr1)->getZExtValue() : outMapTemp[operand1];
                        int opr2Val = isa<ConstantInt>(opr2) ? cast<ConstantInt>(opr2)->getZExtValue() : outMapTemp[operand2];

                        int computedVal = INT_MIN;
                        if (opr1Val != INT_MIN && opr2Val != INT_MIN)
                        {
                            switch (ins.getOpcode())
                            {
                            case Instruction::Add:
                                computedVal = opr1Val + opr2Val;
                                break;
                            case Instruction::Sub:
                                computedVal = opr1Val - opr2Val;
                                break;
                            case Instruction::Mul:
                                computedVal = opr1Val * opr2Val;
                                break;
                            case Instruction::SDiv:
                                computedVal = (opr2Val != 0) ? opr1Val / opr2Val : INT_MIN;
                                break;
                            default:
                                computedVal = INT_MIN;
                            }
                        }

                        outMapTemp[lhsRegisterName] = computedVal;
                    }

                    if (auto *cmpInst = dyn_cast<ICmpInst>(&ins))
                    {
                        Value *opr1 = cmpInst->getOperand(0);
                        Value *opr2 = cmpInst->getOperand(1);
                        std::string operand1 = getRegisterNameFromValue(opr1);
                        std::string operand2 = getRegisterNameFromValue(opr2);

                        int opr1Val = isa<ConstantInt>(opr1) ? cast<ConstantInt>(opr1)->getZExtValue() : outMapTemp[operand1];
                        int opr2Val = isa<ConstantInt>(opr2) ? cast<ConstantInt>(opr2)->getZExtValue() : outMapTemp[operand2];

                        if (opr1Val == INT_MIN || opr2Val == INT_MIN)
                        {
                            outMapTemp[lhsRegisterName] = 2;
                        }
                        else
                        {
                            outMapTemp[lhsRegisterName] = (opr1Val == opr2Val);
                        }
                    }

                    if (auto *brInst = dyn_cast<BranchInst>(&ins))
                    {
                        if (brInst->isConditional())
                        {
                            Value *cond = brInst->getCondition();
                            std::string condVar = getRegisterNameFromValue(cond);
                            int condVal = outMapTemp[condVar];

                            if (!compareMaps(OUT[block], outMapTemp))
                            {
                                if (condVal == 1)
                                {
                                    q.push(brInst->getSuccessor(0));
                                }
                                else if (condVal == 0)
                                {
                                    q.push(brInst->getSuccessor(1));
                                }
                                else
                                {
                                    q.push(brInst->getSuccessor(0));
                                    q.push(brInst->getSuccessor(1));
                                }
                            }
                        }
                        else
                        {
                            if (!compareMaps(OUT[block], outMapTemp))
                            {
                                q.push(brInst->getSuccessor(0));
                            }
                        }

                        OUT[block] = outMapTemp;
                    }
                }
            }

            // Replace constants in the instructions
            map<int, int> lineToConstantVal;
            for (auto &BB : F)
            {
                printFinalOutput(OUT[&BB], insToLine, lineToConstantVal);
            }

            for (auto &mp : lineToConstantVal)
            {
                Instruction *ins = lineToIns[mp.first];
                if (isNotCompareStoreAlloca(*ins))
                {
                    Constant *constant = ConstantInt::get(ins->getType(), mp.second);
                    ins->replaceAllUsesWith(constant);
                    ins->eraseFromParent();
                }
            }

            errs() << F;
            return true;
        }
    };

} // end of anonymous namespace

char ConstantPropagation::ID = 0;
static RegisterPass<ConstantPropagation> X("ConstantPropagation", "Constant Propagation Pass",
                                           false /* Only looks at CFG */,
                                           false /* Analysis Pass */);
