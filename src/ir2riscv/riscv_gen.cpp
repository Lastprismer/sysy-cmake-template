#include "riscv_gen.h"

namespace riscv {

#pragma region Register

RegisterModule::RegisterModule() {
  available_regs = {};
  for (Reg reg = Reg::t0; reg != Reg::ra; reg = (Reg)((int)reg + 1)) {
    available_regs.insert(reg);
  }
}

Reg RegisterModule::GetAvailableReg() {
  assert(available_regs.size() > 0);
  Reg reg = *available_regs.begin();
  available_regs.erase(reg);
  return reg;
}

void RegisterModule::ReleaseReg(Reg reg) {
  if (reg != Reg::NONE)
    available_regs.insert(reg);
}

bool RegisterModule::GetReg(const Reg& reg) {
  if (available_regs.find(reg) != available_regs.end()) {
    available_regs.erase(reg);
    return true;
  }
  return false;
}

#pragma endregion

#pragma region Stack

const string InstResultInfo::Output() const {
  stringstream ss;
  ss << "type: ";
  switch (ty) {
    case ValueType::e_imm:
      ss << "imm, value: " << content.imm;
      break;
    case ValueType::e_reg:
      ss << "reg, Reg: " << regstr(content.reg);
      break;
    case ValueType::e_stack:
      ss << "stack, addr: " << content.addr;
      break;
    default:
      ss << "not init";
      break;
  }
  return ss.str();
}

InstResultInfo::InstResultInfo() : ty(ValueType::e_unused) {}

InstResultInfo::InstResultInfo(const Reg& reg) : ty(ValueType::e_reg) {
  content.reg = reg;
}

InstResultInfo::InstResultInfo(const ValueType& type, int value) : ty(type) {
  if (type == ValueType::e_imm) {
    content.imm = value;
  } else if (type == ValueType::e_stack) {
    content.addr = value;
  } else {
    cerr << "InstResultInfo: invalid type" << endl;
    assert(false);
  }
}

void StackMemoryModule::SetStackMem(const int& mem) {
  stack_memory = mem;
  stack_used = 0;
}

void StackMemoryModule::WriteStoreInst(const InstResultInfo& src,
                                       const InstResultInfo& dest) {
  auto& gen = RiscvGenerator::getInstance();
  ostream& os = gen.setting.getOs();
  switch (src.ty) {
    case ValueType::e_imm:
      if (dest.ty == ValueType::e_reg) {
        // store imm to reg
        // 现在不会用到，之后优化寄存器策略时可能用？
        li(os, dest.content.reg, src.content.imm);
      } else if (dest.ty == ValueType::e_stack) {
        // store imm to stack
        Reg rd = gen.regCore.GetAvailableReg();
        li(os, rd, src.content.imm);
        WriteSW(rd, dest.content.addr);
        gen.regCore.ReleaseReg(rd);
      }
      break;
    case ValueType::e_reg:
      if (dest.ty == ValueType::e_reg) {
        // store reg to reg
        // 啥也不用做，最后会改表
      } else if (dest.ty == ValueType::e_stack) {
        // store reg to stack
        WriteSW(src.content.reg, dest.content.addr);
        gen.regCore.ReleaseReg(src.content.reg);
      }
      break;
    case ValueType::e_stack: {
      if (dest.ty == ValueType::e_stack) {
        Reg rs = gen.regCore.GetAvailableReg();
        WriteLW(rs, src.content.addr);
        WriteSW(rs, dest.content.addr);
        gen.regCore.ReleaseReg(rs);
      } else if (dest.ty == ValueType::e_reg) {
        WriteLW(dest.content.reg, src.content.addr);
      }
    }
    default:
      break;
  }
  return;
}

void StackMemoryModule::WriteLI(const Reg& rd, int imm) {
  ostream& os = RiscvGenerator::getInstance().setting.getOs();
  li(os, rd, imm);
}

void StackMemoryModule::WriteLW(const Reg& rd, int addr) {
  auto& gen = RiscvGenerator::getInstance();
  ostream& os = gen.setting.getOs();
  if (IsImmInBound(addr)) {
    lw(os, rd, Reg::sp, addr);
  } else {
    Reg adr = gen.regCore.GetAvailableReg();
    li(os, adr, addr);
    add(os, adr, adr, Reg::sp);
    lw(os, rd, adr, 0);
    gen.regCore.ReleaseReg(adr);
  }
}

void StackMemoryModule::WriteSW(const Reg& rs, int addr) {
  auto& gen = RiscvGenerator::getInstance();
  ostream& os = gen.setting.getOs();
  if (IsImmInBound(addr)) {
    sw(os, Reg::sp, rs, addr);
  } else {
    Reg adr = gen.regCore.GetAvailableReg();
    li(os, adr, addr);
    add(os, adr, adr, Reg::sp);
    sw(os, adr, rs, 0);
    gen.regCore.ReleaseReg(adr);
  }
}

void StackMemoryModule::Debug_OutputInstResult() {
  cout << endl;
  cout << "count: " << InstResult.size() << endl;
  for (auto i = InstResult.begin(); i != InstResult.end(); i++) {
    cout << GetTypeString(i->first) << "  ";
    i->second.Output();
  }
  cout << endl;
}

int StackMemoryModule::IncreaseStackUsed() {
  stack_used += 4;
  return stack_memory - stack_used;
}

int StackMemoryModule::DecreaseStackUsed() {
  stack_used -= 4;
  return stack_memory - stack_used;
}

void StackMemoryModule::Clear() {
  stack_memory = 0;
  stack_used = 0;
  InstResult.clear();
}

StackMemoryModule::StackMemoryModule() : stack_memory(0), stack_used(0) {
  InstResult = map<koopa_raw_value_t, InstResultInfo>();
}

#pragma endregion

#pragma region BB

BBModule::BBModule() {}

void BBModule::WriteBBName(const string& label) {
  auto& gen = RiscvGenerator::getInstance();
  ostream& os = gen.setting.getOs();
  wlabel(os, ParseSymbol(label) + "_" + gen.funcCore.func_name);
}

void BBModule::WriteJumpInst(const string& label) {
  auto& gen = RiscvGenerator::getInstance();
  ostream& os = gen.setting.getOs();
  j(os, ParseSymbol(label) + "_" + gen.funcCore.func_name);
}

void BBModule::WriteBranch(const Reg& cond,
                           const string& trueLabel,
                           const string& falseLabel) {
  auto& gen = RiscvGenerator::getInstance();
  ostream& os = gen.setting.getOs();
  const string trueMid =
      ParseSymbol(trueLabel) + "_mid_" + gen.funcCore.func_name;
  bnez(os, cond, trueMid);
  // 否则跳到false
  j(os, ParseSymbol(falseLabel) + "_" + gen.funcCore.func_name);
  wlabel(os, trueMid);
  j(os, ParseSymbol(trueLabel) + "_" + gen.funcCore.func_name);
}

#pragma endregion

#pragma region Func

FuncModule::FuncModule() : is_leaf_func(false), func_name() {}

void FuncModule::WritePrologue() {
  auto& gen = RiscvGenerator::getInstance();
  auto& os = gen.setting.getOs();
  os << "\n  .text" << endl;
  os << "  .globl " << func_name << endl;
  os << func_name << ":" << endl;

  // 分配栈内存
  int stackMemoryAlloc = gen.stackCore.stack_memory;
  if (stackMemoryAlloc == 0)
    return;
  if (IsImmInBound(-stackMemoryAlloc)) {
    addi(os, Reg::sp, Reg::sp, -stackMemoryAlloc);
  } else {
    Reg rd = gen.regCore.GetAvailableReg();
    li(os, rd, -stackMemoryAlloc);
    add(os, Reg::sp, Reg::sp, rd);
    gen.regCore.ReleaseReg(rd);
  }

  // 保存ra
  if (is_leaf_func)
    gen.stackCore.WriteSW(Reg::ra, gen.stackCore.IncreaseStackUsed());
}

void FuncModule::WriteEpilogue(const InstResultInfo& retValueInfo) {
  auto& gen = RiscvGenerator::getInstance();
  auto& os = gen.setting.getOs();

  if (retValueInfo.ty == ValueType::e_imm) {
    li(os, Reg::a0, retValueInfo.content.imm);
  } else if (retValueInfo.ty == ValueType::e_reg) {
    mv(os, Reg::a0, retValueInfo.content.reg);
  } else if (retValueInfo.ty == ValueType::e_stack) {
    gen.stackCore.WriteLW(a0, retValueInfo.content.addr);
  } else {
    // 返回void
  }

  int stack_memory_alloc = gen.stackCore.stack_memory;
  // 读取ra
  if (is_leaf_func)
    gen.stackCore.WriteLW(Reg::ra, stack_memory_alloc - 4);

  // 回收栈内存
  if (stack_memory_alloc != 0) {
    if (IsImmInBound(stack_memory_alloc)) {
      addi(os, Reg::sp, Reg::sp, stack_memory_alloc);
    } else {
      Reg rd = gen.regCore.GetAvailableReg();
      li(os, rd, stack_memory_alloc);
      add(os, Reg::sp, Reg::sp, rd);
      gen.regCore.ReleaseReg(rd);
    }
  }
  ret(os);
}

void FuncModule::WriteCallInst(const string& name) {
  ostream& os = RiscvGenerator::getInstance().setting.getOs();
  call(os, name);
}

void FuncModule::Clear() {
  is_leaf_func = false;
  func_name = string();
}

#pragma endregion

#pragma region globalvar

GlobalVarModule::GlobalVarModule() {}

void GlobalVarModule::WriteGlobalVarDecl(const string& name,
                                         const InitInfo& init) {
  auto& gen = RiscvGenerator::getInstance();
  auto& os = gen.setting.getOs();

  os << "\n  .data\n  .globl " << name << "\n" << name << ": \n";
  switch (init.ty) {
    case InitType::e_zeroinit:
      os << "  .zero 4" << endl;
      break;
    case InitType::e_int:
      os << "  .word " << init.value << endl;
  }
}

const Reg GlobalVarModule::WriteLoadGlobalVar(const string& name) {
  auto& gen = RiscvGenerator::getInstance();
  auto& os = gen.setting.getOs();
  Reg tmp = gen.regCore.GetAvailableReg();
  la(os, tmp, name);
  lw(os, tmp, tmp, 0);
  return tmp;
}

void GlobalVarModule::WriteStoreGlobalVar(const string& name,
                                          const InstResultInfo& info) {
  auto& gen = RiscvGenerator::getInstance();
  auto& os = gen.setting.getOs();
  Reg addr = gen.regCore.GetAvailableReg();
  la(os, addr, name);

  Reg src = gen.regCore.GetAvailableReg();
  if (info.ty == ValueType::e_imm) {
    li(os, src, info.content.imm);
  } else if (info.ty == ValueType::e_reg) {
    src = info.content.reg;
  } else if (info.ty == ValueType::e_stack) {
    gen.stackCore.WriteLW(src, info.content.addr);
  }
  sw(os, addr, src, 0);
  gen.regCore.ReleaseReg(src);
  return;
}

#pragma endregion

#pragma region RiscvGen

RiscvGenerator::RiscvGenerator()
    : regCore(), stackCore(), bbCore(), globalCore() {
  setting.setOs(cout).setIndent(0);
}

RiscvGenerator& RiscvGenerator::getInstance() {
  static RiscvGenerator riscgen;
  return riscgen;
}

void RiscvGenerator::WriteBinaInst(OpType op,
                                   const Reg& left,
                                   const Reg& right) {
  ostream& os = setting.getOs();

  switch (op) {
    case koopa_raw_binary_op::KOOPA_RBO_NOT_EQ:
      neq(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_EQ:
      eq(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_GT:
      sgt(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_LT:
      slt(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_GE:
      sge(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_LE:
      sle(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_ADD:
      add(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_SUB:
      sub(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_MUL:
      mul(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_DIV:
      div(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_MOD:
      rem(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_AND:
      andr(os, left, left, right);
      break;

    case koopa_raw_binary_op::KOOPA_RBO_OR:
      orr(os, left, left, right);
      break;

    default:
      cerr << op;
      break;
      assert(false);
  }
}

void RiscvGenerator::Clear() {
  stackCore.Clear();
  funcCore.Clear();
}

#pragma endregion

}  // namespace riscv
