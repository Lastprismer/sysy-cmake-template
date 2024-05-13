#include "riscv_read.h"

namespace riscv {

void visit_program(const koopa_raw_program_t& program) {
  // 执行一些其他的必要操作
  // ...
  // 访问所有全局变量
  visit_slice(program.values);
  // 访问所有函数=
  visit_slice(program.funcs);
}

void visit_slice(const koopa_raw_slice_t& slice) {
  for (size_t i = 0; i < slice.len; ++i) {
    auto ptr = slice.buffer[i];
    // 根据 slice 的 kind 决定将 ptr 视作何种元素
    switch (slice.kind) {
      case KOOPA_RSIK_FUNCTION:
        // 访问函数
        visit_func(reinterpret_cast<koopa_raw_function_t>(ptr));
        break;
      case KOOPA_RSIK_BASIC_BLOCK:
        // 访问基本块
        visit_basic_block(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
        break;
      case KOOPA_RSIK_VALUE:
        // 访问指令
        visit_value(reinterpret_cast<koopa_raw_value_t>(ptr));
        break;
      default:
        // 我们暂时不会遇到其他内容, 于是不对其做任何处理
        assert(false);
        break;
    }
  }
}

void visit_func(const koopa_raw_function_t& func) {
  // 执行一些其他的必要操作
  // ...
  // 访问所有基本块
  RiscvGenerator& gen = RiscvGenerator::getInstance();
  gen.FunctionName = ParseSymbol(func->name);
  gen.StackMemManager.Activate();
  CalcMemoryNeeded(func);
  gen.WritePrologue();
  visit_slice(func->bbs);
  gen.StackMemManager.Deactivate();
}

void visit_basic_block(const koopa_raw_basic_block_t& bb) {
  // 执行一些其他的必要操作
  // ...
  // 访问所有指令
  // os << "basic block: name: " << bb->name << endl;
  visit_slice(bb->insts);
}

void visit_value(const koopa_raw_value_t& value) {
  //  根据指令类型判断后续需要如何访问
  const auto& kind = value->kind;
  cout << GetTypeString(value);
  // cout << kind.tag;
  switch (kind.tag) {
    case KOOPA_RVT_INTEGER:
      cout << "  int" << endl;
      // visit_inst_int(kind.data.integer);
      break;
    case KOOPA_RVT_ALLOC:
      cout << "  alloc" << endl;
      // visit_inst_alloc(value);
      break;
    case KOOPA_RVT_LOAD:
      cout << "  load" << endl;
      visit_inst_load(value);
      break;
    case KOOPA_RVT_STORE:
      cout << "  store" << endl;
      visit_inst_store(value);
      break;
    case KOOPA_RVT_BINARY:
      cout << "  binary" << endl;
      visit_inst_binary(value);
      break;
    case KOOPA_RVT_RETURN:
      cout << "  ret" << endl;
      visit_inst_ret(kind.data.ret);
      break;
    default:
      cout << "  what?" << endl;
      break;
      // 其他类型暂时遇不到
  }
}

void visit_inst_int(const koopa_raw_integer_t& inst_int) {
  cerr << "NOT EXPECTED" << endl;
}

void visit_inst_alloc(const koopa_raw_value_t& value) {
  cerr << "NOT EXPECTED" << endl;
}

void visit_inst_load(const koopa_raw_value_t& inst_load) {
  RiscvGenerator& gen = RiscvGenerator::getInstance();
  StackMemoryModule& smem = gen.StackMemManager;
  if (smem.InstResult.find(inst_load->kind.data.load.src) !=
      smem.InstResult.end()) {
    smem.InstResult[inst_load] = smem.InstResult[inst_load->kind.data.load.src];
    return;
  }
  // assert(false);
}

void visit_inst_store(const koopa_raw_value_t& inst) {
  const koopa_raw_store_t& inst_store = inst->kind.data.store;
  RiscvGenerator& gen = RiscvGenerator::getInstance();
  StackMemoryModule& smem = gen.StackMemManager;
  StackMemoryModule::InstResultInfo dst, src;
  if (smem.InstResult.find(inst_store.dest) != smem.InstResult.end()) {
    dst = smem.InstResult[inst_store.dest];
  } else {
    // 存进栈里
    dst.ty = StackMemoryModule::ValueType::stack;
    dst.content.addr = smem.IncreaseStackUsed();
    smem.InstResult[inst_store.dest] = dst;
  }

  if (inst_store.value->kind.tag == KOOPA_RVT_INTEGER) {
    src.ty = StackMemoryModule::ValueType::imm;
    src.content.imm = inst_store.value->kind.data.integer.value;
  } else {
    // src不应该是stack
    auto srcResult = smem.InstResult[inst_store.value];
    src.ty = srcResult.ty;
    if (src.ty == StackMemoryModule::ValueType::reg)
      src.content.reg = srcResult.content.reg;
    else
      src.content.addr = srcResult.content.addr;
  }

  smem.WriteStoreInst(StackMemoryModule::StoreInfo(dst, src));
}

void visit_inst_binary(const koopa_raw_value_t& inst) {
  const koopa_raw_binary_t& inst_bina = inst->kind.data.binary;
  RiscvGenerator& gen = RiscvGenerator::getInstance();
  auto& smem = gen.StackMemManager;
  Reg r1 = GetValueResult(inst_bina.lhs);
  Reg r2 = GetValueResult(inst_bina.rhs);
  RiscvGenerator::getInstance().WriteBinaInst(inst_bina.op, r1, r2);
  // 把r1的内容存回内存，返回地址?
  StackMemoryModule::InstResultInfo dst, src;
  dst.ty = StackMemoryModule::ValueType::stack;
  dst.content.addr = smem.IncreaseStackUsed();
  src.ty = StackMemoryModule::ValueType::reg;
  src.content.reg = r1;
  smem.WriteStoreInst(StackMemoryModule::StoreInfo(dst, src));
  if (smem.InstResult.find(inst_bina.rhs) != smem.InstResult.end() ||
      inst_bina.rhs->kind.tag == KOOPA_RVT_INTEGER) {  // 保存过或是立即数
    // r2是临时分配的
    gen.RegManager.releaseReg(r2);
  }
  smem.InstResult[inst] = dst;
}

void visit_inst_ret(const koopa_raw_return_t& inst_ret) {
  RiscvGenerator& gen = RiscvGenerator::getInstance();
  auto& instResult = gen.StackMemManager.InstResult;
  StackMemoryModule::InstResultInfo info;

  if (instResult.find(inst_ret.value) != instResult.end())
    info = instResult[inst_ret.value];
  else if (inst_ret.value->kind.tag == KOOPA_RVT_INTEGER) {
    info.ty = StackMemoryModule::ValueType::imm;
    info.content.imm = inst_ret.value->kind.data.integer.value;
  }
  gen.WriteEpilogue(info);
}

void CalcMemoryNeeded(const koopa_raw_function_t& func) {
  int allocSize = 0;
  // 遍历函数所有的bb中的所有指令
  const koopa_raw_slice_t& func_bbs = func->bbs;
  // assert(func_bbs.kind == KOOPA_RSIK_BASIC_BLOCK);
  for (size_t i = 0; i < func_bbs.len; ++i) {
    const koopa_raw_basic_block_t& bb =
        reinterpret_cast<koopa_raw_basic_block_t>(func_bbs.buffer[i]);
    // assert(bb->insts.kind == KOOPA_RSIK_VALUE);

    for (size_t j = 0; j < bb->insts.len; ++j) {
      const koopa_raw_value_t& value =
          reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
      if (value->ty->tag == KOOPA_RTT_INT32)
        allocSize += 4;
    }
  }

  // 向上进位到16
  allocSize = (allocSize + 15) / 16 * 16;
  RiscvGenerator::getInstance().StackMemManager.SetStackMem(allocSize);
  return;
}

Reg GetValueResult(const koopa_raw_value_t& value) {
  RiscvGenerator& gen = RiscvGenerator::getInstance();
  auto& smem = gen.StackMemManager;
  if (value->kind.tag == KOOPA_RVT_INTEGER) {
    // 处理常数
    Reg rs = gen.RegManager.getAvailableReg();
    smem.WriteLI(rs, value->kind.data.integer.value);
    return rs;
  }
  // assert(smem.InstResult.find(value) != smem.InstResult.end());

  StackMemoryModule::InstResultInfo info;
  info = smem.InstResult.at(value);
  if (info.ty == StackMemoryModule::ValueType::reg) {
    cout << "will this actually run?" << endl;
    // 还没做分配策略的
    return info.content.reg;
  } else if (info.ty == StackMemoryModule::ValueType::stack) {
    // 先读出
    Reg rs = gen.RegManager.getAvailableReg();
    smem.WriteLW(rs, Reg::sp, info.content.addr);
    return rs;
  }
  assert(false);
}

}  // namespace riscv