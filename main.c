#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OPERAND 4
#define ORIGIN 0
#define MAX_TOKEN 8

// 文字列をすべて小文字にする
void ToLower(char *s) {
  while (s && *s) {
    *s = tolower(*s);
    s++;
  }
}

enum TokenKind {
  kTokenInt = 128,
  kTokenRelInt,
  kTokenLabel,
  kTokenRelLabel,
  kTokenByte,
  kTokenWord,
};

struct Token {
  //   0 - 15  : レジスタ名（kRegIR1 - kRegZR）
  //  32 - 127 : 一文字演算子
  // 128 - last: TokenKind
  int kind;
  char *raw;
  int len;
  int val;
};

void InitToken(struct Token *token, enum TokenKind kind, char *raw, int len, int val) {
  token->kind = kind;
  token->raw = raw;
  token->len = len;
  token->val = val;
}

const char* const reg_names[16] = {
  "ir1",  "ir2", "ir3", "flag",
  "iv",   "a",   "b",   "c",
  "d",    "e",   "mem", "bank",
  "addr", "ip",  "sp",  "zr"
};
enum RegNames {
  kRegIR1,  kRegIR2, kRegIR3, kRegFLAG,
  kRegIV,   kRegA,   kRegB,   kRegC,
  kRegD,    kRegE,   kRegMEM, kRegBANK,
  kRegADDR, kRegIP,  kRegSP,  kRegZR
};

int RegNameToIndex(const char *name, int n) {
  for (int i = 0; i < 16; i++) {
    int j;
    for (j = 0; j < n; j++) {
      if (reg_names[i][j] != tolower(name[j])) {
        break;
      }
    }
    if (j == n && reg_names[i][j] == '\0') {
      return i;
    }
  }
  return -1;
}

struct Operand {
  int len;
  struct Token tokens[MAX_TOKEN];
};

void TokenizeOperand(char *opr_str, struct Operand *dest) {
  char *p = opr_str;

  for (int i = 0; i < MAX_TOKEN; i++) {
    p += strspn(p, " \t");
    if (*p == '\0') {
      dest->len = i;
      return;
    }

    if (isdigit(*p)) {
      char *endptr;
      int v = strtol(p, &endptr, 0);
      InitToken(dest->tokens + i, kTokenInt, p, endptr - p, v);
      p = endptr;
    } else if (strchr("+-", *p) != NULL) {
      InitToken(dest->tokens + i, *p, p, 1, 0);
      p++;
    } else if (p[0] == '@') {
      char *endptr;
      if (isdigit(p[1])) {
        int v = strtol(p + 1, &endptr, 0);
        InitToken(dest->tokens + i, kTokenRelInt, p + 1, endptr - p - 1, v);
      } else if (isalpha(p[1])) {
        endptr = p + 2;
        while (isalnum(*endptr)) {
          endptr++;
        }
        InitToken(dest->tokens + i, kTokenRelLabel, p + 1, endptr - p - 1, 0);
      } else {
        fprintf(stderr, "unexpectec character for relative-int/label: '%c'\n", p[1]);
        exit(1);
      }
      p = endptr;
    } else if (isalpha(*p)) {
      char *endptr = p + 1;
      while (isalnum(*endptr)) {
        endptr++;
      }
      int len = endptr - p;
      if (len == 4 && strncmp(p, "byte", 4) == 0) {
        InitToken(dest->tokens + i, kTokenByte, p, 4, 0);
      } else if (len == 4 && strncmp(p, "word", 4) == 0) {
        InitToken(dest->tokens + i, kTokenWord, p, 4, 0);
      } else {
        int reg_idx = RegNameToIndex(p, len);
        if (reg_idx < 0) {
          InitToken(dest->tokens + i, kTokenLabel, p, len, 0);
        } else {
          InitToken(dest->tokens + i, reg_idx, p, len, 0);
        }
      }
      p = endptr;
    } else {
      fprintf(stderr, "unexpected character: '%c'\n", *p);
      exit(1);
    }
  }

  dest->len = MAX_TOKEN;
}

