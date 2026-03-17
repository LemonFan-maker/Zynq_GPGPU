#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_cache.h"

#define GPU_BASE_ADDR  0x40000000
#define GPU_CTRL_REG   (GPU_BASE_ADDR + 0x000)
#define GPU_STATUS_REG (GPU_BASE_ADDR + 0x004)
#define GPU_IMEM_BASE  (GPU_BASE_ADDR + 0x100)
#define GPU_DMEM_BASE  (GPU_BASE_ADDR + 0x1000)

// R-type: [opcode:4][rd:5][reserved:5][rs2:5][rs1:5][imm8:8]
#define R_INST(op, rd, rs1, rs2) \
    (((op)&0xF)<<28 | ((rd)&0x1F)<<23 | ((rs2)&0x1F)<<13 | ((rs1)&0x1F)<<8)

#define ADD(rd, rs1, rs2)  R_INST(0x0, rd, rs1, rs2)
#define SUB(rd, rs1, rs2)  R_INST(0x1, rd, rs1, rs2)
#define MUL(rd, rs1, rs2)  R_INST(0x2, rd, rs1, rs2)
#define AND(rd, rs1, rs2)  R_INST(0x3, rd, rs1, rs2)
#define OR(rd, rs1, rs2)   R_INST(0x4, rd, rs1, rs2)
#define XOR(rd, rs1, rs2)  R_INST(0x5, rd, rs1, rs2)
#define SLL(rd, rs1, rs2)  R_INST(0x6, rd, rs1, rs2)
#define SRL(rd, rs1, rs2)  R_INST(0x7, rd, rs1, rs2)
#define SLT(rd, rs1, rs2)  R_INST(0xB, rd, rs1, rs2)
#define SETM(rd, rs1, rs2) R_INST(0xA, rd, rs1, rs2)

// I-type (8-bit imm): [opcode:4][rd:5][reserved:5][rs2:5][rs1:5][imm8:8]
#define LDR(rd, rs1, imm8)  (0x80000000 | ((rd)&0x1F)<<23 | ((rs1)&0x1F)<<8 | ((imm8)&0xFF))
#define STR(rs2, rs1, imm8) (0x90000000 | ((rs2)&0x1F)<<13 | ((rs1)&0x1F)<<8 | ((imm8)&0xFF))

// I13-type (13-bit imm): imm13 = {[17:13], [7:0]}
// ADDI rd, rs1, imm13
#define ADDI(rd, rs1, imm13) \
    (0xC0000000 | ((rd)&0x1F)<<23 | (((imm13)>>8)&0x1F)<<13 | ((rs1)&0x1F)<<8 | ((imm13)&0xFF))

// BEQ rA, rB, offset13: rA=[12:8](rs1), rB=[27:23](rd field), imm13={[17:13],[7:0]}
#define BEQ(rA, rB, off13) \
    (0xD0000000 | ((rB)&0x1F)<<23 | (((off13)>>8)&0x1F)<<13 | ((rA)&0x1F)<<8 | ((off13)&0xFF))

// BNE rA, rB, offset13
#define BNE(rA, rB, off13) \
    (0xE0000000 | ((rB)&0x1F)<<23 | (((off13)>>8)&0x1F)<<13 | ((rA)&0x1F)<<8 | ((off13)&0xFF))

// JMP imm13 (绝对跳转)
#define JMP(imm13) \
    (0xF0000000 | (((imm13)>>8)&0x1F)<<13 | ((imm13)&0xFF))

#define NOP ADD(0, 0, 0)

// DMEM entry N, lane L (0-3)
#define DMEM_WRITE(entry, lane, val) \
    Xil_Out32(GPU_DMEM_BASE + (entry)*16 + (lane)*4, (val))

#define DMEM_READ(entry, lane) \
    Xil_In32(GPU_DMEM_BASE + (entry)*16 + (lane)*4)

static int test_pass = 0;
static int test_fail = 0;

static void upload_and_run(uint32_t *prog, int len, int wait_cycles) {
    // 停止
    Xil_Out32(GPU_CTRL_REG, 0x00000000);

    // 上传指令
    for (int i = 0; i < len; i++) {
        Xil_Out32(GPU_IMEM_BASE + i * 4, prog[i]);
    }
    // 填充NOP到程序末尾
    for (int i = len; i < 256; i++) {
        Xil_Out32(GPU_IMEM_BASE + i * 4, NOP);
    }

    // 启动GPU
    Xil_Out32(GPU_CTRL_REG, 0x00000001);

    // 等待执行
    for (volatile int i = 0; i < wait_cycles; i++);

    // 停止GPU
    Xil_Out32(GPU_CTRL_REG, 0x00000000);
}

static void check(const char *name, uint32_t actual, uint32_t expected) {
    if (actual == expected) {
        xil_printf("  [PASS] %s = %d\n\r", name, actual);
        test_pass++;
    } else {
        xil_printf("  [FAIL] %s = %d (expected %d)\n\r", name, actual, expected);
        test_fail++;
    }
}

