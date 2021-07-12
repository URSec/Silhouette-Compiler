//===- ARMSilhouetteShadowStack - Modify Prologue/Epilogue for Shadow Stack ==//
//
//         Protecting Control Flow of Real-time OS applications
//              Copyright (c) 2019-2020, University of Rochester
//
// Part of the Silhouette Project, under the Apache License v2.0 with
// LLVM Exceptions.
// See LICENSE.txt in the top-level directory for license information.
//
//===----------------------------------------------------------------------===//
//
// This pass instruments the function prologue/epilogue to save/load the return
// address from a parallel shadow stack.
//
//===----------------------------------------------------------------------===//
//

#include "ARMSilhouetteShadowStack.h"
#include "MCTargetDesc/ARMAddressingModes.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/Support/CommandLine.h"

#include <vector>

using namespace llvm;

extern bool SilhouetteInvert;

char ARMSilhouetteShadowStack::ID = 0;

static cl::opt<int>
ShadowStackOffset("arm-silhouette-shadowstack-offset",
                  cl::desc("Silhouette shadow stack offset"),
                  cl::init(14680064), cl::Hidden);

ARMSilhouetteShadowStack::ARMSilhouetteShadowStack()
    : MachineFunctionPass(ID) {
  return;
}

StringRef
ARMSilhouetteShadowStack::getPassName() const {
  return "ARM Silhouette Shadow Stack Pass";
}

//
// Method: setupShadowStack()
//
// Description:
//   This method inserts instructions that store the return address onto the
//   shadow stack.
//
// Input:
//   MI - A reference to a PUSH instruction before which to insert instructions.
//
// Return value:
//   true - The PUSH was modified.
//
bool
ARMSilhouetteShadowStack::setupShadowStack(MachineInstr & MI) {
  MachineFunction & MF = *MI.getMF();
  const TargetInstrInfo * TII = MF.getSubtarget().getInstrInfo();

  int offset = ShadowStackOffset;

  unsigned PredReg;
  ARMCC::CondCodes Pred = getInstrPredicate(MI, PredReg);

  std::vector<MachineInstr *> NewMIs;
  const DebugLoc & DL = MI.getDebugLoc();

  if (offset >= 0 && offset <= 4092 && !SilhouetteInvert) {
    // Single-instruction shortcut
    NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2STRi12))
                     .addReg(ARM::LR)
                     .addReg(ARM::SP)
                     .addImm(offset)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));
  } else {
    // Try to find a free register; if we couldn't find one, spill and use R4
    std::vector<Register> FreeRegs = findFreeRegistersBefore(MI);
    bool Spill = FreeRegs.empty();
    Register ScratchReg = Spill ? ARM::R4 : FreeRegs[0];
    if (Spill) {
      errs() << "[SS] Unable to find a free register in " << MF.getName()
             << " for " << MI;
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tPUSH))
                       .add(predOps(Pred, PredReg))
                       .addReg(ScratchReg));
    }

    // First encode the shadow stack offset into the scratch register
    if (ARM_AM::getT2SOImmVal(offset) != -1) {
      // Use one MOV if the offset can be expressed in Thumb modified constant
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2MOVi), ScratchReg)
                       .addImm(offset)
                       .add(predOps(Pred, PredReg))
                       .add(condCodeOp()) // No 'S' bit
                       .setMIFlag(MachineInstr::ShadowStack));
    } else {
      // Otherwise use MOV/MOVT to load lower/upper 16 bits of the offset
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2MOVi16), ScratchReg)
                       .addImm(offset & 0xffff)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
      if ((offset >> 16) != 0) {
        NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2MOVTi16), ScratchReg)
                         .addReg(ScratchReg)
                         .addImm(offset >> 16)
                         .add(predOps(Pred, PredReg))
                         .setMIFlag(MachineInstr::ShadowStack));
      }
    }

    // Store the return address onto the shadow stack
    if (SilhouetteInvert) {
      // Add SP with the offset to the scratch register
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tADDrSP), ScratchReg)
                       .addReg(ARM::SP)
                       .addReg(ScratchReg)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
      // Generate an STRT to the shadow stack
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2STRT))
                       .addReg(ARM::LR)
                       .addReg(ScratchReg)
                       .addImm(0)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
    } else {
      // Generate an STR to the shadow stack
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2STRs))
                       .addReg(ARM::LR)
                       .addReg(ARM::SP)
                       .addReg(ScratchReg)
                       .addImm(0)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
    }

    // Restore the scratch register if we spilled it
    if (Spill) {
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tPOP))
                       .add(predOps(Pred, PredReg))
                       .addReg(ScratchReg));
    }
  }

  // Now insert these new instructions into the basic block
  insertInstsBefore(MI, NewMIs);

  return true;
}