// line をニーモニックとオペランドに分割する
// 戻り値: オペランドの数
int SplitOpcode(char *line, char **label, char **mnemonic, struct Operand *operands, int n) {
  char *comment = strchr(line, '#');
  if (comment) {
    *comment = '\0';
    comment++;
  }

  char *colon = strchr(line, ':');
  if (colon) {
    *label = line;
    *colon = '\0';
    line = colon + 1;
  } else {
    *label = NULL;
  }

  if ((*mnemonic = strtok(line, " \t\n")) == NULL) {
    return -1;
  }
  for (int i = 0; i < n; ++i) {
    char *opr = strtok(NULL, ",\n");
    if (opr == NULL) {
      return i;
    }
    TokenizeOperand(opr, operands + i);
  }
  return n;
}

enum RegImmKind {
  kReg = 0, kImm8 = 1, kImm16 = 2
};

struct RegImm {
  enum RegImmKind kind;
  int16_t val;
  const char *label; // 即値がラベルの場合
};

uint8_t FlagNameToBits(const char* flag_name) {
  char flag = flag_name[0];
  uint8_t mask = 0;

  if (flag_name[0] == 'n') {
    if (strcmp(flag_name + 1, "op") == 0) { // NOP
      return 0;
    }
    flag = flag_name[1];
    mask = 1;
  }

  switch (flag) {
  case 'c': return 2u | mask;
  case 'v': return 4u | mask;
  case 'z': return 6u | mask;
  case 's': return 8u | mask;
  default:
    fprintf(stderr, "unknown flag: '%c'\n", flag);
    exit(1);
  }
}

struct Instruction {
  int ip, len;
  uint8_t op, out;
  uint8_t in, imm8;
  uint16_t imm16;
};

enum DataWidth {
  kByte,
  kWord,
};

struct LabelAddr {
  const char *label;
  int ip;
};

// Back patch type
enum BPType {
  BP_ABS,
  BP_ABS8,
  BP_ABS16,
  BP_IP_REL,
  BP_IP_REL8,
  BP_IP_REL16,
};

struct Backpatch {
  int insn_idx;
  const char *label;
  enum BPType type;
};

void InitBackpatch(struct Backpatch *bp, int insn_idx,
                   const char *label, enum BPType type) {
  bp->insn_idx = insn_idx;
  bp->label = label;
  bp->type = type;
}

struct Instruction insn[1024];
int insn_idx = 0;
int ip = ORIGIN;

struct Backpatch backpatches[128];
int num_backpatches = 0;

struct LabelAddr labels[128];
int num_labels = 0;

// i 番目のオペランドを文字列として取得
struct Operand *GetOperand(char *mnemonic, struct Operand *operands, int n, int i) {
  if (n <= i) {
    fprintf(stderr, "too few operands for '%s': %d\n", mnemonic, n);
    exit(1);
  }
  return operands + i;
}

// i 番目のオペランドをレジスタ番号として取得
int GetOperandReg(struct Operand *operand) {
  if (operand->len == 1 && operand->tokens[0].kind < 16) {
    return operand->tokens[0].kind;
  }
  return -1;
}

// i 番目のオペランドを RegImm として取得
struct RegImm GetOperandRegImm(struct Operand *operand, int start_token, uint8_t imm_slot) {
  int token_idx = start_token;
  struct Token *value = operand->tokens + token_idx;
  struct Token *prefix = NULL;
  if (value->kind == kTokenByte || value->kind == kTokenWord) {
    prefix = value;
    token_idx++;
    value = operand->tokens + token_idx;
  }

