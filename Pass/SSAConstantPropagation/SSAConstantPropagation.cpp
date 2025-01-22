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

        // Checks if the destination block is reachable from the source block
        bool visitPhiReachable(BasicBlock *source, BasicBlock *dest,
                               std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, bool> &ExecutableFlag,
                               std::map<llvm::BasicBlock *, std::set<llvm::BasicBlock *>> &AdjacencyList,
                               std::map<BasicBlock *, bool> Visited)
        {
            std::queue<BasicBlock *> q;
            q.push(source);

            while (!q.empty())
            {
                BasicBlock *top = q.front();
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

        // Retrieves the constant value for a PHI operand
        int getPhiOperandVal(llvm::Value *Operand, BasicBlock *dest,
                             std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, bool> &ExecutableFlag,
                             std::map<llvm::BasicBlock *, std::set<llvm::BasicBlock *>> &AdjacencyList,
                             std::map<BasicBlock *, bool> Visited,
                             std::map<Instruction *, int> &insConstantVal, Instruction *ins)
        {
            if (auto *ConstInt = llvm::dyn_cast<llvm::ConstantInt>(Operand))
            {
                return ConstInt->getSExtValue();
            }

            if (auto *DefiningInst = llvm::dyn_cast<llvm::Instruction>(Operand))
            {
                llvm::BasicBlock *source = DefiningInst->getParent();
                if (visitPhiReachable(source, dest, ExecutableFlag, AdjacencyList, Visited))
                {
                    return insConstantVal[llvm::dyn_cast<llvm::Instruction>(Operand)];
                }
            }

            return INT_MAX;
        }

        // Computes the meet value for SSA constants
        int computeMeetValue(int Operand1Val, int Operand2Val)
        {
            if (Operand1Val == INT_MIN || Operand2Val == INT_MIN)
            {
                return INT_MIN;
            }
            else if (Operand1Val == INT_MAX)
            {
                return Operand2Val;
            }
            else if (Operand2Val != INT_MAX && Operand1Val != Operand2Val)
            {
                return INT_MIN;
            }
            return Operand1Val;
        }

        // Evaluates expressions and updates the constant values
        int visitExpression(BasicBlock *block,
                            std::map<Instruction *, int> &insConstantValue,
                            std::queue<std::pair<Instruction *, Instruction *>> &SSAWorkList,
                            std::queue<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> &FlowWorkList)
        {
            int condition = 0;

            for (auto &ins : *block)
            {
                if (ins.isBinaryOp())
                {
                    // Process binary operations
                    Value *opr1 = ins.getOperand(0);
                    Value *opr2 = ins.getOperand(1);
                    auto *binaryInst = cast<BinaryOperator>(&ins);
                    int opr1Val, opr2Val;

                    opr1Val = (auto *operand1 = dyn_cast<ConstantInt>(opr1)) ? operand1->getZExtValue() : insConstantValue[dyn_cast<llvm::Instruction>(opr1)];
                    opr2Val = (auto *operand2 = dyn_cast<ConstantInt>(opr2)) ? operand2->getZExtValue() : insConstantValue[dyn_cast<llvm::Instruction>(opr2)];

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
                    case Instruction::SDiv:
                        computedVal = (opr2Val != 0) ? opr1Val / opr2Val : 0;
                        break;
                    default:
                        computedVal = 0;
                    }

                    if (opr1Val == INT_MIN || opr2Val == INT_MIN)
                    {
                        computedVal = INT_MIN;
                    }

                    if (insConstantValue[&ins] != computedVal)
                    {
                        insConstantValue[&ins] = computedVal;
                        for (auto *user : ins.users())
                        {
                            auto *userInstr = llvm::dyn_cast<llvm::Instruction>(user);
                            SSAWorkList.push({&ins, userInstr});
                        }
                    }
                }
                else if (auto *cmpInst = dyn_cast<ICmpInst>(&ins))
                {
                    // Handle comparison instructions
                    Value *opr1 = cmpInst->getOperand(0);
                    Value *opr2 = cmpInst->getOperand(1);

                    int opr1Val = (auto *operand1 = dyn_cast<ConstantInt>(opr1)) ? operand1->getZExtValue() : insConstantValue[dyn_cast<llvm::Instruction>(opr1)];
                    int opr2Val = (auto *operand2 = dyn_cast<ConstantInt>(opr2)) ? operand2->getZExtValue() : insConstantValue[dyn_cast<llvm::Instruction>(opr2)];

                    ICmpInst::Predicate predicate = cmpInst->getPredicate();
                    if (opr1Val != INT_MIN && opr2Val != INT_MIN)
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
                            break;
                        }
                    }
                    else
                    {
                        condition = 2; // Undefined condition
                    }
                }
                else if (auto *branchInst = dyn_cast<BranchInst>(&ins))
                {
                    // Handle branch instructions
                    if (branchInst->isConditional())
                    {
                        if (condition == 1)
                        {
                            FlowWorkList.push({block, branchInst->getSuccessor(0)});
                        }
                        else if (condition == 0)
                        {
                            FlowWorkList.push({block, branchInst->getSuccessor(1)});
                        }
                        else
                        {
                            FlowWorkList.push({block, branchInst->getSuccessor(0)});
                            FlowWorkList.push({block, branchInst->getSuccessor(1)});
                        }
                    }
                    else
                    {
                        FlowWorkList.push({block, branchInst->getSuccessor(0)});
                    }
                }
            }
            return 0;
        }

        // Processes PHI nodes
        void visitPhi(Instruction &ins, BasicBlock *destNode,
                      std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, bool> &ExecutableFlag,
                      std::map<llvm::BasicBlock *, std::set<llvm::BasicBlock *>> &AdjacencyList,
                      std::map<BasicBlock *, bool> Visited,
                      std::map<Instruction *, int> &insConstantVal,
                      std::queue<std::pair<Instruction *, Instruction *>> &SSAWorkList)
        {
            auto *Phi = llvm::cast<llvm::PHINode>(&ins);
            llvm::Value *Operand1 = Phi->getIncomingValue(0);
            llvm::Value *Operand2 = Phi->getIncomingValue(1);

            int Operand1Val = getPhiOperandVal(Operand1, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, &ins);
            int Operand2Val = getPhiOperandVal(Operand2, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, &ins);
            int ComputedVal = computeMeetValue(Operand1Val, Operand2Val);

            if (insConstantVal[&ins] != ComputedVal)
            {
                insConstantVal[&ins] = ComputedVal;
                for (auto *user : ins.users())
                {
                    auto *userInstr = llvm::dyn_cast<llvm::Instruction>(user);
                    SSAWorkList.push({&ins, userInstr});
                }
            }
        }

        // Main pass logic
        bool runOnFunction(Function &F) override
        {
            std::queue<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> FlowWorkList;
            std::queue<std::pair<Instruction *, Instruction *>> SSAWorkList;
            std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, bool> ExecutableFlag;
            std::map<llvm::BasicBlock *, int> nodeVisits;
            std::map<llvm::BasicBlock *, std::set<llvm::BasicBlock *>> AdjacencyList;
            std::map<llvm::BasicBlock *, bool> Visited;
            std::map<Instruction *, int> insConstantVal;

            // Initialize data structures
            for (auto &BB : F)
            {
                for (auto &ins : BB)
                {
                    insConstantVal[&ins] = INT_MAX;
                }

                for (auto *succ : successors(&BB))
                {
                    AdjacencyList[&BB].insert(succ);
                    ExecutableFlag[{&BB, succ}] = false;
                }

                nodeVisits[&BB] = 0;
                Visited[&BB] = false;
            }

            FlowWorkList.push({nullptr, &F.getEntryBlock()});

            // Process the worklists
            while (!FlowWorkList.empty() || !SSAWorkList.empty())
            {
                while (!FlowWorkList.empty())
                {
                    auto edge = FlowWorkList.front();
                    FlowWorkList.pop();

                    if (!ExecutableFlag[edge])
                    {
                        ExecutableFlag[edge] = true;
                        BasicBlock *destNode = edge.second;
                        nodeVisits[destNode]++;

                        for (auto &ins : *destNode)
                        {
                            if (llvm::isa<llvm::PHINode>(&ins))
                            {
                                visitPhi(ins, destNode, ExecutableFlag, AdjacencyList, Visited, insConstantVal, SSAWorkList);
                            }
                        }

                        if (nodeVisits[destNode] == 1)
                        {
                            visitExpression(destNode, insConstantVal, SSAWorkList, FlowWorkList);
                        }
                    }
                }

                while (!SSAWorkList.empty())
                {
                    auto edge = SSAWorkList.front();
                    SSAWorkList.pop();

                    if (llvm::isa<llvm::PHINode>(edge.second))
                    {
                        visitPhi(*edge.second, edge.second->getParent(), ExecutableFlag, AdjacencyList, Visited, insConstantVal, SSAWorkList);
                    }
                    else
                    {
                        visitExpression(edge.second->getParent(), insConstantVal, SSAWorkList, FlowWorkList);
                    }
                }
            }

            // Replace constants and remove redundant instructions
            for (auto &entry : insConstantVal)
            {
                Instruction *inst = entry.first;
                int constVal = entry.second;

                if (constVal != INT_MIN && constVal != INT_MAX)
                {
                    Constant *constant = ConstantInt::get(inst->getType(), constVal);
                    inst->replaceAllUsesWith(constant);
                    inst->eraseFromParent();
                }
            }

            errs() << F;
            return true;
        }
    };

} // end of anonymous namespace

char SSAConstantPropagation::ID = 0;
static RegisterPass<SSAConstantPropagation> X("SSAConstantPropagation", "SSAConstantPropagation Pass",
                                              false /* Only looks at CFG */,
                                              true /* Transform Pass */);
