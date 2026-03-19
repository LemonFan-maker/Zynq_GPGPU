/*
 * gpuasm - Zynq GPGPU Assembler
 *
 * 用法: ./gpuasm input.asm [-o output.h]
 *
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_LINES    1024
#define MAX_LABELS   256
#define MAX_LINE_LEN 256
#define MAX_TOKENS   8

typedef struct {
    char name[64];
    int  pc;
} Label;

static Label labels[MAX_LABELS];
static int   label_count = 0;

static int label_find(const char *name) {
    for (int i = 0; i < label_count; i++)
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].pc;
    return -1;
}

static void label_add(const char *name, int pc) {
    if (label_count >= MAX_LABELS) {
        fprintf(stderr, "[错误的]: 标签过多 (最多 %d 个)\n", MAX_LABELS);
        exit(1);
    }
    if (label_find(name) >= 0) {
        fprintf(stderr, "[错误的]: 重复的标签 '%s'\n", name);
        exit(1);
    }
    strncpy(labels[label_count].name, name, 63);
    labels[label_count].name[63] = '\0';
    labels[label_count].pc = pc;
    label_count++;
}

typedef struct {
    const char *mnemonic;
    int         opcode;
    enum { FMT_R, FMT_R_MAC, FMT_I8, FMT_I13, FMT_BR, FMT_JMP, FMT_NOP, FMT_HALT } fmt;
} OpEntry;

static const OpEntry optable[] = {
    {"ADD",  0x0, FMT_R},
    {"SUB",  0x1, FMT_R},
    {"MUL",  0x2, FMT_R},
    {"MAC",  0x2, FMT_R_MAC},
    {"AND",  0x3, FMT_R},
    {"OR",   0x4, FMT_R},
    {"XOR",  0x5, FMT_R},
    {"SLL",  0x6, FMT_R},
    {"SRL",  0x7, FMT_R},
    {"LDR",  0x8, FMT_I8},
    {"STR",  0x9, FMT_I8},
    {"SETM", 0xA, FMT_R},
    {"SLT",  0xB, FMT_R},
    {"ADDI", 0xC, FMT_I13},
    {"BEQ",  0xD, FMT_BR},
    {"BNE",  0xE, FMT_BR},
    {"JMP",  0xF, FMT_JMP},
    {"NOP",  -1,  FMT_NOP},
    {"HALT", -1,  FMT_HALT},
    {NULL, 0, 0}
};

static const OpEntry *op_lookup(const char *mnemonic) {
    for (int i = 0; optable[i].mnemonic; i++) {
        if (strcasecmp(optable[i].mnemonic, mnemonic) == 0)
            return &optable[i];
    }
    return NULL;
}

/* 解析寄存器: r0..r31, 返回编号或-1 */
static int parse_reg(const char *s) {
    if ((s[0] == 'r' || s[0] == 'R') && isdigit(s[1])) {
        int n = atoi(s + 1);
        if (n >= 0 && n <= 31) return n;
    }
    return -1;
}

/* 解析立即数: 十进制、0x十六进制、负数 */
static int parse_imm(const char *s) {
    char *end;
    long v = strtol(s, &end, 0);
    if (end == s) {
        fprintf(stderr, "[错误的]: 无效的立即数 '%s'\n", s);
        exit(1);
    }
    return (int)v;
}

/* 去掉字符串中的逗号和方括号，分词 */
static int tokenize(char *line, char *tokens[], int max_tokens) {
    /* 先去掉注释 */
    char *semi = strchr(line, ';');
    if (semi) *semi = '\0';

    /* 替换 [ ] + , 为空格 */
    for (char *p = line; *p; p++) {
        if (*p == '[' || *p == ']' || *p == '+' || *p == ',')
            *p = ' ';
    }

    int count = 0;
    char *tok = strtok(line, " \t\r\n");
    while (tok && count < max_tokens) {
        tokens[count++] = tok;
        tok = strtok(NULL, " \t\r\n");
    }
    return count;
}