  if (operand->len <= token_idx) {
    fprintf(stderr, "value must be specified\n");
    exit(1);
  } else if (operand->len > token_idx + 1) {
    struct Token *tk = operand->tokens + token_idx + 1;
    fprintf(stderr, "too many tokens: '%.*s'\n", tk->len, tk->raw);
    exit(1);
  }

  struct RegImm ri = {kReg, 0, NULL};
  if (prefix == NULL && value->kind < 16) {
    ri.val = value->kind;
    return ri;
  }

  if (prefix) {
    if (prefix->kind == kTokenWord) {
      ri.kind = kImm16;
    } else if (prefix->kind == kTokenByte) {
      ri.kind = kImm8;
    } else {
      fprintf(stderr, "unknown prefix: '%.*s'\n", prefix->len, prefix->raw);
      exit(1);
    }
  }

  if (value->kind == kTokenLabel) {
    if (prefix == NULL) {
      ri.kind = kImm8;
    }
    ri.label = strndup(value->raw, value->len);
    InitBackpatch(backpatches + num_backpatches, insn_idx, ri.label, BP_ABS + ri.kind);
    num_backpatches++;
  } else if (value->kind == kTokenRelLabel) {
    if (prefix == NULL) {
      ri.kind = kImm8;
    }
    ri.label = strndup(value->raw, value->len);
    InitBackpatch(backpatches + num_backpatches, insn_idx, ri.label, BP_IP_REL + ri.kind);
    num_backpatches++;
  } else if (value->kind == kTokenInt) {
    ri.val = value->val;
    if (0 <= ri.val && ri.val < 256) {
      if (prefix == NULL) {
        ri.kind = kImm8;
      }
    } else {
      ri.kind = kImm16;
    }
  } else if (value->kind == kTokenRelInt) {
    if (imm_slot & kImm8) { // imm8 が使用されているので imm16 を使うしかなく、必ず 3 ワードになる
      ri.val = value->val - ip - 3;
      ri.kind = kImm16;
    } else if (imm_slot & kImm16) { // imm16 が使用されているので必ず 3 ワードになる
      ri.val = value->val - ip - 3;
      ri.kind = kImm8;
    } else if (prefix && prefix->kind == kImm16) { // imm16 が指定されているので必ず 3 ワードになる
      ri.val = value->val - ip - 3;
    } else { // imm8 も imm16 も未使用なので、どちらを使うか選択権がある
      ri.val = value->val - ip - 2;
      if (0 <= ri.val && ri.val < 256) {
        if (prefix == NULL) {
          ri.kind = kImm8;
        }
      } else { // imm8 では表せないので imm16 を使う
        ri.val = value->val - ip - 3;
        ri.kind = kImm16;
      }
    }
  } else {
    fprintf(stderr, "unexpected token: '%.*s'\n", value->len, value->raw);
    exit(1);
  }

  if (prefix) {
    if ((prefix->kind == kTokenByte && ri.kind != kImm8) ||
        (prefix->kind == kTokenWord && ri.kind != kImm16)) {
      fprintf(stderr, "prefix conflicts with immediate size: '%.*s'\n", prefix->len, prefix->raw);
      exit(1);
    }
  }

  return ri;
}

#define GET_OPR(i) GetOperand((mnemonic), (operands), (num_opr), (i))
#define GET_REG(i) GetOperandReg(GET_OPR(i))
#define GET_REGIMM(i, imm_slot) GetOperandRegImm(GET_OPR(i), 0, (imm_slot))

void SetImm(struct Instruction *insn, enum RegImmKind imm_kind, uint16_t v) {
  if (imm_kind == kImm8) {
    insn->imm8 = v;
  } else if (imm_kind == kImm16) {
    insn->imm16 = v;
  }
}

