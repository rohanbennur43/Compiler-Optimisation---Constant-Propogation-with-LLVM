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

#define DEBUG_TYPE "SSAConstantPropagation"

namespace
{

    struct SSAConstantPropagation : public FunctionPass
    {

        static char ID;
        SSAConstantPropagation() : FunctionPass(ID) {}
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

        bool VisitPhiReachable(BasicBlock *source, BasicBlock *dest, std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, bool> &ExecutableFlag, std::map<llvm::BasicBlock *, std::set<llvm::BasicBlock *>> &AdjacencyList, std::map<BasicBlock *, bool> Visited)
        {
            std::queue<BasicBlock *> q;
            q.push(source);
            // errs() << "Top and dest are" << source << " " << dest << " " << ExecutableFlag[{source, dest}] << "\n";
            while (!q.empty())
            {
                BasicBlock *top = q.front();
                // errs() << top << "Inside Visit phi\n";

                q.pop();
                if (top == dest)
                {
                    return true;
                }
                Visited[top] = true;

                for (auto node : AdjacencyList[top])
                {
                    if (ExecutableFlag[{top, node}] && !Visited[node])
                    {
                        q.push(node);
                    }
                }
            }
            return false;
        }

        int getPhiOperandVal(llvm::Value *Operand, BasicBlock *dest, std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, bool> &ExecutableFlag, std::map<llvm::BasicBlock *, std::set<llvm::BasicBlock *>> &AdjacencyList, std::map<BasicBlock *, bool> Visited, std::map<Instruction *, int> &insConstantVal, Instruction *ins)
        {
            if (auto *ConstInt = llvm::dyn_cast<llvm::ConstantInt>(Operand))
            {
                int OperandVal = ConstInt->getSExtValue();
                return OperandVal;
            }
            if (auto *DefiningInst = llvm::dyn_cast<llvm::Instruction>(Operand))
            {
                llvm::BasicBlock *source = DefiningInst->getParent();
                if (VisitPhiReachable(source, dest, ExecutableFlag, AdjacencyList, Visited))
                {
                    Instruction *ins = llvm::dyn_cast<llvm::Instruction>(Operand);
                    // errs() << "This is visited here" << *ins << source->getName() << "----" << dest->getName() << "\n";
                    return insConstantVal[ins];
                }
                else
                {
                    // errs() << "Not visited here" << *Operand << "\n";
                    return INT_MAX;
                }
            }
            return INT_MAX;
        }

        int ComputeMeetValue(int Operand1Val, int Operand2Val)
        {
            int ComputedVal;
            if (Operand1Val == INT_MIN || Operand2Val == INT_MIN)
            {
                ComputedVal = INT_MIN;
            }
            else if (Operand1Val == INT_MAX)
            {
                ComputedVal = Operand2Val;
            }
            else if (Operand2Val != INT_MAX && Operand1Val != Operand2Val)
            {
                ComputedVal = INT_MIN;
            }
            return ComputedVal;
        }

