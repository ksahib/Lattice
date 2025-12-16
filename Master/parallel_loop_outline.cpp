#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "llvm/Frontend/OpenMP/OMPIRBuilder.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/BasicBlock.h"
#include <cassert>
#include "parallel_loop_outline.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "fstream"

static void collectInnermostLoops(llvm::Loop *L, llvm::SmallVectorImpl<llvm::Loop *> &Out)
{
    if (L->getSubLoops().empty())
    {
        Out.push_back(L);
        return;
    }
    for (llvm::Loop *Sub : L->getSubLoops())
        collectInnermostLoops(Sub, Out);
}

llvm::Function *outlineLoop(llvm::Function &F, llvm::LoopInfo &LI, llvm::DominatorTree &DT, llvm::ScalarEvolution &SE, llvm::Value *&StartV, llvm::Value *&EndV, llvm::Value *&StepV)
{
    // llvm::errs() << "Starting loop outlining pass on function: " << F.getName() << "\n";
    llvm::SmallVector<llvm::Loop *, 8> Innermost;

    for (llvm::Loop *Top : LI)
        collectInnermostLoops(Top, Innermost);
    if (Innermost.empty())
    {
        // llvm::outs() << "innermost empty";
        return nullptr;
    }

    llvm::CodeExtractorAnalysisCache CEAC(F);
    for (llvm::Loop *L : Innermost)
    {
        bool doall_check = false;

        // llvm::errs() << "Found loop\n";

        if (llvm::Instruction *Term = L->getHeader()->getTerminator())
        {
            if (llvm::MDNode *LoopMD = Term->getMetadata("my.loop.parallel"))
            {
                // llvm::errs() << "Found my.loop.parallel metadata node (operands = "
                //  << LoopMD->getNumOperands() << ")\n";

                for (unsigned i = 0; i < LoopMD->getNumOperands(); ++i)
                {
                    llvm::Metadata *Op = LoopMD->getOperand(i);
                    if (!Op)
                        continue;

                    // Case 1: MDString -> e.g. "parallel.type=DOALL"
                    if (auto *MDS = llvm::dyn_cast<llvm::MDString>(Op))
                    {
                        llvm::StringRef S = MDS->getString();
                        // llvm::errs() << "  MDString: " << S << "\n";
                        if (S.starts_with("parallel.type="))
                        {
                            llvm::StringRef Val = S.substr(strlen("parallel.type="));
                            if (Val.equals_insensitive("DOALL"))
                            {
                                llvm::errs() << "  -> Detected DOALL parallel loop\n";
                                doall_check = true;
                            }
                            else
                            {
                                llvm::errs() << "  -> Unknown parallel.type value: " << Val << "\n";

                                continue;
                            }
                        }
                    }
                    else if (auto *Inner = llvm::dyn_cast<llvm::MDNode>(Op))
                    {
                        // llvm::errs() << "  Nested MDNode with " << Inner->getNumOperands()
                        //              << " operands — recursing one level\n";
                        for (unsigned j = 0; j < Inner->getNumOperands(); ++j)
                        {
                            if (auto *S = llvm::dyn_cast_or_null<llvm::MDString>(Inner->getOperand(j)))
                            {
                                // llvm::errs() << "    nested MDString: " << S->getString() << "\n";
                            }
                            else if (auto *CAM = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(Inner->getOperand(j)))
                            {
                                if (auto *CI = llvm::dyn_cast<llvm::ConstantInt>(CAM->getValue()))
                                {
                                    // llvm::errs() << "    nested ConstantInt: " << CI->getZExtValue() << "\n";
                                }
                            }
                        }
                    }
                    else if (auto *CAM = llvm::dyn_cast<llvm::ConstantAsMetadata>(Op))
                    {
                        if (auto *CI = llvm::dyn_cast<llvm::ConstantInt>(CAM->getValue()))
                        {
                            bool Parallel = CI->getZExtValue() != 0;
                            // llvm::errs() << "  ConstantInt metadata -> parallel = " << Parallel << "\n";
                        }
                        else
                        {
                            // llvm::errs() << "  ConstantAsMetadata with non-int constant\n";
                        }
                    }
                    else
                    {
                        // llvm::errs() << "  Unknown metadata operand kind\n";
                    }
                }
            }
            else
            {
                // llvm::errs() << "No my.loop.parallel metadata on terminator\n";
            }
        }
        else
        {
            // llvm::errs() << "Header has no terminator?!\n";
        }

        if (!doall_check)
        {
            continue;
        }
        else
        {
            llvm::errs() << "Found loop metadata...\n";
        }

        // --- NEW: build LoopBody such that PHI indvar stays outside extraction ---

        llvm::SmallVector<llvm::BasicBlock *, 8> LoopBody;
        //// SCEV
        llvm::BasicBlock *Preheader = L->getLoopPreheader();
        llvm::PHINode *indvar = nullptr;

        for (auto &I : *L->getHeader())
        {
            if (auto *phi = llvm::dyn_cast<llvm::PHINode>(&I))
            {
                // check if one incoming is from preheader and other from latch
                if (L->contains(phi->getIncomingBlock(1)) &&
                    phi->getIncomingBlock(0) == L->getLoopPreheader())
                {
                    indvar = phi;
                    break;
                }
            }
        }

        if (!indvar)
        {
            llvm::outs() << "No canonical induction variable\n";
            llvm::errs() << "No canonical induction variable\n";
            return nullptr;
        }
        const llvm::SCEV *BackedgeCount = SE.getBackedgeTakenCount(L);
        const llvm::SCEV *Start = nullptr, *Step = nullptr, *End = nullptr;

        if (const llvm::SCEVAddRecExpr *AR = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SE.getSCEV(indvar)))
        {
            Start = AR->getStart();
            Step = AR->getStepRecurrence(SE);

            // BackedgeCount is the number of times the backedge is taken.
            // TripCount (number of loop iterations) is BackedgeCount + 1.
            // Our runtime expects an end-exclusive bound, so:
            //   End = Start + Step * TripCount.
            const llvm::SCEV *One = SE.getOne(BackedgeCount->getType());
            const llvm::SCEV *TripCount = SE.getAddExpr(BackedgeCount, One);
            End = SE.getAddExpr(Start, SE.getMulExpr(Step, TripCount));
        }
        llvm::SCEVExpander Exp(SE, F.getParent()->getDataLayout(), "scevexp");
        StartV = Exp.expandCodeFor(Start, indvar->getType(), Preheader->getTerminator());
        StepV = Exp.expandCodeFor(Step, indvar->getType(), Preheader->getTerminator());
        EndV = Exp.expandCodeFor(End, indvar->getType(), Preheader->getTerminator());

        ////SCEV

        // get the loop header
        llvm::BasicBlock *Header = L->getHeader();
        if (!Header)
        {
            // defensive fallback
            for (llvm::BasicBlock *BB : L->blocks())
                LoopBody.push_back(BB);
        }
        else
        {
            // Find first non-PHI in header
            llvm::Instruction *FirstNonPhi = Header->getFirstNonPHI();
            if (!FirstNonPhi)
            {
                // header only has PHIs (rare) — just use full region
                for (llvm::BasicBlock *BB : L->blocks())
                    LoopBody.push_back(BB);
            }
            else
            {
                // split header after PHIs, so PHIs (like induction variable) are *outside*
                std::string BodyName = "23";
                llvm::BasicBlock *HeaderBody = Header->splitBasicBlock(FirstNonPhi, BodyName);
                Header->setName("");
                Header->setName("ind_split");

                // Replace Header with HeaderBody in loop block list
                for (llvm::BasicBlock *BB : L->blocks())
                {
                    if (BB == Header)
                        LoopBody.push_back(HeaderBody);
                    else
                        LoopBody.push_back(BB);
                }
            }
        }

        if (LoopBody.empty())
        {

            llvm::errs() << "Loop body empty after processing — skipping loop.\n";
            continue;
        }

        // Recompute DT because splitBasicBlock changed the CFG
        llvm::DominatorTree LocalDT;
        LocalDT.recalculate(F);

        // Now continue as before:
        llvm::CodeExtractor CE(LoopBody, &LocalDT);
        if (!CE.isEligible())
        {
            llvm::errs() << "CodeExtractor region ineligible — skipping loop.\n";
            llvm::outs() << "outs region ineligible — skipping loop.\n";
            continue;
        }
        else
        {
            llvm::outs() << "CodeExtractor region eligible\n";
            llvm::errs() << "outs region ineligible — skipping loop.\n";
        }

        llvm::BasicBlock *ExitBlock = L->getExitBlock();

        llvm::SetVector<llvm::Value *> Inputs, Outputs;
        llvm::SetVector<llvm::Value *> SinkCands; // usually empty for loops

        CE.findInputsOutputs(Inputs, Outputs, SinkCands, /*CollectGlobalInputs = */ true);

        // --- store-scan: find memory destinations the loop writes to and add their underlying object
        for (llvm::BasicBlock *BB : LoopBody)
        {
            for (llvm::Instruction &I : *BB)
            {
                if (auto *SI = llvm::dyn_cast<llvm::StoreInst>(&I))
                {
                    llvm::Value *Ptr = SI->getPointerOperand();
                    llvm::Value *Base = llvm::getUnderlyingObject(Ptr, /*MaxLookup=*/64);
                    if (!Base)
                        continue;

                    // if the base is an instruction, ensure it is defined outside the loop
                    if (auto *BI = llvm::dyn_cast<llvm::Instruction>(Base))
                    {
                        if (L->contains(BI->getParent()))
                            continue; // base defined inside loop -> skip
                    }

                    // consider Alloca/Global/Argument or any value defined outside loop
                    if (!Inputs.count(Base) && !Outputs.count(Base))
                    {
                        Outputs.insert(Base);
                        llvm::errs() << "store-scan: added base to Outputs: " << *Base << "\n";
                    }
                }
            }
        }

        // continue;
        llvm::outs() << "Captured Inputs:\n";
        for (llvm::Value *V : Inputs)
        {
            llvm::outs() << "  - " << V << "\n";
        }

        llvm::outs() << "Captured Inputs:\n";
        for (llvm::Value *V : Inputs)
        {
            llvm::outs() << "  - " << *V << "\n";
        }
        llvm::outs() << "Captured Outputs:\n";

        for (llvm::Value *V : Outputs)
        {
            llvm::outs() << "  - " << *V << "\n";
        }

        if (llvm::Function *Outlined = CE.extractCodeRegion(CEAC))
        {
            llvm::errs() << "Successfully outlined loop to function: " << Outlined->getName() << "\n";
            // make the outlined function externally visible and give it a friendly name
            Outlined->setLinkage(llvm::GlobalValue::ExternalLinkage);
            Outlined->setName("outlined_main_loopbody");

            llvm::SmallVector<llvm::Value *, 16> Candidates;
            // Prefer outputs first (so output allocas / results are matched to Outlined's output params)
            for (llvm::Value *V : Outputs)
                Candidates.push_back(V);
            // then inputs as fallback
            for (llvm::Value *V : Inputs)
                Candidates.push_back(V);

            llvm::Value *IndVar = indvar; // your earlier detected canonical PHI (may be nullptr)

            // Map each Outlined arg to a candidate original value (or nullptr)
            llvm::SmallVector<llvm::Value *, 8> ArgOriginVals;
            ArgOriginVals.reserve(Outlined->arg_size());

            llvm::SmallPtrSet<llvm::Value *, 8> UsedCandidates;

            // !!!!!!!!!!locate outlined function Call site and map args!!!!!!!!!!!!!!!!
            for (llvm::BasicBlock &BB : F)
            {
                llvm::StringRef BBName = BB.getName();
                if (BBName == "codeRepl")
                {
                    for (llvm::Instruction &I : BB)
                    {
                        if (auto *CI = llvm::dyn_cast<llvm::CallInst>(&I))
                        {
                            if (CI->getCalledFunction() == Outlined)
                            {
                                // llvm::errs() << "Found outlined function call: " << *CI << "\n";
                                for (unsigned ai = 0; ai < CI->getNumOperands() - 1; ++ai)
                                {
                                    llvm::Value *ArgOp = CI->getArgOperand(ai);
                                    ArgOriginVals.push_back(ArgOp);
                                }
                            }
                        }
                    }
                }
            }

            // Debug
            for (llvm::Value *U : Candidates)
            {
                llvm::errs() << " candidate: " << *U << "\n";
            }
            // Debug
            llvm::errs() << "ArgOriginVals mapping:\n";
            for (unsigned i = 0; i < ArgOriginVals.size(); ++i)
            {
                llvm::errs() << "  param[" << i << "] -> ";
                if (ArgOriginVals[i])
                    ArgOriginVals[i]->print(llvm::errs());
                else
                    llvm::errs() << "NULL";
                llvm::errs() << "\n";
            }

            // Debug
            llvm::errs() << "Captured Outputs:\n";
            for (llvm::Value *V : Outputs)
            {
                if (V)
                    V->print(llvm::errs());
                llvm::errs() << "\n";
            }
            int InductionParamIndex = -1;
            llvm::SmallVector<llvm::Type *, 8> NewEnvFieldTys;
            llvm::SmallVector<llvm::Value *, 8> NewEnvOriginVals;

            for (unsigned pi = 0; pi < ArgOriginVals.size(); ++pi)
            {
                llvm::Value *orig = ArgOriginVals[pi];
                llvm::Type *ParamTy = Outlined->getFunctionType()->getParamType(pi);
                if (orig == IndVar)
                {
                    InductionParamIndex = (int)pi;
                    llvm::errs() << "Detected induction variable parameter at index "
                                 << InductionParamIndex << "\n";
                    continue; // index param will be provided by runtime (idx)
                }
                NewEnvFieldTys.push_back(ParamTy);
                NewEnvOriginVals.push_back(orig); // may be nullptr => will store null
            }

            if (F.getName() == "main")
            {

                Preheader = &F.getEntryBlock();
                // llvm::BasicBlock *TargetBB = nullptr;
                // for (llvm::BasicBlock &BB : F)
                // {
                //     if (BB.getName() == "2")
                //     {
                //         TargetBB = &BB;
                //         break;
                //     }
                // }

                // if (!TargetBB)
                // {
                //     llvm::errs() << "Could not find block named '2' in function "
                //                  << F.getName() << "\n";
                // }
                // else
                // {
                //     llvm::errs() << "Found block: " << TargetBB->getName() << "\n";
                // }
                // ExitBlock = TargetBB;
                llvm::BasicBlock *TargetBB = nullptr;
                for (llvm::BasicBlock &BB : F)
                {
                    llvm::StringRef BBName = BB.getName();
                    if (BBName == "ind_split")
                    {
                        TargetBB = &BB;
                        llvm::errs() << "Found block: " << TargetBB->getName() << "\n";
                        break;
                    }
                }
                TargetBB->dropAllReferences();
                TargetBB->eraseFromParent();
                TargetBB = nullptr;
                for (llvm::BasicBlock &BB : F)
                {
                    llvm::StringRef BBName = BB.getName();
                    if (BBName == "codeRepl")
                    {
                        TargetBB = &BB;
                        llvm::errs() << "Found block: " << TargetBB->getName() << "\n";
                        break;
                    }
                }
                TargetBB->dropAllReferences();
                TargetBB->eraseFromParent();
                for (llvm::BasicBlock &BB : F)
                {
                    llvm::StringRef BBName = BB.getName();
                    llvm::errs() << "Examining block: " << BBName << "\n";
                    if (llvm::isa<llvm::ReturnInst>(BB.getTerminator()))
                    {
                        ExitBlock = &BB;
                        break;
                    }
                }
                if (!ExitBlock)
                {
                    llvm::errs() << "Could not find exit block for loop in function "
                                 << F.getName() << "\n";
                }
                else
                {
                    llvm::errs() << "Found exit block: " << ExitBlock->getName() << "\n";
                }
                // llvm::Instruction *OldTerm = TargetBB->getTerminator();
                // llvm::BranchInst *NewBr = llvm::BranchInst::Create(ExitBlock);
                // // If OldTerm is conditional and you need to preserve one destination, handle accordingly.
                // llvm::ReplaceInstWithInst(OldTerm, NewBr);
            }
            if (!Preheader)
            {
                llvm::errs() << "LoopOutliner: failed to find preheader after extraction.\n";
                continue; // or return nullptr if inside a function
            }

            else
            {
                llvm::errs() << "Found preheader: " << Preheader->getName() << "\n";
            }

            // if (!ExitBlock)
            // {
            //     llvm::errs() << "LoopOutliner: failed to find exit block after extraction.\n";
            //     continue;
            // }
            // else
            // {
            //     llvm::errs() << "Found exit block: " << ExitBlock->getName() << "\n";
            // }
            llvm::Instruction *Term = Preheader->getTerminator();

            llvm::IRBuilder<> B(Term);
            llvm::Module *M = F.getParent();
            llvm::errs() << "Preparing to build environment struct for outlined loop\n";
            // Types
            llvm::Type *VoidTy = llvm::Type::getVoidTy(F.getContext());
            llvm::Type *Int64Ty = llvm::Type::getInt64Ty(F.getContext());
            llvm::Type *Int8PtrTy = llvm::Type::getInt8Ty(F.getContext())->getPointerTo();
            llvm::Type *LoopBodyFnTy = llvm::FunctionType::get(VoidTy, {Int64Ty, Int8PtrTy}, false)->getPointerTo();

            llvm::StructType *NewEnvStructTy =
                llvm::StructType::create(F.getContext(), NewEnvFieldTys, "env.struct");
            llvm::errs() << "Created environment struct typeddd\n";
            // Allocate env on heap via malloc
            uint64_t NewEnvSize = M->getDataLayout().getTypeAllocSize(NewEnvStructTy);
            llvm::Value *SizeConst = llvm::ConstantInt::get(Int64Ty, NewEnvSize);
            llvm::errs() << "NewEnvSize: " << NewEnvSize << "\n";
            {
                const char *OutPath = "/tmp/env_struct_size.txt";
                std::ofstream ofs(OutPath, std::ios::app);
                if (!ofs.is_open()) {
                    llvm::errs() << "Failed to open " << OutPath << " for writing\n";
                } else {
                    ofs  << NewEnvSize << "\n";
                }
            }
            // Emit env metadata JSON for runtime serialization
            {
                const char *MetaPath = "/home/niloy/vs_code/course/cse299/Lattice/Worker/env_metadata.json";
                std::ofstream meta(MetaPath, std::ios::trunc);
                if (!meta.is_open()) {
                    llvm::errs() << "Failed to open " << MetaPath << " for writing\n";
                } else {
                    const llvm::DataLayout &DL = M->getDataLayout();
                    const llvm::StructLayout *SL = DL.getStructLayout(NewEnvStructTy);
                    meta << "{\n";
                    meta << "  \"struct_size\": " << NewEnvSize << ",\n";
                    meta << "  \"fields\": [\n";
                    for (unsigned k = 0; k < NewEnvFieldTys.size(); ++k) {
                        llvm::Type *FTy = NewEnvFieldTys[k];
                        uint64_t offset = SL->getElementOffset(k);
                        std::string kind = "SCALAR";
                        uint64_t elemSize = 0;
                        int lenField = -1;
                        int64_t fixedLen = -1;

                        if (FTy->isPointerTy()) {
                            llvm::Type *ElemTy = nullptr;
                            llvm::Value *orig = NewEnvOriginVals[k];

                            // Try to recover the original element type / array length
                            if (auto *AI = llvm::dyn_cast<llvm::AllocaInst>(orig)) {
                                ElemTy = AI->getAllocatedType();
                            } else if (auto *GV = llvm::dyn_cast<llvm::GlobalVariable>(orig)) {
                                ElemTy = GV->getValueType();
                            }

                            if (auto *ArrTy = llvm::dyn_cast_or_null<llvm::ArrayType>(ElemTy)) {
                                // Fixed-size array like [N x T]
                                kind = "FIXED_ARRAY";
                                fixedLen = static_cast<int64_t>(ArrTy->getNumElements());
                                elemSize = DL.getTypeAllocSize(ArrTy->getElementType());
                            } else if (ElemTy &&
                                       (ElemTy->isIntegerTy() || ElemTy->isFloatingPointTy())) {
                                // Pointer to a single scalar value (e.g., i64*)
                                kind = "SCALAR_PTR";
                                elemSize = DL.getTypeAllocSize(ElemTy);
                            } else {
                                // Generic pointer array, possibly with a separate length field
                                kind = "POINTER_ARRAY";
                                if (ElemTy)
                                    elemSize = DL.getTypeAllocSize(ElemTy);
                                else
                                    elemSize = 0;  // opaque pointer

                                if (k + 1 < NewEnvFieldTys.size() &&
                                    NewEnvFieldTys[k + 1]->isIntegerTy()) {
                                    lenField = static_cast<int>(k + 1);
                                }
                            }
                        }

                        meta << "    {\"index\": " << k
                             << ", \"offset\": " << offset
                             << ", \"kind\": \"" << kind << "\""
                             << ", \"elem_size\": " << elemSize
                             << ", \"len_field\": " << lenField
                             << ", \"fixed_length\": " << fixedLen
                             << "}";
                        if (k + 1 < NewEnvFieldTys.size()) meta << ",";
                        meta << "\n";
                    }
                    meta << "  ]\n}\n";
                }
            }
            llvm::FunctionCallee MallocFn = M->getOrInsertFunction("malloc",
                                                                   llvm::FunctionType::get(Int8PtrTy, {Int64Ty}, false));
            llvm::Value *RawPtr = B.CreateCall(MallocFn, {SizeConst}, "env_raw"); // i8*
            llvm::Value *NewEnvPtr = B.CreateBitCast(RawPtr, NewEnvStructTy->getPointerTo(), "envptr_struct");

            // Populate env fields (same order)
            for (unsigned k = 0; k < NewEnvOriginVals.size(); ++k)
            {
                llvm::Value *orig = NewEnvOriginVals[k];
                llvm::Value *GEP = B.CreateStructGEP(NewEnvStructTy, NewEnvPtr, k, "env_gep");
                llvm::Value *storeVal = nullptr;
                llvm::Type *FTy = NewEnvFieldTys[k];

                if (!orig)
                {
                    storeVal = llvm::Constant::getNullValue(FTy);
                }
                else
                {
                    if (orig->getType() != FTy)
                    {
                        if (orig->getType()->isPointerTy() && FTy->isPointerTy())
                            storeVal = B.CreateBitCast(orig, FTy);
                        else if (orig->getType()->isIntegerTy() && FTy->isIntegerTy())
                            storeVal = B.CreateIntCast(orig, FTy, /*isSigned=*/true);
                        else
                            storeVal = llvm::Constant::getNullValue(FTy);
                    }
                    else
                    {
                        storeVal = orig;
                    }
                }

                B.CreateStore(storeVal, GEP);
            }
            llvm::errs() << "Populated env struct and allocated on heap\n";
            llvm::Value *EnvPtrCast = RawPtr;

            // -------- define wrapper(i64, i8*) that unpacks env and calls Outlined --------

            llvm::FunctionType *WrapperFT = llvm::FunctionType::get(VoidTy, {Int64Ty, Int8PtrTy}, false);
            llvm::Function *WrapperFn = M->getFunction("wrapper");
            if (!WrapperFn)
            {
                WrapperFn = llvm::Function::Create(WrapperFT, llvm::Function::ExternalLinkage, "wrapper", M);
                llvm::errs() << "Created wrapper function\n";
            }
            else
            {
                llvm::errs() << "Wrapper function already exists\n";
            }

            if (WrapperFn->empty())
            {
                auto argIt = WrapperFn->arg_begin();
                llvm::Argument *IdxArg = argIt++;
                IdxArg->setName("idx");
                llvm::Argument *EnvArg = argIt++;
                EnvArg->setName("env");

                llvm::BasicBlock *WEntry = llvm::BasicBlock::Create(F.getContext(), "entry", WrapperFn);
                llvm::IRBuilder<> Wb(WEntry);

                // cast env -> NewEnvStructTy*
                llvm::Value *EnvStructPtr = Wb.CreateBitCast(EnvArg, NewEnvStructTy->getPointerTo(), "envstruct");

                // load fields in same order
                llvm::SmallVector<llvm::Value *, 8> LoadedFields;
                LoadedFields.reserve(NewEnvFieldTys.size());
                for (unsigned k = 0; k < NewEnvFieldTys.size(); ++k)
                {
                    llvm::Value *Fgep = Wb.CreateStructGEP(NewEnvStructTy, EnvStructPtr, k, "fgep");
                    llvm::Value *Fload = Wb.CreateLoad(NewEnvFieldTys[k], Fgep, "fload");
                    LoadedFields.push_back(Fload);
                }

                // assemble call args for Outlined in exact param order
                llvm::SmallVector<llvm::Value *, 8> CallArgsForOutlined;
                unsigned envCursor = 0;
                for (unsigned pi = 0; pi < ArgOriginVals.size(); ++pi)
                {
                    if ((int)pi == InductionParamIndex)
                    {
                        // use runtime idx (cast to param type)
                        llvm::Type *PTy = Outlined->getFunctionType()->getParamType(pi);
                        llvm::Value *IdxVal = Wb.CreateIntCast(IdxArg, PTy, /*isSigned=*/true, "idxcast");
                        CallArgsForOutlined.push_back(IdxVal);
                    }
                    else
                    {
                        llvm::Value *fv = LoadedFields[envCursor++];
                        llvm::Type *PTy = Outlined->getFunctionType()->getParamType(pi);
                        if (fv->getType() != PTy)
                        {
                            if (fv->getType()->isPointerTy() && PTy->isPointerTy())
                                fv = Wb.CreateBitCast(fv, PTy);
                            else if (fv->getType()->isIntegerTy() && PTy->isIntegerTy())
                                fv = Wb.CreateIntCast(fv, PTy, /*isSigned=*/true);
                            else
                                fv = llvm::Constant::getNullValue(PTy);
                        }
                        CallArgsForOutlined.push_back(fv);
                    }
                }

                Wb.CreateCall(Outlined, CallArgsForOutlined);
                Wb.CreateRetVoid();
            }

            // Bitcast wrapper to runtime expected function pointer type
            llvm::Value *CastedWrapper = B.CreateBitCast(WrapperFn, LoopBodyFnTy);

            //  Declare parallel_for_runtime
            llvm::FunctionCallee ParallelForFunc = M->getOrInsertFunction(
                "parallel_for_runtime",
                llvm::FunctionType::get(
                    VoidTy,
                    {Int64Ty, Int64Ty, Int64Ty, LoopBodyFnTy, Int8PtrTy}, false));

            llvm::Type *ArgTy = nullptr;
            if (Outlined->getFunctionType()->getNumParams() > 0)
                ArgTy = Outlined->getFunctionType()->getParamType(0);

            // Insert the parallel call
            llvm::Value *StartArg = StartV;
            llvm::Value *EndArg = EndV;
            llvm::Value *StepArg = StepV;

            // Improvement: Handle without sign extension or make dynamic to allow for larger types
            // If any are not i64, sign-extend them to i64 (safe for small positive indices)
            if (StartArg->getType() != Int64Ty && StartArg->getType()->isIntegerTy())
                StartArg = B.CreateSExt(StartArg, Int64Ty, "start64");
            if (EndArg->getType() != Int64Ty && EndArg->getType()->isIntegerTy())
                EndArg = B.CreateSExt(EndArg, Int64Ty, "end64");
            if (StepArg->getType() != Int64Ty && StepArg->getType()->isIntegerTy())
                StepArg = B.CreateSExt(StepArg, Int64Ty, "step64");

            if (!Preheader || !ExitBlock || !Term)
            {
                // llvm::errs() << "PDG_OUTLINE: missing Preheader/ExitBlock/Term; skipping outlining for this loop\n";
                // Leave IR untouched; return nullptr to indicate we didn't outline.
                return nullptr;
            }

            // Insert the parallel call (at Preheader's terminator)
            llvm::SmallVector<llvm::Value *, 5> CallArgs;
            CallArgs.push_back(StartArg);
            CallArgs.push_back(EndArg);
            CallArgs.push_back(StepArg);
            CallArgs.push_back(CastedWrapper);
            CallArgs.push_back(EnvPtrCast);

            // Create the call and a branch to the original exit block.
            llvm::CallInst *CI = llvm::cast<llvm::CallInst>(B.CreateCall(ParallelForFunc, CallArgs));
            (void)CI; // silence unused-var in release builds

            // Create new branch to the loop exit.
            B.CreateBr(ExitBlock);

            Term->eraseFromParent();

            return Outlined;
        }

        else
        {
            llvm::errs() << "Code extraction failed!\n";
        }
    }
    return nullptr;
}