// 入力レジスタ番号を insn に設定する。
// 入力が即値の場合は適切な即値番号（1 or 2）と即値を設定する。
//
// 戻り値
// -1: 入力が両方とも 8 ビットで表せない大きな数値である
// 2: 入力に byte リテラルが高々 1 つだけある
// 3: 入力に word リテラルが含まれる
int SetInput(struct Instruction *insn, struct RegImm *in1, struct RegImm *in2) {
  if (in2 == NULL) {
    if (in1->kind == kReg) {
      insn->in = in1->val << 4;
      return 2;
    } else {
      insn->in = in1->kind << 4;
      SetImm(insn, in1->kind, in1->val);
      return 1 + in1->kind;
    }
  }

  if (in1->kind == kReg && in2->kind == kReg) {
    insn->in = (in1->val << 4) | in2->val;
    return 2;
  } else if (in1->kind == kReg && in2->kind != kReg) {
    insn->in = (in1->val << 4) | in2->kind;
    SetImm(insn, in2->kind, in2->val);
    return 1 + in2->kind;
  } else if (in1->kind != kReg && in2->kind == kReg) {
    insn->in = (in1->kind << 4) | in2->val;
    SetImm(insn, in1->kind, in1->val);
    return 1 + in1->kind;
  } else { // in1, in2 両方が即値
    if (in1->kind == kImm16 && in2->kind == kImm16) {
      return -1;
    }
    if (in1->kind == kImm8) {
      insn->in = (kImm8 << 4) | kImm16;
      SetImm(insn, kImm8, in1->val);
      SetImm(insn, kImm16, in2->val);
    } else {
      insn->in = (kImm16 << 4) | kImm8;
      SetImm(insn, kImm16, in1->val);
      SetImm(insn, kImm8, in2->val);
    }
    return 3;
  }
}

// 前方ジャンプなら 1, 後方ジャンプなら 2 を返す。
// 前方ジャンプであれば jump_to->val を符号反転する。
int CalcJumpDirForIPRelImm(struct RegImm *jump_to) {
  int dir = 1;
  if (jump_to->label) {
    int i;
    for (i = 0; i < num_labels; i++) {
      if (strcmp(jump_to->label, labels[i].label) == 0) {
        break;
      }
    }
    if (i == num_labels) {
      dir = 2;
    }
  } else if (jump_to->val >= 0) {
    dir = 2;
  } else {
    jump_to->val = -jump_to->val;
  }
  return dir;
}

// ins->op の上位 4 ビットは、入力レジスタが 1 個なら 0、2 個なら 1
int SetInputForBranch(struct Instruction *ins, struct Operand *addr) {
  struct Token *tokens = addr->tokens;
  if (tokens[0].kind == kTokenInt || tokens[0].kind == kTokenLabel) {
    struct RegImm in = GetOperandRegImm(addr, 0, 0);
    ins->op = 0x00;
    return SetInput(ins, &in, NULL);
  }
  if (tokens[0].kind == kTokenByte || tokens[0].kind == kTokenWord ||
      tokens[0].kind == kTokenRelInt || tokens[0].kind == kTokenRelLabel) {
    struct RegImm in1 = {kReg, kRegIP, NULL};
    struct RegImm in2 = GetOperandRegImm(addr, 0, 0);
    ins->op = 0x10 | CalcJumpDirForIPRelImm(&in2);
    return SetInput(ins, &in1, &in2);
  }
  if (tokens[0].kind < 16) { // レジスタ加算
    char op;
    if (tokens[1].kind == '+' || tokens[1].kind == '-') {
      op = tokens[1].kind;
    } else {
      fprintf(stderr, "register-relative addressing needs +/-: '%.*s'\n",
              tokens[1].len, tokens[1].raw);
      exit(1);
    }
    struct RegImm in1 = {kReg, tokens[0].kind, NULL};
    struct RegImm in2 = GetOperandRegImm(addr, 2, 0);
    int dir = CalcJumpDirForIPRelImm(&in2);
    if (in2.label == NULL && op == '-') {
      dir = 3 - dir;
    }
    ins->op = 0x10 | dir;
    return SetInput(ins, &in1, &in2);
  }

  return -1;
}

