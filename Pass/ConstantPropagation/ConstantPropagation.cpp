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

        std::string getregisternamefromInstruction(const llvm::Instruction &ins)
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

        std::string getregisternameValue(Value *valueIns)
        {
            std::string regirsterName;
            llvm::raw_string_ostream rso(regirsterName);
            valueIns->printAsOperand(rso, false); // `false` omits the type information
            rso.flush();
            return regirsterName;
        }

        void printFinalOutput(map<string, int> &insToVal, map<string, int> &insToLine, map<int, int> &lineToConstantVal)
        {

            map<int, int> lineToVal;
            for (auto mp1 : insToVal)
            {
                lineToVal[insToLine[mp1.first]] = mp1.second;
            }
            for (auto mp2 : lineToVal)
            {
                if (mp2.second != INT_MIN && mp2.second != INT_MAX)
                {
                    lineToConstantVal[mp2.first] = mp2.second;
                    // errs() << mp2.first << ":" << mp2.second << "\n";
                }
            }
        }

        bool isNotCompareStoreAlloca(llvm::Instruction &inst)
        {
            // Check if the instruction is a compare
            if (llvm::isa<llvm::ICmpInst>(&inst) || llvm::isa<llvm::FCmpInst>(&inst))
            {
                return false;
            }
            // Check if the instruction is a store
            if (llvm::isa<llvm::StoreInst>(&inst))
            {
                return false;
            }
            // Check if the instruction is an alloca
            if (llvm::isa<llvm::AllocaInst>(&inst))
            {
                return false;
            }
            return true; // It's not a compare, store, or alloca
        }

        bool compareMaps(const map<string, int> &map1, const map<string, int> &map2)
        {
            bool mapsEqual = true;

            // Check if each key-value pair in map1 exists in map2 and matches
            for (const auto &pair : map1)
            {
                auto it = map2.find(pair.first);
                if (it == map2.end() || it->second != pair.second)
                {
                    mapsEqual = false;
                    /*errs() << "Difference found: Key = " << pair.first
                           << ", map1 Value = " << pair.second
                           << ", map2 Value = " << (it != map2.end() ? std::to_string(it->second) : "Not found") << "\n";*/
                }
            }

            // Check if map2 has any additional keys not in map1
            for (const auto &pair : map2)
            {
                if (map1.find(pair.first) == map1.end())
                {
                    mapsEqual = false;
                    /*
                    errs() << "Extra key in map2: Key = " << pair.first
                       << ", Value = " << pair.second << "\n";*/
                }
            }

            // Return the overall comparison result
            return mapsEqual;
        }
        bool runOnFunction(Function &F) override
        {

            // Map to hold constant propagation information with nullptr as "no value" indicator
            std::map<BasicBlock *, std::map<string, int>> IN, OUT;
            // stores the mapping of register name to the line number of its first instruction
            // ex:-  %1->1
            map<string, int> insToLine;
            map<int, Instruction *> lineToIns;
            std::map<string, int> outM;
            int line = 0;
            for (auto &BB : F)
            {
                // errs()<<"--------------"<<BB.getName()<<"-----------\n";
                for (auto &ins : BB)
                {
                    ++line;
                    // errs()<<line<<":-" << ins << "\n";
                    std::string lhsRegisterName;
                    lhsRegisterName = getregisternamefromInstruction(ins);
                    if (insToLine.find(lhsRegisterName) == insToLine.end())
                    {
                        insToLine[lhsRegisterName] = line;
                        lineToIns[line] = &ins;
                    }
                    // Perform all type checks in a single if condition
                    if (auto *loadInst = dyn_cast<LoadInst>(&ins))
                    {
                        outM[lhsRegisterName] = INT_MAX;
                    }
                    else if (isa<AllocaInst>(ins))
                    {
                        outM[lhsRegisterName] = INT_MAX;
                    }
                    else if (isa<BinaryOperator>(ins))
                    {
                        outM[lhsRegisterName] = INT_MAX;
                    }
                    else if (auto *cmpInst = dyn_cast<ICmpInst>(&ins))
                    {
                        outM[lhsRegisterName] = INT_MAX;
                    }
                }
                // errs() << "=====END OF BASIC BLOCK=====" << "\n";
            }
            /*
            // for (auto mp : outM)
            // {
            //     errs() << mp.first << " " << mp.second << "\n";
            // }*/

            // initialise the in and out of all the blocks to INT_MAX
            // INT_MAX - maps to top , and INT_MIN to bottom
            for (auto &BB : F)
            {
                OUT[&BB] = outM;
                IN[&BB] = outM;
            }

            // initialise the IN of entry block to INT_MIN
            BasicBlock &startBlock = F.getEntryBlock();
            for (auto mp : OUT[&startBlock])
            {
                IN[&startBlock][mp.first] = INT_MIN;
            }

            /*      for(auto mp:insToLine){
                            errs()<<mp.first<<"----"<<mp.second<<"\n";
                        }
            */

            // initialise the queue and add start block to run the worklist algorithm
            queue<BasicBlock *> q;
            q.push(&startBlock);
            while (!q.empty())
            {
                BasicBlock *block = q.front();
                q.pop();

                map<string, int> inMapTemp, outMapTemp;
                inMapTemp = IN[block];

                // perform the meet operation
                for (auto predBB : predecessors(block))
                {

                    for (auto mp : OUT[predBB])
                    {
                        if (mp.second == INT_MIN || inMapTemp[mp.first] == INT_MIN)
                        {
                            inMapTemp[mp.first] = INT_MIN;
                        }
                        else if (inMapTemp[mp.first] == INT_MAX)
                        {
                            // The variable is only coming from one block at this point
                            inMapTemp[mp.first] = mp.second;
                        }
                        else if (mp.second != INT_MAX && inMapTemp[mp.first] != mp.second)
                        {
                            inMapTemp[mp.first] = INT_MIN;
                        }
                    }
                }
                /*
                errs()<<"---------------meet values----------"<<block->getName()<<"\n";
                for(auto mp:inMapTemp){
                    errs()<<mp.first<<" "<<mp.second<<"\n";
                }*/
                IN[block] = inMapTemp;
                outMapTemp = inMapTemp;
                for (auto &ins : *block)
                {
                    // errs() << "Instruction: " << ins << "\n";
                    string lhsRegisterName = getregisternamefromInstruction(ins);

                    // Handle Store Instruction
                    if (ins.getOpcode() == Instruction::Store)
                    {
                        StoreInst *storeInst = cast<StoreInst>(&ins);
                        Value *value = storeInst->getValueOperand();
                        auto *constValue = dyn_cast<ConstantInt>(value);
                        Value *ptr = storeInst->getPointerOperand();
                        // checks if the value operand is constantint and stores it to map
                        if (constValue)
                        {
                            Value *ptr = storeInst->getPointerOperand();
                            std::string ptrName;
                            ptrName = getregisternameValue(ptr);
                            outMapTemp[ptrName] = constValue->getZExtValue();
                        } // if its not constantint, we assign the value from the map
                        else
                        {
                            string registerName = getregisternameValue(ptr);
                            string valueVarName = getregisternameValue(value);
                            outMapTemp[registerName] = outMapTemp[valueVarName];
                        }
                    }

                    // Handle Load Instruction
                    else if (ins.getOpcode() == Instruction::Load)
                    {
                        LoadInst *loadInst = cast<LoadInst>(&ins);
                        Value *ptr = loadInst->getPointerOperand();
                        string lhsRegisterName = getregisternamefromInstruction(ins);
                        string rhsVarName = getregisternameValue(ptr);
                        outMapTemp[lhsRegisterName] = outMapTemp[rhsVarName];
                    }

                    // Handle Binary Operations
                    if (ins.isBinaryOp())
                    {
                        Value *opr1 = ins.getOperand(0); // Left operand
                        Value *opr2 = ins.getOperand(1); // Right operand
                        string operand1 = getregisternameValue(opr1);
                        string operand2 = getregisternameValue(opr2);
                        string lhsregisterName = getregisternamefromInstruction(ins);

                        auto *binaryInst = cast<BinaryOperator>(&ins);
                        int opr1Val, opr2Val;

                        // Retrieve operand values
                        if (auto *lhsConst = dyn_cast<ConstantInt>(opr1))
                        {
                            opr1Val = lhsConst->getZExtValue();
                        }
                        else
                        {
                            opr1Val = outMapTemp[operand1];
                        }

                        if (auto *rhsConst = dyn_cast<ConstantInt>(opr2))
                        {
                            opr2Val = rhsConst->getZExtValue();
                        }
                        else
                        {
                            opr2Val = outMapTemp[operand2];
                        }

                        int computedVal = 0;
                        switch (binaryInst->getOpcode())
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
                        case Instruction::SDiv: // Signed division
                            if (opr2Val != 0)
                            {
                                computedVal = opr1Val / opr2Val;
                            }
                            else
                            {
                                // Handle division by zero case
                                // errs() << "Error: Division by zero\n";
                                computedVal = 0; // Or handle differently (e.g., error handling or setting to NaN)
                            }
                            break;
                        default:
                            computedVal = 0; // Default case if the operation is unknown
                        }
                        if (opr1Val == INT_MIN || opr2Val == INT_MIN)
                        {
                            // errs()<<"One of the operand is not a constant";
                            computedVal = INT_MIN;
                        }

                        outMapTemp[lhsregisterName] = computedVal;
                        // errs() << "Computed value: " << computedVal << "\n";
                    }

                    if (auto *cmpInst = dyn_cast<ICmpInst>(&ins))
                    {
                        // Operands of the comparison
                        Value *opr1 = cmpInst->getOperand(0); // Left-hand side operand
                        Value *opr2 = cmpInst->getOperand(1); // Right-hand side operand
                        string operand1 = getregisternameValue(opr1);
                        string operand2 = getregisternameValue(opr2);

                        int opr1Val, opr2Val;

                        if (auto *lhsConst = dyn_cast<ConstantInt>(opr1))
                        {
                            opr1Val = lhsConst->getZExtValue();
                        }
                        else
                        {
                            string operand1 = getregisternameValue(opr1);
                            opr1Val = outMapTemp[operand1];
                        }

                        if (auto *rhsConst = dyn_cast<ConstantInt>(opr2))
                        {
                            opr2Val = rhsConst->getZExtValue();
                        }
                        else
                        {
                            string operand2 = getregisternameValue(opr2);
                            opr2Val = outMapTemp[operand2];
                        }

                        ICmpInst::Predicate predicate = cmpInst->getPredicate();
                        if (opr2Val == INT_MIN || opr1Val == INT_MIN)
                        {
                            string registerName = getregisternamefromInstruction(ins);
                            outMapTemp[registerName] = 2;
                        }
                        // Now check if it's followed by a branch instruction (conditional)
                        if (auto *brInst = dyn_cast<BranchInst>(cmpInst->getNextNode()))
                        {
                            if (brInst->isConditional())
                            {
                                string registerName = getregisternamefromInstruction(ins);

                                if (opr2Val == INT_MIN || opr1Val == INT_MIN)
                                {
                                    string registerName = getregisternamefromInstruction(ins);
                                    // we assign the val to 2 here, if were not able to evaluate the conditional statement
                                    // in this case, both the true and false block are added to the queue
                                    outMapTemp[registerName] = 2;
                                }
                                else
                                {
                                    bool condition = false;
                                    switch (predicate)
                                    {
                                    case ICmpInst::ICMP_EQ:
                                        condition = (opr1Val == opr2Val);
                                        break;
                                    case ICmpInst::ICMP_NE:
                                        condition = (opr1Val != opr2Val);
                                        break;
                                    case ICmpInst::ICMP_SGT:
                                        condition = (opr1Val > opr2Val);
                                        break;
                                    case ICmpInst::ICMP_SLT:
                                        condition = (opr1Val < opr2Val);
                                        break;
                                    case ICmpInst::ICMP_SGE:
                                        condition = (opr1Val >= opr2Val);
                                        break;
                                    case ICmpInst::ICMP_SLE:
                                        condition = (opr1Val <= opr2Val);
                                        break;
                                    default:
                                        // errs() << "Unhandled comparison predicate\n";
                                        break;
                                    }
                                    outMapTemp[registerName] = condition;
                                }
                            }
                        }
                    }
                    else if (auto *brInst = dyn_cast<BranchInst>(&ins))
                    {
                        if (brInst->isConditional())
                        {
                            // Conditional branch: It has two successors
                            BasicBlock *trueBlock = brInst->getSuccessor(0);  // Block to jump to if condition is true
                            BasicBlock *falseBlock = brInst->getSuccessor(1); // Block to jump to if condition is false
                            Value *cond = brInst->getCondition();
                            string condlhsVar = getregisternameValue(cond);
                            int condStateVal = outMapTemp[condlhsVar];
                            outMapTemp.erase(condlhsVar);
                            // check if the OUT has stabilised. If it has stabilised, do not add the blocks to the queue
                            bool mapsEqual = compareMaps(OUT[block], outMapTemp);
                            if (!mapsEqual)
                            {
                                // add true if condition evaluated to true
                                if (condStateVal == 1)
                                {
                                    q.push(trueBlock);
                                }
                                // add falseBlock, if condition evaluated to false
                                else if (condStateVal == 0)
                                {
                                    q.push(falseBlock);
                                }
                                // add true and false block , if were not able to evaluate the condition
                                else
                                {
                                    q.push(trueBlock);
                                    q.push(falseBlock);
                                }
                            }
                        }
                        else
                        {
                            // Unconditional branch: Only one successor
                            BasicBlock *nextBlock = brInst->getSuccessor(0); // The only successor for an unconditional branch
                            bool mapsEqual = compareMaps(OUT[block], outMapTemp);
                            if (!mapsEqual)
                            {
                                q.push(nextBlock);
                            } // Add the next block to the queue
                        }

                        OUT[block] = outMapTemp;
                        /* errs()<<"-----------Block exit values-----------\n"; */
                        /* for(auto mp:OUT[block]){ */
                        /*     errs()<<mp.first<<" "<<mp.second<<"\n"; */
                        /* } */
                        /* errs()<<"zzzzzzzzzzzzzzzzzzzzzzzzz\n"; */

                        /* printFinalOutput(OUT[block],insToLine); */
                    }
                    else if (auto *returnInst = dyn_cast<ReturnInst>(&ins))
                    {
                        OUT[block] = outMapTemp;
                        /*
                        errs()<<"-----------Block exit values-----------\n";
                        for(auto mp:OUT[block]){
                            errs()<<mp.first<<" "<<mp.second<<"\n";
                        }
                        errs()<<"zzzzzzzzzzzzzzzzzzzzzzzzz\n";
                        */
                    }
                }
            }

            map<int, int> lineToConstantVal;
            for (auto &BB : F)
            {
                // errs() << "-----" << BB.getName() << "-----\n";
                printFinalOutput(OUT[&BB], insToLine, lineToConstantVal);
            }

            // errs() << F;

            for (auto mp : lineToConstantVal)
            {
                Instruction *ins = lineToIns[mp.first];
                if (isNotCompareStoreAlloca(*ins))
                {
                    // errs() << mp.first << " " << *lineToIns[mp.first] << " " << mp.second << "\n";
                    Constant *constant = ConstantInt::get(ins->getType(), mp.second);

                    // Replace all uses of the instruction with the constant

                    ins->replaceAllUsesWith(constant);

                    //             // Erase the replaced instruction from the parent block
                    ins->eraseFromParent();
                }
            }
            errs() << F;
            // for (auto mp : lineToIns)
            // {
            //     // errs() << mp.first << " " << mp.second << "\n";
            //     if (constantLines.find(mp.first) != constantLines.end())
            //     {
            //         errs() << mp.first << " " << *mp.second << "\n";
            //     }
            // }
            // for (auto &BB : F)
            // {
            //     for (auto mp : OUT[&BB])
            //     {
            //         if (mp.second != INT_MAX || mp.second != INT_MIN)
            //         {
            //             Constant *constant = ConstantInt::get(mp.first.getType(), mp.second);

            //             // Replace all uses of the instruction with the constant
            //             mp.first.replaceAllUsesWith(constant);

            //             // Erase the replaced instruction from the parent block
            //             mp.first.eraseFromParent();
            //         }
            //     }
            // }

            // errs() << F;

            return true;
        }
    };

} // end of anonymous namespace

char ConstantPropagation::ID = 0;
static RegisterPass<ConstantPropagation> X("ConstantPropagation", "",
                                           false /* Only looks at CFG */,
                                           false /* Analysis Pass */);