static uint32_t encode_r(int opcode, int rd, int rs1, int rs2) {
    return ((opcode & 0xF) << 28) | ((rd & 0x1F) << 23) |
           ((rs2 & 0x1F) << 13)  | ((rs1 & 0x1F) << 8);
}

static uint32_t encode_i8(int opcode, int rd, int rs1, int imm8) {
    return ((opcode & 0xF) << 28) | ((rd & 0x1F) << 23) |
           ((rs1 & 0x1F) << 8)   | (imm8 & 0xFF);
}

static uint32_t encode_i13(int opcode, int rd, int rs1, int imm13) {
    int hi5 = (imm13 >> 8) & 0x1F;
    int lo8 = imm13 & 0xFF;
    return ((opcode & 0xF) << 28) | ((rd & 0x1F) << 23) |
           (hi5 << 13) | ((rs1 & 0x1F) << 8) | lo8;
}

static uint32_t encode_branch(int opcode, int rA, int rB, int offset13) {
    int hi5 = (offset13 >> 8) & 0x1F;
    int lo8 = offset13 & 0xFF;
    return ((opcode & 0xF) << 28) | ((rB & 0x1F) << 23) |
           (hi5 << 13) | ((rA & 0x1F) << 8) | lo8;
}

static uint32_t encode_jmp(int imm13) {
    int hi5 = (imm13 >> 8) & 0x1F;
    int lo8 = imm13 & 0xFF;
    return (0xF << 28) | (hi5 << 13) | lo8;
}

typedef struct {
    char text[MAX_LINE_LEN];
    int  line_no;  /* 原始行号 */
} SourceLine;

static SourceLine src_lines[MAX_LINES];
static int src_count = 0;

static void pass1(FILE *fp) {
    char buf[MAX_LINE_LEN];
    int pc = 0;
    int line_no = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        line_no++;
        char line[MAX_LINE_LEN];
        strncpy(line, buf, MAX_LINE_LEN - 1);
        line[MAX_LINE_LEN - 1] = '\0';

        char *tokens[MAX_TOKENS];
        int ntok = tokenize(line, tokens, MAX_TOKENS);

        if (ntok == 0) continue;

        int ti = 0;
        /* 检查标签 */
        size_t len = strlen(tokens[0]);
        if (len > 0 && tokens[0][len - 1] == ':') {
            tokens[0][len - 1] = '\0';
            label_add(tokens[0], pc);
            ti = 1;
        }

        if (ti >= ntok) continue; /* 只有标签的行 */

        /* 检查是否是有效指令 */
        const OpEntry *op = op_lookup(tokens[ti]);
        if (!op) {
            fprintf(stderr, "[错误的]: 第 %d 行: '%s' 这是未知的指令\n",
                    line_no, tokens[ti]);
            exit(1);
        }

        /* 保存源代码行 */
        if (src_count >= MAX_LINES) {
            fprintf(stderr, "[错误的]: 过多的指令传输进来 (最大 %d)\n", MAX_LINES);
            exit(1);
        }
        strncpy(src_lines[src_count].text, buf, MAX_LINE_LEN - 1);
        src_lines[src_count].text[MAX_LINE_LEN - 1] = '\0';
        /* 去掉换行 */
        char *nl = strchr(src_lines[src_count].text, '\n');
        if (nl) *nl = '\0';
        nl = strchr(src_lines[src_count].text, '\r');
        if (nl) *nl = '\0';
        src_lines[src_count].line_no = line_no;
        src_count++;
        pc++;
    }
}