//
// Method: popFromShadowStack()
//
// Description:
//   This method modifies a POP instruction to not write to PC/LR and inserts
//   new instructions that load the return address from the shadow stack into
//   PC/LR.
//
// Input:
//   MI   - A reference to a POP instruction after which to insert instructions.
//   PCLR - A reference to the PC or LR operand of the POP.
//
// Return value:
//   true - The POP was modified.
//
bool
ARMSilhouetteShadowStack::popFromShadowStack(MachineInstr & MI,
                                             MachineOperand & PCLR) {
  MachineFunction & MF = *MI.getMF();
  const TargetInstrInfo * TII = MF.getSubtarget().getInstrInfo();

  int offset = ShadowStackOffset;

  unsigned PredReg;
  ARMCC::CondCodes Pred = getInstrPredicate(MI, PredReg);

  std::vector<MachineInstr *> NewMIs;
  const DebugLoc & DL = MI.getDebugLoc();

  // Adjust SP to skip PC/LR on the stack
  NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tADDspi), ARM::SP)
                   .addReg(ARM::SP)
                   .addImm(1)
                   .add(predOps(Pred, PredReg))
                   .setMIFlag(MachineInstr::ShadowStack));

  if (offset >= 0 && offset <= 4092) {
    // Single-instruction shortcut
    NewMIs.push_back(BuildMI(MF, DL, TII->get(PCLR.getReg() == ARM::PC ?
                                              ARM::t2LDRi12_RET : ARM::t2LDRi12),
                             PCLR.getReg())
                     .addReg(ARM::SP)
                     .addImm(offset)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));
  } else {
    // Try to find a free register; if we couldn't find one, spill and use R4
    std::vector<Register> FreeRegs = findFreeRegistersAfter(MI);
    bool Spill = FreeRegs.empty();
    Register ScratchReg = Spill ? ARM::R4 : FreeRegs[0];
    if (Spill) {
      errs() << "[SS] Unable to find a free register in " << MF.getName()
             << " for " << MI;
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tPUSH))
                       .add(predOps(Pred, PredReg))
                       .addReg(ScratchReg));
    }

    // If we spilled the scratch register, change to use LR to hold the return
    // address as we have to restore the scratch register before writing to PC;
    // a BX_RET then needs to be inserted at the end
    bool NeedReturn = Spill && PCLR.getReg() == ARM::PC;
    if (NeedReturn) {
      PCLR.setReg(ARM::LR);

      // Mark LR as restored
      MachineFrameInfo & MFI = MF.getFrameInfo();
      if (MFI.isCalleeSavedInfoValid()) {
        for (CalleeSavedInfo & CSI : MFI.getCalleeSavedInfo()) {
          if (CSI.getReg() == ARM::LR) {
            CSI.setRestored(true);
            break;
          }
        }
      }
    }

    // First encode the shadow stack offset into the scratch register
    if (ARM_AM::getT2SOImmVal(offset) != -1) {
      // Use one MOV if the offset can be expressed in Thumb modified constant
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2MOVi), ScratchReg)
                       .addImm(offset)
                       .add(predOps(Pred, PredReg))
                       .add(condCodeOp()) // No 'S' bit
                       .setMIFlag(MachineInstr::ShadowStack));
    } else {
      // Otherwise use MOV/MOVT to load lower/upper 16 bits of the offset
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2MOVi16), ScratchReg)
                       .addImm(offset & 0xffff)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
      if ((offset >> 16) != 0) {
        NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2MOVTi16), ScratchReg)
                         .addReg(ScratchReg)
                         .addImm(offset >> 16)
                         .add(predOps(Pred, PredReg))
                         .setMIFlag(MachineInstr::ShadowStack));
      }
    }

    // Generate an LDR from the shadow stack to PC/LR
    NewMIs.push_back(BuildMI(MF, DL, TII->get(PCLR.getReg() == ARM::PC ?
                                              ARM::t2LDRs_RET : ARM::t2LDRs),
                             PCLR.getReg())
                     .addReg(ARM::SP)
                     .addReg(ScratchReg)
                     .addImm(0)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));

    // Restore the scratch register if we spilled it
    if (Spill) {
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tPOP))
                       .add(predOps(Pred, PredReg))
                       .addReg(ScratchReg));
    }

    // Insert a return if needed
    if (NeedReturn) {
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tBX_RET))
                       .add(predOps(Pred, PredReg)));
    }
  }

  // Now insert these new instructions into the basic block
  insertInstsAfter(MI, NewMIs);

  // At last, replace the old POP with a new one that doesn't write to PC/LR
  switch (MI.getOpcode()) {
  case ARM::t2LDMIA_RET:
    MI.setDesc(TII->get(ARM::t2LDMIA_UPD));
    // Copy implicit operands from the old POP to the new return
    NewMIs.back()->copyImplicitOps(MF, MI);
    for (unsigned i = MI.getNumOperands() - 1, e = MI.getNumExplicitOperands();
         i >= e; --i) {
      MI.RemoveOperand(i);
    }
    LLVM_FALLTHROUGH;
  case ARM::t2LDMIA_UPD:
    // LDMIA_UPD should load at least two registers; if it happens to be two,
    // we replace it with a LDR_POST
    assert(MI.getNumExplicitOperands() >= 6 && "Buggy LDMIA_UPD!");
    if (MI.getNumExplicitOperands() > 6) {
      MI.RemoveOperand(MI.getOperandNo(&PCLR));
    } else {
      unsigned Idx = MI.getOperandNo(&PCLR);
      Idx = Idx == 4 ? 5 : 4; // Index of the other register than PC/LR
      insertInstAfter(MI, BuildMI(MF, DL, TII->get(ARM::t2LDR_POST),
                                  MI.getOperand(Idx).getReg())
                          .addReg(ARM::SP, RegState::Define)
                          .addReg(ARM::SP)
                          .addImm(4)
                          .add(predOps(Pred, PredReg))
                          .setMIFlags(MI.getFlags()));
      removeInst(MI);
    }
    break;

  case ARM::tPOP_RET:
    MI.setDesc(TII->get(ARM::tPOP));
    // Copy implicit operands from the old POP to the new return
    NewMIs.back()->copyImplicitOps(MF, MI);
    for (unsigned i = MI.getNumOperands() - 1, e = MI.getNumExplicitOperands();
         i >= e; --i) {
      MI.RemoveOperand(i);
    }
    LLVM_FALLTHROUGH;
  case ARM::tPOP:
    // POP should load at least one register; if it happens to be one, we just
    // remove it
    assert(MI.getNumExplicitOperands() >= 3 && "Buggy POP!");
    if (MI.getNumExplicitOperands() > 3) {
      MI.RemoveOperand(MI.getOperandNo(&PCLR));
    } else {
      removeInst(MI);
    }
    break;

  // ARM::t2LDR_POST
  default:
    // LDR_POST only loads one register, so we just remove it
    removeInst(MI);
    break;
  }

  return true;
}

