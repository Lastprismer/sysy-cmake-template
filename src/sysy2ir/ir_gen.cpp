#include "ir_gen.h"

namespace ir {

Node::Node() {}

Node::Node(int i) : tag(NodeTag::IMM), imm(i) {}

Node::Node(const Node& n) : tag(n.tag) {
  if (tag == NodeTag::IMM) {
    imm = n.imm;
  } else if (tag == NodeTag::SYMBOL) {
    symbol_name = n.symbol_name;
  }
}

Node::Node(Node&& n) : tag(n.tag) {
  if (tag == NodeTag::IMM) {
    imm = n.imm;
  } else if (tag == NodeTag::SYMBOL) {
    symbol_name = n.symbol_name;
  }
}

IRGenerator::IRGenerator() : _enable(true) {
  setting.setOs(cout).setIndent(0);

  variable_pool = 0;
  function_name = "";
  return_type = "";
  node_stack = deque<Node>();
  symbol_table = SymbolTable();
}

IRGenerator& IRGenerator::getInstance() {
  static IRGenerator gen;
  return gen;
}

void IRGenerator::writeFuncPrologue() {
  if (_enable) {
    setting.getOs() << "fun @" << function_name << "(): " << return_type
                    << "{\n";
    return;
  }
}

void IRGenerator::writeFuncEpilogue() {
  if (_enable) {
    ostream& os = setting.getOs();
    os << setting.getIndentStr() << "ret ";
    Node node = node_stack.front();
    if (node.tag == NodeTag::IMM) {
      os << node.imm;
    } else if (node.tag == NodeTag::SYMBOL) {
      os << node.symbol_name;
    } else {
      assert(false);
    }
    os << "\n"
       << "}" << endl;
    _enable = false;
    return;
  }
}

void IRGenerator::writeBlockPrologue() {
  if (_enable) {
    ostream& os = setting.getOs();
    os << setting.getIndentStr()
       << "%"
          "entry:"
       << endl;
    setting.getIndent() += 2;
    return;
  }
}

void IRGenerator::pushSymbol(int syb) {
  Node comp;
  comp.tag = NodeTag::SYMBOL;
  if (syb == -1) {
    comp.symbol_name = getSymbolName(registerNewSymbol());
  }
  comp.symbol_name = getSymbolName(syb);
  node_stack.push_front(comp);
}

void IRGenerator::pushImm(int int_const) {
  Node comp;
  comp.tag = NodeTag::IMM;
  comp.imm = int_const;
  node_stack.push_front(comp);
}

const Node& IRGenerator::checkFrontNode() const {
  return node_stack.front();
}

Node IRGenerator::getFrontNode() {
  Node node = node_stack.front();
  node_stack.pop_front();
  return node;
}

void IRGenerator::writeUnaryInst(OpID op) {
  if (_enable) {
    if (op == OpID::UNARY_POS) {
      return;
    }
    assert(node_stack.size() >= 1);
    // 只有两种运算
    // 1. a = -b，等效于 a = 0 - b，推出b，加入0，推入b，调用sub
    // 2. a = !b，等效于a = 0 == b，推出b，加入0，推入b，调用eq
    if (op == OpID::UNARY_NEG) {
      Node node = node_stack.front();
      node_stack.pop_front();
      pushImm(0);
      node_stack.push_front(node);
      writeBinaryInst(OpID::BI_SUB);
      return;
    } else if (op == OpID::UNARY_NOT) {
      Node node = node_stack.front();
      node_stack.pop_front();
      pushImm(0);
      node_stack.push_front(node);
      writeBinaryInst(OpID::LG_EQ);
      return;
    } else {
      assert(false);
    }
  }
}

void IRGenerator::writeBinaryInst(OpID op) {
  if (_enable) {
    assert(node_stack.size() >= 2);
    ostream& os = setting.getOs();
    Node right = node_stack.front();
    node_stack.pop_front();
    Node left = node_stack.front();
    node_stack.pop_front();

    // const expr
    if (right.tag == NodeTag::IMM && left.tag == NodeTag::IMM) {
      pushImm(calcConstExpr(left, right, op));
      return;
    }

    int new_symbol = registerNewSymbol();
    os << setting.getIndentStr() << getSymbolName(new_symbol) << " = ";
    os << BiOp2koopa(op) << ' ';
    parseNode(left);
    os << ", ";
    parseNode(right);
    os << endl;
    pushSymbol(new_symbol);
  }
}

void IRGenerator::writeLogicInst(OpID op) {
  if (_enable) {
    assert(node_stack.size() >= 2);
    assert(op == OpID::LG_AND || op == OpID::LG_OR);

    Node right = node_stack.front();
    node_stack.pop_front();

    // left -> bool
    pushImm(0);
    writeBinaryInst(OpID::LG_NEQ);

    // right -> bool
    node_stack.push_front(right);
    pushImm(0);
    writeBinaryInst(OpID::LG_NEQ);

    // logic
    writeBinaryInst(op);
  }
}

void IRGenerator::writeAllocInst(const SymbolTableEntry& entry) {
  if (_enable) {
    ostream& os = setting.getOs();
    os << setting.getIndentStr() << entry.GetAllocInst() << endl;
  }
}

void IRGenerator::writeLoadInst(const SymbolTableEntry& entry) {
  if (_enable) {
    ostream& os = setting.getOs();
    // 申请新符号
    int new_symbol = registerNewSymbol();
    os << setting.getIndentStr() << entry.GetLoadInst(getSymbolName(new_symbol))
       << endl;
    // 推入节点
    pushSymbol(new_symbol);
  }
}

void IRGenerator::writeStoreInst(const SymbolTableEntry& entry) {
  if (_enable) {
    ostream& os = setting.getOs();
    // 弹出栈顶节点
    const Node& node = getFrontNode();

    os << setting.getIndentStr();
    if (node.tag == NodeTag::IMM) {
      // 常量赋值
      os << entry.GetStoreInst(node.imm) << endl;
    } else if (node.tag == NodeTag::SYMBOL) {
      os << entry.GetStoreInst(node.symbol_name) << endl;
    }
  }
}

int IRGenerator::registerNewSymbol() {
  return variable_pool++;
}

string IRGenerator::getSymbolName(const int& symbol) const {
  return string("%") + to_string(symbol);
}

void IRGenerator::parseNode(const Node& node) {
  ostream& os = setting.getOs();
  switch (node.tag) {
    case NodeTag::IMM:
      os << node.imm;
      break;
    case NodeTag::SYMBOL:
      os << node.symbol_name;
      break;
    default:
      cerr << (int)node.tag;
      assert(false);
      break;
  }
}

int IRGenerator::calcConstExpr(const Node& left, const Node& right, OpID op) {
  int l = left.imm;
  int r = right.imm;
  switch (op) {
    case BI_ADD:
      return l + r;
    case BI_SUB:
      return l - r;
    case BI_MUL:
      return l * r;
    case BI_DIV:
      return l / r;
    case BI_MOD:
      return l % r;
    case LG_GT:
      return l > r;
    case LG_GE:
      return l >= r;
    case LG_LT:
      return l < r;
    case LG_LE:
      return l <= r;
    case LG_EQ:
      return l == r;
    case LG_NEQ:
      return l != r;
    case LG_AND:
      return l && r;
    case LG_OR:
      return l || r;
    default:
      assert(false);
  }
  return 0;
}
}  // namespace ir