using namespace llvm;

struct LoopOutlinerPass : public PassInfoMixin<LoopOutlinerPass>
{
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)
    {
        // Skip functions that we (or CodeExtractor) already generated.
        // This prevents re-processing newly created outlined functions.
        if (F.getName().contains(".loopcond") || F.hasFnAttribute("outlined-loop"))
            return PreservedAnalyses::all();

        LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
        DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
        ScalarEvolution &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);

        llvm::Value *Start = nullptr;
        llvm::Value *Step = nullptr;
        llvm::Value *End = nullptr;

        Function *outlined = outlineLoop(F, LI, DT, SE, Start, End, Step);

        if (outlined)
        {
            // marks it so we never process it again
            outlined->addFnAttr("outlined-loop");
            return PreservedAnalyses::none();
        }

        return PreservedAnalyses::all();
    }
};

void registerLoopOutlinerPass(llvm::FunctionPassManager &FPM)
{
    FPM.addPass(LoopOutlinerPass());
}

// Register the pass as a plugin
extern "C" PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo()
{
    llvm::outs() << "Registering Loop Outliner Pass...\n";
    return {
        LLVM_PLUGIN_API_VERSION, "LoopOutliner", "v0.1",
        [](PassBuilder &PB)
        {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>)
                {
                    if (Name == "loop-outliner")
                    {
                        FPM.addPass(LoopOutlinerPass());
                        return true;
                    }
                    return false;
                });
        }};
}

// ensure canonical exported entrypoint
extern "C" __attribute__((visibility("default"))) ::llvm::PassPluginLibraryInfo LLVMGetPassPluginInfo()
{
    return llvmGetPassPluginInfo(); // calls your existing function
}