int ProcALUOp1In(uint8_t op, uint8_t flag, struct Operand *opr_out,
                 struct Operand *opr_in) {
  insn[insn_idx].op = op;
  insn[insn_idx].out = (flag << 4) | GetOperandReg(opr_out);
  struct RegImm in = GetOperandRegImm(opr_in, 0, 0);
  return SetInput(insn + insn_idx, &in, NULL);
}

int ProcALUOp2In(uint8_t op, uint8_t flag, struct Operand *opr_out,
                 struct Operand *opr_in1, struct Operand *opr_in2) {
  insn[insn_idx].op = op;
  insn[insn_idx].out = (flag << 4) | GetOperandReg(opr_out);
  struct RegImm in1 = GetOperandRegImm(opr_in1, 0, 0);
  struct RegImm in2 = GetOperandRegImm(opr_in2, 0, in1.kind);
  return SetInput(insn + insn_idx, &in1, &in2);
}

#define ALU2OPR(op) \
  do { \
    insn_len = ProcALUOp1In((op), flag, GET_OPR(0), GET_OPR(1)); \
  } while (0)

#define ALU3OPR(op) \
  do { \
    insn_len = ProcALUOp2In((op), flag, GET_OPR(0), GET_OPR(1), GET_OPR(2)); \
    if (insn_len  == -1) { \
      \
      fprintf(stderr, "both literals are imm16: %s\n", line0); \
      exit(1); \
    } \
  } while (0)