static uint32_t pass2_encode(int pc, const char *original_line, int line_no) {
    char line[MAX_LINE_LEN];
    strncpy(line, original_line, MAX_LINE_LEN - 1);
    line[MAX_LINE_LEN - 1] = '\0';

    char *tokens[MAX_TOKENS];
    int ntok = tokenize(line, tokens, MAX_TOKENS);

    if (ntok == 0) return 0;

    int ti = 0;
    /* 跳过标签 */
    size_t len = strlen(tokens[0]);
    if (len > 0 && tokens[0][len - 1] == ':') {
        ti = 1;
    }

    const char *mnem = tokens[ti];
    const OpEntry *op = op_lookup(mnem);

    if (!op) {
        fprintf(stderr, "[错误的]: 第 %d 行: '%s' 这是未知的指令\n", line_no, mnem);
        exit(1);
    }

    int rd, rs1, rs2, imm;

    switch (op->fmt) {
    case FMT_NOP:
        return 0x00000000;

    case FMT_HALT:
        return 0x00000001;

    case FMT_R:
        /* OP rd, rs1, rs2 */
        if (ntok - ti < 4) {
            fprintf(stderr, "[错误的]: 第 %d 行: 指令 '%s' 需要3个寄存器操作数\n", line_no, mnem);
            exit(1);
        }
        rd  = parse_reg(tokens[ti + 1]);
        rs1 = parse_reg(tokens[ti + 2]);
        rs2 = parse_reg(tokens[ti + 3]);
        if (rd < 0 || rs1 < 0 || rs2 < 0) {
            fprintf(stderr, "[错误的]: 第 %d 行: 指令 '%s' 中的寄存器无效\n", line_no, mnem);
            exit(1);
        }
        return encode_r(op->opcode, rd, rs1, rs2);

    case FMT_R_MAC:
        /* MAC rd, rs1, rs2 — same as MUL but imm8[0]=1 */
        if (ntok - ti < 4) {
            fprintf(stderr, "[错误的]: 第 %d 行: 指令 '%s' 需要3个寄存器操作数\n", line_no, mnem);
            exit(1);
        }
        rd  = parse_reg(tokens[ti + 1]);
        rs1 = parse_reg(tokens[ti + 2]);
        rs2 = parse_reg(tokens[ti + 3]);
        if (rd < 0 || rs1 < 0 || rs2 < 0) {
            fprintf(stderr, "[错误的]: 第 %d 行: 指令 '%s' 中的寄存器无效\n", line_no, mnem);
            exit(1);
        }
        return encode_r(op->opcode, rd, rs1, rs2) | 0x01;

    case FMT_I8:
        /* LDR rd, [rs1 + imm8]  or  STR rs2, [rs1 + imm8] */
        if (ntok - ti < 4) {
            fprintf(stderr, "[错误的]: 第 %d 行: 指令 '%s' 需要 rd/rs2, rs1, imm8\n", line_no, mnem);
            exit(1);
        }
        if (op->opcode == 0x8) { /* LDR */
            rd  = parse_reg(tokens[ti + 1]);
            rs1 = parse_reg(tokens[ti + 2]);
            imm = parse_imm(tokens[ti + 3]);
            return encode_i8(op->opcode, rd, rs1, imm);
        } else { /* STR */
            rs2 = parse_reg(tokens[ti + 1]);
            rs1 = parse_reg(tokens[ti + 2]);
            imm = parse_imm(tokens[ti + 3]);
            /* STR 编码: rs2 放在 [17:13] */
            return ((op->opcode & 0xF) << 28) | ((rs2 & 0x1F) << 13) |
                   ((rs1 & 0x1F) << 8) | (imm & 0xFF);
        }

    case FMT_I13:
        /* ADDI rd, rs1, imm13 */
        if (ntok - ti < 4) {
            fprintf(stderr, "[错误的]: 第 %d 行: ADDI 需要 rd, rs1, imm\n", line_no);
            exit(1);
        }
        rd  = parse_reg(tokens[ti + 1]);
        rs1 = parse_reg(tokens[ti + 2]);
        imm = parse_imm(tokens[ti + 3]);
        return encode_i13(op->opcode, rd, rs1, imm & 0x1FFF);

    case FMT_BR: {
        /* BEQ/BNE rA, rB, label_or_imm */
        if (ntok - ti < 4) {
            fprintf(stderr, "[错误的]: 第 %d 行: 指令 '%s' 需要 rA, rB, target\n", line_no, mnem);
            exit(1);
        }
        int rA = parse_reg(tokens[ti + 1]);
        int rB = parse_reg(tokens[ti + 2]);
        if (rA < 0 || rB < 0) {
            fprintf(stderr, "[错误的]: 第 %d 行: 指令 '%s' 中的寄存器无效\n", line_no, mnem);
            exit(1);
        }
        /* 尝试标签 */
        int target_pc = label_find(tokens[ti + 3]);
        if (target_pc >= 0) {
            imm = target_pc - pc;
        } else {
            imm = parse_imm(tokens[ti + 3]);
        }
        return encode_branch(op->opcode, rA, rB, imm & 0x1FFF);
    }

    case FMT_JMP:
        /* JMP label_or_imm */
        if (ntok - ti < 2) {
            fprintf(stderr, "[错误的]: 第 %d 行: JMP 需要目标\n", line_no);
            exit(1);
        }
        {
            int target_pc = label_find(tokens[ti + 1]);
            if (target_pc >= 0) {
                imm = target_pc;
            } else {
                imm = parse_imm(tokens[ti + 1]);
            }
        }
        return encode_jmp(imm & 0x1FFF);

    default:
        return 0;
    }
}

