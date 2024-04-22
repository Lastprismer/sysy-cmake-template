#pragma once

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "koopa.h"
#include "riscv_util.h"

using namespace std;

namespace riscv {

// 语法：ret
// 行为：将寄存器a0中的值作为返回值并返回
void ret(ostream& os);

// 语法：li {dest}, {imm}
// 行为：将imm加载到dest寄存器
void li(ostream& os, const string& dest, int imm);
void li(ostream& os, const Reg& dest, int imm);

// 语法：mv {rd}, {rs}
// 行为：rd = rs
void mv(ostream& os, const string& rd, const string& rs);
void mv(ostream& os, const Reg& rd, const Reg& rs);

// 语法：add {rd}, {rs1}, {rs2}
// 行为：rd = rs1 + rs2
void add(ostream& os, const string& rd, const string& rs1, const string& rs2);
void add(ostream& os, const Reg& rd, const Reg& rs1, const Reg& rs2);

// 语法：addi {rd}, {rs1}, {imm}
// 行为：rd = rs1 + imm
void addi(ostream& os, const string& rd, const string& rs1, int imm);
void addi(ostream& os, const Reg& rd, const Reg& rs1, int imm);

// 语法：sub {rd}, {rs1}, {rs2}
// 行为：rd = rs1 - rs2
void sub(ostream& os, const string& rd, const string& rs1, const string& rs2);
void sub(ostream& os, const Reg& rd, const Reg& rs1, const Reg& rs2);

// 语法：mul {rd}, {rs1}, {rs2}
// 行为：rd = rs1 * rs2
void mul(ostream& os, const string& rd, const string& rs1, const string& rs2);
void mul(ostream& os, const Reg& rd, const Reg& rs1, const Reg& rs2);

// 语法：div {rd}, {rs1}, {rs2}
// 行为：rd = rs1 / rs2
void div(ostream& os, const string& rd, const string& rs1, const string& rs2);
void div(ostream& os, const Reg& rd, const Reg& rs1, const Reg& rs2);

// 语法：rem {rd}, {rs1}, {rs2}
// 行为：rd = rs1 % rs2
void rem(ostream& os, const string& rd, const string& rs1, const string& rs2);
void rem(ostream& os, const Reg& rd, const Reg& rs1, const Reg& rs2);

// 语法：seqz {rd}, {rs}
// 行为：rs == 0 ? rd = 1 : rd = 0
void seqz(ostream& os, const string& rd, const string& rs);
void seqz(ostream& os, const Reg& rd, const Reg& rs);

/* --- 辅助函数 ---*/

string regstr(Reg reg);

}  // namespace riscv