        int VisitExpression(BasicBlock *block, std::map<Instruction *, int> &insConstantValue, std::queue<std::pair<Instruction *, Instruction *>> &SSAWorkList, std::queue<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> &FlowWorkList)
        {
            int condition = 0;
            // errs() << "Inside VisitExpression" << *block << "\n";
            for (auto &ins : *block)
            {
                // errs() << "Instruction inside visit expression" << ins << "\n";
                if (ins.isBinaryOp())
                {
                    // errs() << "Coming inside the binary operation" << ins << "}}}}}}}}}}}}}}}}}}}}}}}}}}]\n";

                    Value *opr1 = ins.getOperand(0); // Left operand
                    Value *opr2 = ins.getOperand(1); // Right operand

                    auto *binaryInst = cast<BinaryOperator>(&ins);
                    int opr1Val, opr2Val;

                    // Retrieve operand values
                    if (auto *operand1 = dyn_cast<ConstantInt>(opr1))
                    {
                        opr1Val = operand1->getZExtValue();
                    }
                    else
                    {
                        auto *Inst = llvm::dyn_cast<llvm::Instruction>(opr1);
                        opr1Val = insConstantValue[Inst];
                    }

                    if (auto *operand2 = dyn_cast<ConstantInt>(opr2))
                    {
                        opr2Val = operand2->getZExtValue();
                    }
                    else
                    {
                        auto *Inst = llvm::dyn_cast<llvm::Instruction>(opr2);
                        opr2Val = insConstantValue[Inst];
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
                    // errs() << ins << "--------val before assignment is" << insConstantValue[&ins] << "\n";
                    // errs() << ins << "--------val is" << computedVal << "\n";

                    if (insConstantValue[&ins] != computedVal)
                    {

                        insConstantValue[&ins] = computedVal;
                        for (auto *user : ins.users())
                        {
                            // We check if the user is an instruction and print it
                            auto *userInstr = llvm::dyn_cast<llvm::Instruction>(user);
                            // errs() << "User: " << *userInstr << "-----\n";
                            // errs() << "Adding entry in SSA Worklist " << ins << "-" << *userInstr << "\n";
                            SSAWorkList.push({&ins, userInstr});
                        }
                    }
                    // errs() << "After visit expression" << ins << " " << *opr1 << "-" << opr1Val << " " << *opr2 << "-" << opr2Val << "\n";
                }
                else if (auto *cmpInst = dyn_cast<ICmpInst>(&ins))
                {
                    // Operands of the comparison
                    Value *opr1 = cmpInst->getOperand(0); // Left-hand side operand
                    Value *opr2 = cmpInst->getOperand(1);
                    int opr1Val, opr2Val;
                    if (auto *operand1 = dyn_cast<ConstantInt>(opr1))
                    {
                        opr1Val = operand1->getZExtValue();
                    }
                    else
                    {
                        auto *Inst = llvm::dyn_cast<llvm::Instruction>(opr1);
                        opr1Val = insConstantValue[Inst];
                    }

                    if (auto *operand2 = dyn_cast<ConstantInt>(opr2))
                    {
                        opr2Val = operand2->getZExtValue();
                    }
                    else
                    {
                        auto *Inst = llvm::dyn_cast<llvm::Instruction>(opr2);
                        opr2Val = insConstantValue[Inst];
                    }
                    // errs() << "<<<<<<<<<<<<<<<<<<<<<<<\n";
                    // for (auto mp : insConstantValue)
                    // {
                    //     errs() << mp.first << " " << mp.second << "\n";
                    // }
                    // errs() << INT_MAX << " " << INT_MIN << "Coming here++++++++" << ins << *opr1 << "-" << opr1Val << " " << *opr2 << "-" << opr2Val << "\n";
                    ICmpInst::Predicate predicate = cmpInst->getPredicate();
                    if (opr2Val == INT_MIN || opr1Val == INT_MIN)
                    {
                        // we assign the val to 2 here, if were not able to evaluate the conditional statement
                        // in this case, both the true and false block are added to the queue
                        condition = 2;
                    }
                    else
                    {
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
                    }
                    // if (auto *branchInst = llvm::dyn_cast<BranchInst>(cmpInst->getNextNode()))
                    // {
                    //     if (branchInst->isConditional())
                    //     {
                    //         if (condition == 1)
                    //         {
                    //             errs() << "Adding True block: " << branchInst->getSuccessor(0)->getName() << " to the queue.\n";
                    //             FlowWorkList.push({block, branchInst->getSuccessor(0)});
                    //         }
                    //         else if (condition == 0)
                    //         {
                    //             errs() << "Adding False block: " << branchInst->getSuccessor(1)->getName() << " to the queue.\n";
                    //             FlowWorkList.push({block, branchInst->getSuccessor(1)});
                    //         }
                    //         else
                    //         {
                    //             errs() << "Adding both True and False blocks due to undefined condition.\n";
                    //             FlowWorkList.push({block, branchInst->getSuccessor(0)});
                    //             FlowWorkList.push({block, branchInst->getSuccessor(1)});
                    //         }
                    //     }
                    //     else
                    //     {
                    //         errs() << "Branch instruction is not conditional.\n";
                    //     }
                    // }
                    // else
                    // {
                    //     errs() << "No branch instruction found after the compare instruction.\n";
                    // }
                    // errs() << "Condition value is" << condition << "\n";
                }
                else if (auto *branchInst = dyn_cast<BranchInst>(&ins))
                {
                    if (branchInst->isConditional())
                    {
                        if (condition == 1)
                        {
                            // errs() << "Adding True block: " << branchInst->getSuccessor(0)->getName() << " to the queue.\n";
                            FlowWorkList.push({block, branchInst->getSuccessor(0)});
                        }
                        else if (condition == 0)
                        {
                            // errs() << "Adding False block: " << branchInst->getSuccessor(1)->getName() << " to the queue.\n";
                            FlowWorkList.push({block, branchInst->getSuccessor(1)});
                        }
                        else
                        {
                            // errs() << "Adding both True and False blocks due to undefined condition.\n";
                            FlowWorkList.push({block, branchInst->getSuccessor(0)});
                            FlowWorkList.push({block, branchInst->getSuccessor(1)});
                        }
                    }
                    else
                    {
                        BasicBlock *nextBlock = branchInst->getSuccessor(0);
                        // errs() << "Inside a non conditional instruction in visit phi\n";
                        FlowWorkList.push({block, branchInst->getSuccessor(0)});
                    }
                }
                // else if (isa<llvm::PHINode>(ins))
                // {
                //     llvm::PHINode *phiInst = cast<llvm::PHINode>(&ins);
                //     errs() << "Found a PHI node: " << *phiInst << "\n";
                //     for (auto *user : ins.users())
                //     {
                //         // We check if the user is an instruction and print it
                //         auto *userInstr = llvm::dyn_cast<llvm::Instruction>(user);
                //         // errs() << "User: " << *userInstr << "-----\n";
                //         errs() << "Adding entry in SSA Worklist from visit phi " << ins << "-" << *userInstr << "\n";
                //         SSAWorkList.push({&ins, userInstr});
                //     }
                // }
            }
            return 0;
        }

        void visitPhi(Instruction &ins, BasicBlock *destNode, std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, bool> &ExecutableFlag, std::map<llvm::BasicBlock *, std::set<llvm::BasicBlock *>> &AdjacencyList, std::map<BasicBlock *, bool> Visited, std::map<Instruction *, int> &insConstantVal, std::queue<std::pair<Instruction *, Instruction *>> &SSAWorkList)
        {
            auto *Phi = llvm::cast<llvm::PHINode>(&ins);

            // Iterate over all incoming values of the PHI node
            // errs() << "Phi instruction" << ins << "\n";

            llvm::Value *Operand1 = Phi->getIncomingValue(0);
            llvm::BasicBlock *IncomingBlock1 = Phi->getIncomingBlock(0);
            llvm::Value *Operand2 = Phi->getIncomingValue(1);
            llvm::BasicBlock *IncomingBlock2 = Phi->getIncomingBlock(1);
            int Operand1Val, Operand2Val, ComputedVal;

            Operand1Val = getPhiOperandVal(Operand1, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, &ins);
            Operand2Val = getPhiOperandVal(Operand2, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, &ins);
            if (Operand1Val == INT_MIN || Operand2Val == INT_MIN)
            {
                ComputedVal = INT_MIN;
            }
            else if (Operand1Val == INT_MAX)
            {
                ComputedVal = Operand2Val;
            }
            else if (Operand2Val == INT_MAX)
            {
                ComputedVal = Operand1Val;
            }
            else if (Operand1Val != Operand2Val)
            {
                ComputedVal = INT_MIN;
            }
            else
            {
                ComputedVal = Operand1Val;
            }

            if (insConstantVal[&ins] != ComputedVal)
            {
                insConstantVal[&ins] = ComputedVal;
                llvm::PHINode *phiInst = cast<llvm::PHINode>(&ins);
                // errs() << "Found a PHI node: " << *phiInst << "\n";
                for (auto *user : ins.users())
                {
                    // We check if the user is an instruction and print it
                    auto *userInstr = llvm::dyn_cast<llvm::Instruction>(user);
                    // errs() << "User: " << *userInstr << "-----\n";
                    // errs() << "Adding entry in SSA Worklist from visit phi " << ins << "-" << *userInstr << "\n";
                    SSAWorkList.push({&ins, userInstr});
                }
            }

            // errs() << "<<<<<<<<<<<<<<<<<<<<<<<\n";
            // for (auto mp : insConstantVal)
            // {
            //     errs() << mp.first << " " << mp.second << "\n";
            // }
            // errs() << "Value of operand1 in visit phi" << *Operand1 << " " << Operand1Val << "\n";
            // errs() << "Value of operand2 in visit phi" << *Operand2 << " " << Operand2Val << "\n";
            // errs() << "The ins to constant value is" << ins << "->" << insConstantVal[&ins] << "\n";

            // errs() << "Instruction:" << destNode->getName() << " " << ins << "\n";
            // bool check = visitPhi(sourceNode, destNode, ExecutableFlag, AdjacencyList, Visited);
            // errs() << check << "check check\n";
        }
        bool runOnFunction(Function &F) override
        {

            std::queue<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> FlowWorkList;
            std::queue<std::pair<Instruction *, Instruction *>> SSAWorkList;
            std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, bool> ExecutableFlag;
            std::map<llvm::BasicBlock *, int> nodeVisits;
            std::map<llvm::BasicBlock *, std::set<llvm::BasicBlock *>> AdjacencyList;
            std::map<llvm::BasicBlock *, bool> Visited;
            std::map<string, int> varRegisters;
            std::map<Instruction *, int> insConstantVal;

            for (auto &BB : F)
            {
                // errs() << "--------------" << BB.getName() << &BB << "-----------\n";
                for (auto &ins : BB)
                {
                    // errs() << &ins << " -- " << ins << "\n";
                    std::string lhsRegisterName;
                    lhsRegisterName = getregisternamefromInstruction(ins);
                    // Perform all type checks in a single if condition
                    if (auto *loadInst = dyn_cast<LoadInst>(&ins))
                    {
                        varRegisters[lhsRegisterName] = INT_MAX;
                    }
                    else if (isa<AllocaInst>(ins))
                    {
                        varRegisters[lhsRegisterName] = INT_MAX;
                    }
                    else if (isa<BinaryOperator>(ins))
                    {
                        varRegisters[lhsRegisterName] = INT_MAX;
                    }
                    else if (auto *cmpInst = dyn_cast<ICmpInst>(&ins))
                    {
                        varRegisters[lhsRegisterName] = INT_MAX;
                    }
                    else if (llvm::isa<llvm::PHINode>(&ins))
                    {
                        varRegisters[lhsRegisterName] = INT_MAX;
                    }
                    insConstantVal[&ins] = INT_MAX;
                }
                // errs() << "=====END OF BASIC BLOCK=====" << "\n";

                for (auto *succ : successors(&BB))
                {
                    AdjacencyList[&BB].insert(succ);
                    pair<BasicBlock *, BasicBlock *> p;
                    p = {&BB, succ};
                    ExecutableFlag[p] = false;
                    // if (&BB == &F.getEntryBlock())
                    // {
                    //     FlowWorkList.push(p);
                    // }
                }

                nodeVisits[&BB] = 0;
                Visited[&BB] = false;
            }

            // errs() << "\n\n";
            pair<BasicBlock *, BasicBlock *> dummy;
            dummy = {NULL, &F.getEntryBlock()};
            FlowWorkList.push(dummy);
            ExecutableFlag[dummy] = false;
            // while (!FlowWorkList.empty())
            // {
            //     pair<BasicBlock *, BasicBlock *> p = FlowWorkList.front();
            //     FlowWorkList.pop();
            //     errs() << p.first << " " << p.second << "\n";
            // }
            // for (auto mp : AdjacencyList)
            // {
            //     errs() << mp.first << "-------------\n";
            //     for (auto block : mp.second)
            //     {
            //         errs() << *block << " ";
            //     }
            //     errs() << "\n";
            // }

            while (!FlowWorkList.empty() || !SSAWorkList.empty())
            {
                while (!FlowWorkList.empty())
                {

                    pair<BasicBlock *, BasicBlock *> edge = FlowWorkList.front();
                    FlowWorkList.pop();
                    // errs() << "FlowWorkList queue size" << FlowWorkList.size() << "\n";
                    // if (edge.first)
                    // {
                    //     errs() << "-----------FLow Worklist queue---------------" << edge.first->getName() << " " << edge.second->getName() << "\n";
                    // }
                    // else
                    // {
                    //     errs() << "-----------FLow Worklist queue---------------" << edge.second->getName() << "\n";
                    // }

                    if (!ExecutableFlag[edge])
                    {
                        ExecutableFlag[edge] = true;

                        BasicBlock *sourceNode = edge.first;
                        BasicBlock *destNode = edge.second;
                        nodeVisits[destNode]++;
                        for (auto &ins : *destNode)
                        {
                            if (llvm::isa<llvm::PHINode>(&ins))
                            {
                                visitPhi(ins, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, SSAWorkList);
                                // auto *Phi = llvm::cast<llvm::PHINode>(&ins);

                                // // Iterate over all incoming values of the PHI node
                                // errs() << "Phi instruction" << ins << "\n";

                                // llvm::Value *Operand1 = Phi->getIncomingValue(0);
                                // llvm::BasicBlock *IncomingBlock1 = Phi->getIncomingBlock(0);
                                // llvm::Value *Operand2 = Phi->getIncomingValue(1);
                                // llvm::BasicBlock *IncomingBlock2 = Phi->getIncomingBlock(1);
                                // int Operand1Val, Operand2Val, ComputedVal;

                                // Operand1Val = getPhiOperandVal(Operand1, IncomingBlock1, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, &ins);
                                // Operand2Val = getPhiOperandVal(Operand2, IncomingBlock2, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, &ins);
                                // if (Operand1Val == INT_MIN || Operand2Val == INT_MIN)
                                // {
                                //     ComputedVal = INT_MIN;
                                // }
                                // else if (Operand1Val == INT_MAX)
                                // {
                                //     ComputedVal = Operand2Val;
                                // }
                                // else if (Operand2Val == INT_MAX)
                                // {
                                //     ComputedVal = Operand1Val;
                                // }
                                // else if (Operand1Val != Operand2Val)
                                // {
                                //     ComputedVal = INT_MIN;
                                // }

                                // insConstantVal[&ins] = ComputedVal;
                                // // errs() << "<<<<<<<<<<<<<<<<<<<<<<<\n";
                                // // for (auto mp : insConstantVal)
                                // // {
                                // //     errs() << mp.first << " " << mp.second << "\n";
                                // // }
                                // errs() << "The ins to constant value is" << ins << " " << insConstantVal[&ins] << "\n";
                                // errs() << "Value of operand1 in visit phi" << Operand1Val << "\n";
                                // errs() << "Value of operand2 in visit phi" << Operand2Val << "\n";

                                // errs() << "Instruction:" << destNode->getName() << " " << ins << "\n";
                                // // bool check = visitPhi(sourceNode, destNode, ExecutableFlag, AdjacencyList, Visited);
                                // // errs() << check << "check check\n";
                            }
                        }
                        // errs() << "+++++++++++-----+++++++++++++++++" << nodeVisits[edge.second] << edge.second << "\n";
                        if (nodeVisits[edge.second] == 1)
                        {
                            nodeVisits[edge.second]++;
                            VisitExpression(destNode, insConstantVal, SSAWorkList, FlowWorkList);
                        }

                        // errs() << VisitExpression(destNode, insConstantVal, SSAWorkList, FlowWorkList) << "\n";

                        // errs() << VisitExpression(destNode, insConstantVal, SSAWorkList, FlowWorkList) << "\n";

                        // if the node contains one outgoing CFGedge
                        // then add the edge to FlowWorkList
                        // else{

                        // }
                    }
                    // errs() << "After Flow worklist ----------------------\n";
                    // for (auto mp : insConstantVal)
                    // {
                    //     errs() << *mp.first << " " << mp.second << "\n";
                    // }
                }

                while (!SSAWorkList.empty())
                {
                    std::pair<Instruction *, Instruction *> edge = SSAWorkList.front();
                    SSAWorkList.pop();
                    // errs() << "-----------SSA Worklist queue---------------" << *edge.first << "--" << *edge.second << "\n";

                    if (llvm::isa<llvm::PHINode>(edge.second))
                    {
                        // errs() << "<<<<<<<<<<<<<<<<<<<<<<<\n";
                        // for (auto mp : insConstantVal)
                        // {
                        //     errs() << mp.first << " " << mp.second << "\n";
                        // }
                        // errs() << "It is a phi node. Will do visit phi\n";
                        BasicBlock *destNode = edge.second->getParent();
                        visitPhi(*edge.second, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, SSAWorkList);
                    }
                    else
                    {
                        BasicBlock *destNode = edge.second->getParent();
                        if (nodeVisits[destNode])
                        {
                            // errs() << "Assignment statements destNode. Visiting expression in" << destNode << "\n";

                            VisitExpression(destNode, insConstantVal, SSAWorkList, FlowWorkList);
                        }
                    }

                    // errs() << "After SSA worklist ----------------------\n";
                    // for (auto mp : insConstantVal)
                    // {
                    //     errs() << *mp.first << " " << mp.second << "\n";
                    // }
                }
            }
            // for (auto mp : ExecutableFlag)
            // {
            //     errs() << mp.first.first << "->" << mp.first.second << "         " << mp.second << "\n";
            // }

            // for (auto mp : insConstantVal)
            // {
            //     errs() << mp.first << " " << *mp.first << " " << mp.second << "\n";
            //     if(mp.second!=INT_MIN && mp.second!=INT_MAX){
            //         errs()<<"Constant value"<<mp.first<<" "<<mp.second<<"\n";
            //     }
            // }

            for (auto mp : insConstantVal)
            {
                Instruction *inst = mp.first;
                int constVal = mp.second;

                // Debugging output for each instruction and its associated constant value
                // errs() << inst << " " << *inst << " " << constVal << "\n";

                // Only process valid constant values
                if (constVal != INT_MIN && constVal != INT_MAX)
                {
                    // errs() << "Constant value: " << inst << " " << constVal << "\n";

                    // Create an LLVM constant from the integer value
                    Constant *constant = ConstantInt::get(inst->getType(), constVal);

                    // Replace all uses of the instruction with the constant
                    inst->replaceAllUsesWith(constant);

                    // Erase the replaced instruction from the parent block
                    inst->eraseFromParent();
                }
            }
            // errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
            errs() << F;
            // errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
            // for (auto &BB : F)
            // {
            //     errs() << "--------------" << BB.getName() << &BB << "-----------\n";
            //     for (auto &ins : BB)
            //     {
            //         errs() << ins << "\n";
            //     }
            // }

            // for (auto &BB : F)
            // {
            //     errs() << "--------------" << BB.getName() << "------------\n";
            //     for (auto &ins : BB)
            //     {
            //         errs() << ins << '\n';
            //     }
            // }
            return true; // Indicate this is a Transform pass
        }
    }; // end of struct SSAConstantPropagation
} // end of anonymous namespace

char SSAConstantPropagation::ID = 0;
static RegisterPass<SSAConstantPropagation> X("SSAConstantPropagation", "SSAConstantPropagation Pass",
                                              false /* Only looks at CFG */,
                                              true /* Tranform Pass */);