//
// Method: runOnMachineFunction()
//
// Description:
//   This method is called when the PassManager wants this pass to transform
//   the specified MachineFunction.  This method instruments the
//   prologue/epilogue of a MachineFunction so that the return address is saved
//   into/loaded from the shadow stack.
//
// Inputs:
//   MF - A reference to the MachineFunction to transform.
//
// Outputs:
//   MF - The transformed MachineFunction.
//
// Return value:
//   true  - The MachineFunction was transformed.
//   false - The MachineFunction was not transformed.
//
bool
ARMSilhouetteShadowStack::runOnMachineFunction(MachineFunction & MF) {
#if 1
  // Skip privileged functions
  if (MF.getFunction().getSection().equals("privileged_functions")) {
    errs() << "[SS] Privileged function! skipped: " << MF.getName() << "\n";
    return false;
  }
#endif

  // Warn if the function has variable-sized objects; we assume the program is
  // transformed by store-to-heap promotion, either via a compiler pass or
  // manually
  if (MF.getFrameInfo().hasVarSizedObjects()) {
    errs() << "[SS] Variable-sized objects not promoted in "
           << MF.getName() << "\n";
  }

  // Find out all pushes that write LR to the stack and all pops that read a
  // return address from the stack to LR or PC
  std::vector<std::pair<MachineInstr *, MachineOperand *> > Pushes;
  std::vector<std::pair<MachineInstr *, MachineOperand *> > Pops;
  for (MachineBasicBlock & MBB : MF) {
    for (MachineInstr & MI : MBB) {
      switch (MI.getOpcode()) {
      // Frame setup instructions in function prologue
      case ARM::t2STR_PRE:
      case ARM::t2STMDB_UPD:
        // STR_PRE and STMDB_UPD are considered as PUSH if they write to SP!
        if (MI.getOperand(0).getReg() != ARM::SP) {
          break;
        }
        LLVM_FALLTHROUGH;
      case ARM::tPUSH:
        // LR can appear as a GPR not in prologue, in which case we don't care
        if (MI.getFlag(MachineInstr::FrameSetup)) {
          for (MachineOperand & MO : MI.explicit_operands()) {
            if (MO.isReg() && MO.getReg() == ARM::LR) {
              Pushes.push_back(std::make_pair(&MI, &MO));
              break;
            }
          }
        }
        break;

      // Frame destroy instructions in function epilogue
      case ARM::t2LDR_POST:
      case ARM::t2LDMIA_UPD:
      case ARM::t2LDMIA_RET:
        // LDR_POST and LDMIA_(UPD|RET) are considered as POP if they write to
        // SP!
        if (MI.getOperand(1).getReg() != ARM::SP) {
          break;
        }
        LLVM_FALLTHROUGH;
      case ARM::tPOP:
      case ARM::tPOP_RET:
        if (MI.getFlag(MachineInstr::FrameDestroy)) {
          // Handle 2 cases:
          // (1) POP writing to LR
          // (2) POP writing to PC
          for (MachineOperand & MO : MI.explicit_operands()) {
            if (MO.isReg() && (MO.getReg() == ARM::LR || MO.getReg() == ARM::PC)) {
              Pops.push_back(std::make_pair(&MI, &MO));
              break;
            }
          }
        }
        break;

      default:
        break;
      }
    }
  }

  // Instrument each push and pop
  bool changed = false;
  for (auto & MIMO : Pushes) {
    changed |= setupShadowStack(*MIMO.first);
  }
  for (auto & MIMO : Pops) {
    changed |= popFromShadowStack(*MIMO.first, *MIMO.second);
  }

  return changed;
}

//
// Create a new pass.
//
namespace llvm {
  FunctionPass * createARMSilhouetteShadowStack(void) {
    return new ARMSilhouetteShadowStack();
  }
}