// ALU
static void test_alu_basic(void) {
    xil_printf("\n\r=== ALU ===\n\r");

    // DMEM[10] = {7, 7, 7, 7}, DMEM[11] = {3, 3, 3, 3}
    for (int l = 0; l < 4; l++) {
        DMEM_WRITE(10, l, 7);
        DMEM_WRITE(11, l, 3);
    }
    // 清空结果区DMEM[20..28]
    for (int e = 20; e <= 28; e++)
        for (int l = 0; l < 4; l++)
            DMEM_WRITE(e, l, 0);

    uint32_t prog[] = {
        LDR(1, 0, 10), LDR(1, 0, 10),
        LDR(2, 0, 11), LDR(2, 0, 11),
        NOP,
        ADD(3, 1, 2), NOP, STR(3, 0, 20),
        SUB(4, 1, 2), NOP, STR(4, 0, 21),
        MUL(5, 1, 2), NOP, STR(5, 0, 22),
        AND(6, 1, 2), NOP, STR(6, 0, 23),
        OR(7, 1, 2),  NOP, STR(7, 0, 24),
        XOR(8, 1, 2), NOP, STR(8, 0, 25),
        SLL(9, 1, 2), NOP, STR(9, 0, 26),
        SRL(10, 1, 2), NOP, STR(10, 0, 27),
        SLT(11, 2, 1), NOP, STR(11, 0, 28),
    };
    upload_and_run(prog, sizeof(prog)/4, 10000);

    check("ADD 7+3",  DMEM_READ(20, 0), 10);
    check("SUB 7-3",  DMEM_READ(21, 0), 4);
    check("MUL 7*3",  DMEM_READ(22, 0), 21);
    check("AND 7&3",  DMEM_READ(23, 0), 3);
    check("OR  7|3",  DMEM_READ(24, 0), 7);
    check("XOR 7^3",  DMEM_READ(25, 0), 4);
    check("SLL 7<<3", DMEM_READ(26, 0), 56);
    check("SRL 7>>3", DMEM_READ(27, 0), 0);
    check("SLT 3<7",  DMEM_READ(28, 0), 1);
}

// ADDI
static void test_addi(void) {
    xil_printf("\n\r=== ADDI ===\n\r");

    for (int l = 0; l < 4; l++) {
        DMEM_WRITE(10, l, 100);
        DMEM_WRITE(30, l, 0);
        DMEM_WRITE(31, l, 0);
    }

    uint32_t prog[] = {
        LDR(1, 0, 10), LDR(1, 0, 10),
        NOP,
        ADDI(2, 1, 50), NOP, STR(2, 0, 30),
        ADDI(3, 0, 42), NOP, STR(3, 0, 31),
    };
    upload_and_run(prog, sizeof(prog)/4, 5000);

    check("ADDI 100+50", DMEM_READ(30, 0), 150);
    check("ADDI 0+42",   DMEM_READ(31, 0), 42);
}

// LDR/STR+AXI
static void test_ldr_str(void) {
    xil_printf("\n\r=== LDR/STR+AXI ===\n\r");

    // 写入DMEM[40]={11, 22, 33, 44}
    DMEM_WRITE(40, 0, 11);
    DMEM_WRITE(40, 1, 22);
    DMEM_WRITE(40, 2, 33);
    DMEM_WRITE(40, 3, 44);
    for (int l = 0; l < 4; l++) DMEM_WRITE(50, l, 0);

    // GPU: LDR r1, [40], STR r1, [50]
    uint32_t prog[] = {
        LDR(1, 0, 40), LDR(1, 0, 40),
        NOP,
        STR(1, 0, 50),
    };
    upload_and_run(prog, sizeof(prog)/4, 5000);

    // ARM通过AXI回读DMEM[50]
    check("STR/LDR Lane0", DMEM_READ(50, 0), 11);
    check("STR/LDR Lane1", DMEM_READ(50, 1), 22);
    check("STR/LDR Lane2", DMEM_READ(50, 2), 33);
    check("STR/LDR Lane3", DMEM_READ(50, 3), 44);
}

// SETM Mask
static void test_setm(void) {
    xil_printf("\n\r=== SETM Mask ===\n\r");

    // DMEM[10] = {10, 100, 30, 100}  (lane0=10, lane1=100, lane2=30, lane3=100)
    DMEM_WRITE(10, 0, 10);
    DMEM_WRITE(10, 1, 100);
    DMEM_WRITE(10, 2, 30);
    DMEM_WRITE(10, 3, 100);
    
    // DMEM[14] = {100, 100, 100, 100}
    for (int l = 0; l < 4; l++) DMEM_WRITE(14, l, 100);
    for (int l = 0; l < 4; l++) DMEM_WRITE(60, l, 0);

    uint32_t prog[] = {
        LDR(1, 0, 10), LDR(1, 0, 10),
        LDR(2, 0, 14), LDR(2, 0, 14),
        NOP,
        SETM(0, 1, 2), NOP,
        ADDI(3, 0, 99), NOP,
        STR(3, 0, 60),
    };
    upload_and_run(prog, sizeof(prog)/4, 5000);

    check("SETM Lane0 (masked)",   DMEM_READ(60, 0), 0);
    check("SETM Lane1 (active)",   DMEM_READ(60, 1), 99);
    check("SETM Lane2 (masked)",   DMEM_READ(60, 2), 0);
    check("SETM Lane3 (active)",   DMEM_READ(60, 3), 99);
}