int main(int argc, char **argv) {
  char line[256], line0[256];
  char *label;
  char *mnemonic;
  struct Operand operands[MAX_OPERAND];

  while (fgets(line, sizeof(line), stdin) != NULL) {
    strcpy(line0, line);
    int num_opr = SplitOpcode(line, &label, &mnemonic, operands, MAX_OPERAND);

    if (label) {
      labels[num_labels].label = strdup(label);
      labels[num_labels].ip = ip;
      num_labels++;
    }

    if (num_opr < 0) {
      continue;
    }
    ToLower(mnemonic);

    char *sep = strchr(mnemonic, '.');
    uint8_t flag = 1; // always do
    if (sep) {
      char *flag_name = sep + 1;
      *sep = '\0';
      flag = FlagNameToBits(flag_name);
    }

    int insn_len = 2;
    if (strcmp(mnemonic, "add") == 0) {
      ALU3OPR(0x12);
    } else if (strcmp(mnemonic, "sub") == 0) {
      ALU3OPR(0x11);
    } else if (strcmp(mnemonic, "addc") == 0) {
      ALU3OPR(0x16);
    } else if (strcmp(mnemonic, "subc") == 0) {
      ALU3OPR(0x15);
    } else if (strcmp(mnemonic, "or") == 0) {
      ALU3OPR(0x0a);
    } else if (strcmp(mnemonic, "not") == 0) {
      ALU2OPR(0x0c);
    } else if (strcmp(mnemonic, "xor") == 0) {
      ALU3OPR(0x0e);
    } else if (strcmp(mnemonic, "and") == 0) {
      ALU3OPR(0x06);
    } else if (strcmp(mnemonic, "inc") == 0) {
      ALU2OPR(0x1b);
    } else if (strcmp(mnemonic, "dec") == 0) {
      ALU2OPR(0x18);
    } else if (strcmp(mnemonic, "incc") == 0) {
      ALU2OPR(0x1f);
    } else if (strcmp(mnemonic, "decc") == 0) {
      ALU2OPR(0x1c);
    } else if (strcmp(mnemonic, "slr") == 0) {
      ALU2OPR(0x2c);
    } else if (strcmp(mnemonic, "sll") == 0) {
      ALU2OPR(0x20);
    } else if (strcmp(mnemonic, "sar") == 0) {
      ALU2OPR(0x2c);
    } else if (strcmp(mnemonic, "sal") == 0) {
      ALU2OPR(0x24);
    } else if (strcmp(mnemonic, "ror") == 0) {
      ALU2OPR(0x2a);
    } else if (strcmp(mnemonic, "rol") == 0) {
      ALU2OPR(0x22);
    } else if (strcmp(mnemonic, "mov") == 0) {
      insn[insn_idx].op = 0x00;
      insn[insn_idx].out = (flag << 4) | GET_REG(0);
      struct RegImm in = GET_REGIMM(1, 0);
      insn_len = SetInput(insn + insn_idx, &in, NULL);
    } else if (strcmp(mnemonic, "jmp") == 0) {
      insn_len = SetInputForBranch(insn + insn_idx, operands + 0);
      if (insn_len < 0) {
        fprintf(stderr, "invalid jump instruction: %s\n", line0);
        exit(1);
      }
      insn[insn_idx].out = (flag << 4) | kRegIP;
    } else if (strcmp(mnemonic, "call") == 0) {
      insn_len = SetInputForBranch(insn + insn_idx, operands + 0);
      if (insn_len < 0) {
        fprintf(stderr, "invalid call instruction: %s\n", line0);
        exit(1);
      }
      insn[insn_idx].op |= 0xD0;
      insn[insn_idx].out = (flag << 4) | kRegIP;
    } else if (strcmp(mnemonic, "load") == 0) {
      insn_len = SetInputForBranch(insn + insn_idx, operands + 1);
      if (insn_len < 0) {
        fprintf(stderr, "invalid load instruction: %s\n", line0);
        exit(1);
      }
      insn[insn_idx].op = (insn[insn_idx].op & 0x0f) | 0x80;
      insn[insn_idx].out = (flag << 4) | GET_REG(0);
    } else if (strcmp(mnemonic, "store") == 0) {
      insn_len = SetInputForBranch(insn + insn_idx, operands);
      if (insn_len < 0) {
        fprintf(stderr, "invalid store instruction: %s\n", line0);
        exit(1);
      }
      insn[insn_idx].op = (insn[insn_idx].op & 0x0f) | 0x90;
      insn[insn_idx].out = (flag << 4) | GET_REG(1);
    } else if (strcmp(mnemonic, "push") == 0) {
      insn[insn_idx].op = 0xd0;
      insn[insn_idx].out = (flag << 4) | GET_REG(0);
      insn_len = 1;
    } else if (strcmp(mnemonic, "pop") == 0) {
      insn[insn_idx].op = 0xc0;
      insn[insn_idx].out = (flag << 4) | GET_REG(0);
      insn_len = 1;
    } else if (strcmp(mnemonic, "cmp") == 0) {
      insn[insn_idx].op = 0x11;
      insn[insn_idx].out = (flag << 4) | kRegZR;
      struct RegImm in1 = GET_REGIMM(0, 0);
      struct RegImm in2 = GET_REGIMM(1, in1.kind);
      insn_len = SetInput(insn + insn_idx, &in1, &in2);
    } else if (strcmp(mnemonic, "ret") == 0) {
      insn[insn_idx].op = 0xc0;
      insn[insn_idx].out = (flag << 4) | kRegIP;
      insn_len = 1;
    } else if (strcmp(mnemonic, "iret") == 0) {
      insn[insn_idx].op = 0xe0;
      insn[insn_idx].out = (flag << 4) | kRegIP;
      insn_len = 1;
    } else if (strcmp(mnemonic, "dw") == 0) {
      struct Token *t = operands[0].tokens;
      if (operands[0].len != 1 || t->kind != kTokenInt) {
        fprintf(stderr, "dw takes just one integer: %s\n", line0);
        exit(1);
      }
      uint16_t data = t->val;
      insn[insn_idx].op = data >> 8;
      insn[insn_idx].out = data & 0xff;
      insn_len = 1;
    } else {
      fprintf(stderr, "unknown mnemonic: '%s'\n", mnemonic);
      exit(1);
    }

    insn[insn_idx].ip = ip;
    insn[insn_idx].len = insn_len;
    ip += insn_len;
    insn_idx++;
  }

  for (int i = 0; i < num_backpatches; i++) {
    int l = 0;
    for (; l < num_labels; l++) {
      if (strcmp(backpatches[i].label, labels[l].label)) {
        continue;
      }

      struct Instruction *target_insn = insn + backpatches[i].insn_idx;
      switch (backpatches[i].type) {
      case BP_ABS8:
        if (labels[l].ip >= 256) {
          fprintf(stderr, "label cannot be fit in imm8: '%s' -> %d\n",
                  labels[l].label, labels[l].ip);
          exit(1);
        }
        target_insn->imm8 = labels[l].ip;
        break;
      case BP_ABS16:
        target_insn->imm16 = labels[l].ip;
        break;
      case BP_IP_REL8:
      case BP_IP_REL16: {
        int ip_base = target_insn->ip + target_insn->len;
        int ip_diff = labels[l].ip - ip_base;
        if (ip_diff < 0) {
          ip_diff = -ip_diff;
        }
        if (backpatches[i].type == BP_IP_REL16) {
          target_insn->imm16 = ip_diff;
        } else if (ip_diff >= 256) {
          fprintf(stderr, "ip-diff cannot be fit in imm8: abs('%s' - %d) -> %d\n",
                  labels[l].label, ip_base, ip_diff);
          exit(1);
        } else {
          target_insn->imm8 = ip_diff;
        }
        break;
      }
      default:
        fprintf(stderr, "unknown relocation type: %d\n", backpatches[i].type);
        exit(1);
      }
      break;
    }
    if (l == num_labels) {
      fprintf(stderr, "unknown label: %s\n", backpatches[i].label);
      exit(1);
    }
  }

  int debug = 0;
  if (argc > 1 && strcmp(argv[1], "-d") == 0) {
    debug = 1;
  }

  for (int i = 0; i < insn_idx; i++) {
    if (debug) {
      printf("%08x: ", insn[i].ip);
    }

    printf("%02X%02X%c", insn[i].op, insn[i].out, debug ? ' ' : '\n');
    if (insn[i].len >= 2) {
      printf("%02X%02X%c", insn[i].in, insn[i].imm8, debug ? ' ' : '\n');
    }
    if (insn[i].len >= 3) {
      printf("%04X%c", insn[i].imm16, debug ? ' ' : '\n');
    }

    if (debug) {
      printf(" # ");
      switch (insn[i].op) {
      case 0x12: printf("add"); break;
      case 0x11: printf("sub"); break;
      case 0x16: printf("addc"); break;
      case 0x15: printf("subc"); break;
      case 0x0a: printf("or"); break;
      case 0x0c: printf("not"); break;
      case 0x0e: printf("xor"); break;
      case 0x06: printf("and"); break;
      case 0x1b: printf("inc"); break;
      case 0x18: printf("dec"); break;
      case 0x1f: printf("incc"); break;
      case 0x1c: printf("decc"); break;
      case 0x2c: printf("slr"); break;
      case 0x20: printf("sll"); break;
      case 0x24: printf("sal"); break;
      case 0x2a: printf("ror"); break;
      case 0x22: printf("rol"); break;
      case 0x00: printf("mov"); break;
      case 0xd0: printf("push"); break;
      case 0xc0: printf("pop"); break;
      case 0xd2: printf("call"); break;
      case 0xd1: printf("call"); break;
      case 0xe0: printf("iret"); break;
      case 0x80: printf("load"); break;
      case 0x82: printf("load"); break;
      case 0x81: printf("load"); break;
      case 0x90: printf("store"); break;
      case 0x92: printf("store"); break;
      case 0x91: printf("store"); break;
      }
      printf("\n");
    }
  }
  return 0;
}