int main(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        } else {
            fprintf(stderr, "[用法]: gpuasm input.asm [-o output.h]\n");
            return 1;
        }
    }

    if (!input_file) {
        fprintf(stderr, "[用法]: gpuasm input.asm [-o output.h]\n");
        return 1;
    }

    /* 默认输出文件名 */
    char default_out[256];
    if (!output_file) {
        strncpy(default_out, input_file, 240);
        char *dot = strrchr(default_out, '.');
        if (dot) strcpy(dot, ".h");
        else strcat(default_out, ".h");
        output_file = default_out;
    }

    /* Pass 1 */
    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        perror(input_file);
        return 1;
    }
    pass1(fp);
    fclose(fp);

    /* Pass 2 */
    uint32_t code[MAX_LINES];
    for (int i = 0; i < src_count; i++) {
        code[i] = pass2_encode(i, src_lines[i].text, src_lines[i].line_no);
    }

    /* 输出 */
    FILE *out = fopen(output_file, "w");
    if (!out) {
        perror(output_file);
        return 1;
    }

    /* 提取文件名做变量名前缀 */
    const char *basename = strrchr(input_file, '/');
    basename = basename ? basename + 1 : input_file;
    char varname[128];
    strncpy(varname, basename, 127);
    char *dot = strchr(varname, '.');
    if (dot) *dot = '\0';
    /* 替换非法字符 */
    for (char *p = varname; *p; p++) {
        if (!isalnum(*p)) *p = '_';
    }

    fprintf(out, "// 由 gpuasm 从 %s 之中生成\n", input_file);
    fprintf(out, "#include <stdint.h>\n\n");
    fprintf(out, "uint32_t %s[] = {\n", varname);

    for (int i = 0; i < src_count; i++) {
        /* 清理原始行做注释 */
        char comment[MAX_LINE_LEN];
        strncpy(comment, src_lines[i].text, MAX_LINE_LEN - 1);
        comment[MAX_LINE_LEN - 1] = '\0';
        /* 去掉前导空白 */
        char *c = comment;
        while (*c == ' ' || *c == '\t') c++;

        fprintf(out, "    0x%08X,  // [%d] %s\n", code[i], i, c);
    }

    fprintf(out, "};\n");
    fprintf(out, "#define %s_LEN %d\n", varname, src_count);
    fclose(out);

    printf("gpuasm: %s -> %s (一共 %d 条指令)\n", input_file, output_file, src_count);
    return 0;
}