// BNE
static void test_branch_loop(void) {
    xil_printf("\n\r=== BNE ===\n\r");

    for (int l = 0; l < 4; l++) {
        DMEM_WRITE(70, l, 0);
        DMEM_WRITE(71, l, 0);
    }

    uint32_t prog[] = {
        ADDI(1, 0, 0),          // [0] r1 = 0
        ADDI(2, 0, 0),          // [1] r2 = 0
        ADDI(3, 0, 5),          // [2] r3 = 5
        ADDI(4, 0, 10),         // [3] r4 = 10
        ADD(1, 1, 4),           // [4] r1 += r4 (loop start)
        NOP,                    // [5] wait writeback
        ADDI(2, 2, 1),          // [6] r2++
        NOP,                    // [7] wait writeback
        BNE(2, 3, (-4)&0x1FFF),// [8] if r2!=r3 goto PC-4=4
        NOP,                    // [9]
        STR(1, 0, 70),          // [10] store result
        STR(2, 0, 71),          // [11] store counter
    };
    upload_and_run(prog, sizeof(prog)/4, 20000);

    check("BNE loop sum 10*5", DMEM_READ(70, 0), 50);
    check("BNE loop counter",  DMEM_READ(71, 0), 5);
}

// JMP
static void test_jmp(void) {
    xil_printf("\n\r=== JMP ===\n\r");

    for (int l = 0; l < 4; l++) {
        DMEM_WRITE(80, l, 0);
        DMEM_WRITE(81, l, 0);
    }

    uint32_t prog[] = {
        ADDI(1, 0, 11),         // [0]
        JMP(3),                 // [1] jump to [3]
        ADDI(1, 0, 99),         // [2] should be skipped
        NOP,                    // [3] wait for r1 writeback
        STR(1, 0, 80),          // [4]
        ADDI(2, 0, 77),         // [5]
        NOP,                    // [6]
        STR(2, 0, 81),          // [7]
    };
    upload_and_run(prog, sizeof(prog)/4, 5000);

    check("JMP skip (expect 11)", DMEM_READ(80, 0), 11);
    check("JMP continue",         DMEM_READ(81, 0), 77);
}

// Dot Product
static void test_dot_product(void) {
    xil_printf("\n\r=== Dot Product ===\n\r");

    for (int l = 0; l < 4; l++) {
        DMEM_WRITE(100, l, 1);
        DMEM_WRITE(101, l, 2);
        DMEM_WRITE(102, l, 3);
        DMEM_WRITE(103, l, 4);
        DMEM_WRITE(104, l, 5);
        DMEM_WRITE(105, l, 6);
        DMEM_WRITE(106, l, 7);
        DMEM_WRITE(107, l, 8);
        DMEM_WRITE(90, l, 0);
    }

    uint32_t prog[] = {
        ADDI(1, 0, 0),          // [0] r1 = 0 (sum)
        ADDI(2, 0, 0),          // [1] r2 = 0 (i)
        ADDI(3, 0, 4),          // [2] r3 = 4
        ADDI(4, 0, 100),        // [3] r4 = 100 (A base)
        ADDI(5, 0, 104),        // [4] r5 = 104 (B base)
        LDR(6, 4, 0),           // [5] r6 = A[i]  (loop start)
        LDR(6, 4, 0),           // [6] (repeat for pipeline)
        LDR(7, 5, 0),           // [7] r7 = B[i]
        LDR(7, 5, 0),           // [8] (repeat)
        NOP,                    // [9]
        MUL(8, 6, 7),           // [10] r8 = A[i]*B[i]
        NOP,                    // [11]
        ADD(1, 1, 8),           // [12] sum += r8
        ADDI(4, 4, 1),          // [13] A_ptr++
        ADDI(5, 5, 1),          // [14] B_ptr++
        ADDI(2, 2, 1),          // [15] i++
        NOP,                    // [16]
        BNE(2, 3, (-12)&0x1FFF),// [17] if i!=4 goto [5]
        NOP,                    // [18] flush
        NOP,                    // [19]
        STR(1, 0, 90),          // [20] store result
    };
    upload_and_run(prog, sizeof(prog)/4, 30000);

    check("Dot product 1*5+2*6+3*7+4*8", DMEM_READ(90, 0), 70);
}

int main()
{
    Xil_DCacheDisable();

    xil_printf("\n\r");
    xil_printf("ZYNQ GPGPU Test\n\r");

    test_alu_basic();
    test_addi();
    test_ldr_str();
    test_setm();
    test_branch_loop();
    test_jmp();
    test_dot_product();

    xil_printf("Results: %d PASS, %d FAIL\n\r", test_pass, test_fail);

    return 0;
}
