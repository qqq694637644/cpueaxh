#pragma once
// Split from test/demo/framework.hpp: machine-code builder and emitter helpers.
// Included through test/framework/framework.hpp; keep include order there.

class CodeBuilder {
public:
    struct Label {
        std::size_t id;
    };

    Label make_label() {
        labels_.push_back(static_cast<std::size_t>(-1));
        return Label{ labels_.size() - 1 };
    }

    void mark(Label label) {
        labels_[label.id] = bytes_.size();
    }

    std::size_t offset() const {
        return bytes_.size();
    }

    std::size_t label_offset(Label label) const {
        return labels_[label.id];
    }

    void align(std::size_t alignment) {
        while ((bytes_.size() % alignment) != 0) {
            emit8(0x90);
        }
    }

    void emit8(std::uint8_t value) {
        bytes_.push_back(value);
    }

    void emit16(std::uint16_t value) {
        emit8(static_cast<std::uint8_t>(value & 0xFFu));
        emit8(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    }

    void emit32(std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            emit8(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
        }
    }

    void emit64(std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) {
            emit8(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
        }
    }

    void emit_bytes(const std::uint8_t* data, std::size_t size) {
        bytes_.insert(bytes_.end(), data, data + size);
    }

    void emit_modrm(std::uint8_t mod, std::uint8_t reg, std::uint8_t rm) {
        emit8(static_cast<std::uint8_t>((mod << 6) | ((reg & 7u) << 3) | (rm & 7u)));
    }

    void emit_sib(std::uint8_t scale, std::uint8_t index, std::uint8_t base) {
        emit8(static_cast<std::uint8_t>((scale << 6) | ((index & 7u) << 3) | (base & 7u)));
    }

    void emit_rex(bool w, std::uint8_t reg, std::uint8_t index, std::uint8_t rm) {
        std::uint8_t rex = 0x40u;
        if (w) rex |= 0x08u;
        if ((reg & 8u) != 0) rex |= 0x04u;
        if ((index & 8u) != 0) rex |= 0x02u;
        if ((rm & 8u) != 0) rex |= 0x01u;
        if (rex != 0x40u) {
            emit8(rex);
        }
    }

    void emit_vex3(bool w, std::uint8_t map_select, std::uint8_t vvvv, bool l, std::uint8_t pp, std::uint8_t reg, std::uint8_t index, std::uint8_t rm) {
        emit8(0xC4);
        emit8(static_cast<std::uint8_t>(((reg & 8u) == 0 ? 0x80u : 0x00u) |
                                        ((index & 8u) == 0 ? 0x40u : 0x00u) |
                                        ((rm & 8u) == 0 ? 0x20u : 0x00u) |
                                        (map_select & 0x1Fu)));
        emit8(static_cast<std::uint8_t>((w ? 0x80u : 0x00u) |
                                        (((~vvvv) & 0x0Fu) << 3) |
                                        (l ? 0x04u : 0x00u) |
                                        (pp & 0x03u)));
    }

    void rip_rel32(Label label) {
        patches_.push_back(Patch{ label.id, bytes_.size(), 4, 0 });
        emit32(0);
    }

    void rip_rel32(Label label, std::size_t trailing_size) {
        patches_.push_back(Patch{ label.id, bytes_.size(), 4, trailing_size });
        emit32(0);
    }

    void rel32(Label label) {
        rip_rel32(label);
    }

    void rel8(Label label) {
        patches_.push_back(Patch{ label.id, bytes_.size(), 1, 0 });
        emit8(0);
    }

    void ret() {
        emit8(0xC3);
    }

    void nop1() {
        emit8(0x90);
    }

    void nop_modrm_reg() {
        emit8(0x0F);
        emit8(0x1F);
        emit_modrm(3, 0, static_cast<std::uint8_t>(Reg::RAX));
    }

    void pause() {
        emit8(0xF3);
        emit8(0x90);
    }

    void jmp32(Label label) {
        emit8(0xE9);
        rel32(label);
    }

    void call32(Label label) {
        emit8(0xE8);
        rel32(label);
    }

    void jcc32(std::uint8_t cc, Label label) {
        emit8(0x0F);
        emit8(static_cast<std::uint8_t>(0x80u + cc));
        rel32(label);
    }

    void loopcc(std::uint8_t opcode, Label label) {
        emit8(opcode);
        rel8(label);
    }

    void mov_reg_reg(Reg dst, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(src), 0, static_cast<std::uint8_t>(dst));
        emit8(0x89);
        emit_modrm(3, static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(dst));
    }

    void xchg_reg_reg(Reg left, Reg right) {
        emit_rex(true, static_cast<std::uint8_t>(right), 0, static_cast<std::uint8_t>(left));
        emit8(0x87);
        emit_modrm(3, static_cast<std::uint8_t>(right), static_cast<std::uint8_t>(left));
    }

    void xchg_rax_reg(Reg reg) {
        emit_rex(true, 0, 0, static_cast<std::uint8_t>(reg));
        emit8(static_cast<std::uint8_t>(0x90u + (static_cast<std::uint8_t>(reg) & 7u)));
    }

    void binary_reg_reg(BinaryOp op, Reg dst, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(src), 0, static_cast<std::uint8_t>(dst));
        switch (op) {
        case BinaryOp::Add: emit8(0x01); break;
        case BinaryOp::Adc: emit8(0x11); break;
        case BinaryOp::Sub: emit8(0x29); break;
        case BinaryOp::Sbb: emit8(0x19); break;
        case BinaryOp::And: emit8(0x21); break;
        case BinaryOp::Or: emit8(0x09); break;
        case BinaryOp::Xor: emit8(0x31); break;
        case BinaryOp::Cmp: emit8(0x39); break;
        case BinaryOp::Test: emit8(0x85); break;
        }
        emit_modrm(3, static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(dst));
    }

    void binary_reg_imm(BinaryOp op, Reg dst, std::uint32_t imm) {
        std::uint8_t reg_field = 0;
        std::uint8_t opcode = 0x81;
        if (op == BinaryOp::Test) {
            opcode = 0xF7;
            reg_field = 0;
        }
        else {
            switch (op) {
            case BinaryOp::Add: reg_field = 0; break;
            case BinaryOp::Or: reg_field = 1; break;
            case BinaryOp::Adc: reg_field = 2; break;
            case BinaryOp::Sbb: reg_field = 3; break;
            case BinaryOp::And: reg_field = 4; break;
            case BinaryOp::Sub: reg_field = 5; break;
            case BinaryOp::Xor: reg_field = 6; break;
            case BinaryOp::Cmp: reg_field = 7; break;
            case BinaryOp::Test: reg_field = 0; break;
            }
        }
        emit_rex(true, reg_field, 0, static_cast<std::uint8_t>(dst));
        emit8(opcode);
        emit_modrm(3, reg_field, static_cast<std::uint8_t>(dst));
        emit32(imm);
    }

    void unary_reg(UnaryOp op, Reg reg) {
        std::uint8_t reg_field = 0;
        std::uint8_t opcode = 0xFF;
        switch (op) {
        case UnaryOp::Inc: reg_field = 0; opcode = 0xFF; break;
        case UnaryOp::Dec: reg_field = 1; opcode = 0xFF; break;
        case UnaryOp::Not: reg_field = 2; opcode = 0xF7; break;
        case UnaryOp::Neg: reg_field = 3; opcode = 0xF7; break;
        case UnaryOp::Mul: reg_field = 4; opcode = 0xF7; break;
        case UnaryOp::Imul: reg_field = 5; opcode = 0xF7; break;
        }
        emit_rex(true, reg_field, 0, static_cast<std::uint8_t>(reg));
        emit8(opcode);
        emit_modrm(3, reg_field, static_cast<std::uint8_t>(reg));
    }

    void shift_imm(ShiftOp op, Reg reg, std::uint8_t amount) {
        std::uint8_t reg_field = 0;
        switch (op) {
        case ShiftOp::Rol: reg_field = 0; break;
        case ShiftOp::Ror: reg_field = 1; break;
        case ShiftOp::Rcl: reg_field = 2; break;
        case ShiftOp::Rcr: reg_field = 3; break;
        case ShiftOp::Shl: reg_field = 4; break;
        case ShiftOp::Shr: reg_field = 5; break;
        case ShiftOp::Sar: reg_field = 7; break;
        }
        emit_rex(true, reg_field, 0, static_cast<std::uint8_t>(reg));
        emit8(0xC1);
        emit_modrm(3, reg_field, static_cast<std::uint8_t>(reg));
        emit8(amount);
    }

    void shift_cl(ShiftOp op, Reg reg) {
        std::uint8_t reg_field = 0;
        switch (op) {
        case ShiftOp::Rol: reg_field = 0; break;
        case ShiftOp::Ror: reg_field = 1; break;
        case ShiftOp::Rcl: reg_field = 2; break;
        case ShiftOp::Rcr: reg_field = 3; break;
        case ShiftOp::Shl: reg_field = 4; break;
        case ShiftOp::Shr: reg_field = 5; break;
        case ShiftOp::Sar: reg_field = 7; break;
        }
        emit_rex(true, reg_field, 0, static_cast<std::uint8_t>(reg));
        emit8(0xD3);
        emit_modrm(3, reg_field, static_cast<std::uint8_t>(reg));
    }

    void bt_reg_imm(Reg reg, std::uint8_t bit) {
        emit_rex(true, 4, 0, static_cast<std::uint8_t>(reg));
        emit8(0x0F);
        emit8(0xBA);
        emit_modrm(3, 4, static_cast<std::uint8_t>(reg));
        emit8(bit);
    }

    void bit_reg_imm(std::uint8_t group, Reg reg, std::uint8_t bit) {
        emit_rex(true, group, 0, static_cast<std::uint8_t>(reg));
        emit8(0x0F);
        emit8(0xBA);
        emit_modrm(3, group, static_cast<std::uint8_t>(reg));
        emit8(bit);
    }

    void bit_mem_imm(std::uint8_t group, Label label, std::uint8_t bit) {
        emit_rex(true, group, 0, 5);
        emit8(0x0F);
        emit8(0xBA);
        emit_modrm(0, group, 5);
        rip_rel32(label);
        emit8(bit);
    }

    void bt_reg_reg(Reg dst, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(src), 0, static_cast<std::uint8_t>(dst));
        emit8(0x0F);
        emit8(0xA3);
        emit_modrm(3, static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(dst));
    }

    void bit_reg_reg(std::uint8_t opcode, Reg dst, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(src), 0, static_cast<std::uint8_t>(dst));
        emit8(0x0F);
        emit8(opcode);
        emit_modrm(3, static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(dst));
    }

    void bsf(Reg dst, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0xBC);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void bsr(Reg dst, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0xBD);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void popcnt(Reg dst, Reg src) {
        emit8(0xF3);
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0xB8);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void tzcnt(Reg dst, Reg src) {
        emit8(0xF3);
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0xBC);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void lzcnt(Reg dst, Reg src) {
        emit8(0xF3);
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0xBD);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void crc32_case(const Crc32CaseSpec& spec, Label memory_label) {
        if (spec.source_bits == 16 && spec.f2_before_66) {
            emit8(0xF2);
            emit8(0x66);
        }
        else {
            if (spec.source_bits == 16) {
                emit8(0x66);
            }
            emit8(0xF2);
        }
        emit_rex(spec.rex_w, static_cast<std::uint8_t>(spec.dst), 0,
            spec.memory_source ? 5 : static_cast<std::uint8_t>(spec.src));
        emit8(0x0F);
        emit8(0x38);
        emit8(spec.source_bits == 8 ? 0xF0 : 0xF1);
        emit_modrm(spec.memory_source ? 0 : 3,
            static_cast<std::uint8_t>(spec.dst),
            spec.memory_source ? 5 : static_cast<std::uint8_t>(spec.src));
        if (spec.memory_source) {
            rip_rel32(memory_label);
        }
    }
    void bswap(Reg reg) {
        emit_rex(true, 0, 0, static_cast<std::uint8_t>(reg));
        emit8(0x0F);
        emit8(static_cast<std::uint8_t>(0xC8u + (static_cast<std::uint8_t>(reg) & 7u)));
    }

    void setcc(std::uint8_t cc, Reg reg) {
        emit_rex(false, 0, 0, static_cast<std::uint8_t>(reg));
        emit8(0x0F);
        emit8(static_cast<std::uint8_t>(0x90u + cc));
        emit_modrm(3, 0, static_cast<std::uint8_t>(reg));
    }

    void cmovcc(std::uint8_t cc, Reg dst, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(static_cast<std::uint8_t>(0x40u + cc));
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void movzx_byte(Reg dst, Label label) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0xB6);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movzx_word(Reg dst, Label label) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0xB7);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movsx_byte(Reg dst, Label label) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0xBE);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movsx_word(Reg dst, Label label) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0xBF);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movsxd_dword(Reg dst, Label label) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x63);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void lea_scaled(Reg dst, Reg base, Reg index, std::uint8_t scale, std::int8_t disp) {
        emit_rex(true, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(base));
        emit8(0x8D);
        emit_modrm(1, static_cast<std::uint8_t>(dst), 4);
        emit_sib(scale, static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(base));
        emit8(static_cast<std::uint8_t>(disp));
    }

    void lea_rip(Reg dst, Label label) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x8D);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void mov_mem_reg(Label label, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(src), 0, 5);
        emit8(0x89);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void mov_reg_mem(Reg dst, Label label) {
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x8B);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movbe_case(const MovbeCaseSpec& spec, Label load_label, Label store_label) {
        if (spec.operand_bits == 16) {
            emit8(0x66);
        }
        emit_rex(spec.operand_bits == 64, static_cast<std::uint8_t>(spec.reg), 0, 5);
        emit8(0x0F);
        emit8(0x38);
        emit8(spec.store_to_memory ? 0xF1 : 0xF0);
        emit_modrm(0, static_cast<std::uint8_t>(spec.reg), 5);
        rip_rel32(spec.store_to_memory ? store_label : load_label);
    }
    void binary_mem_reg(BinaryOp op, Label label, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(src), 0, 5);
        switch (op) {
        case BinaryOp::Add: emit8(0x01); break;
        case BinaryOp::Sub: emit8(0x29); break;
        case BinaryOp::And: emit8(0x21); break;
        case BinaryOp::Or: emit8(0x09); break;
        case BinaryOp::Xor: emit8(0x31); break;
        default: emit8(0x01); break;
        }
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void xadd_mem_reg(Label label, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(src), 0, 5);
        emit8(0x0F);
        emit8(0xC1);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void cmpxchg_mem_reg(Label label, Reg src) {
        emit_rex(true, static_cast<std::uint8_t>(src), 0, 5);
        emit8(0x0F);
        emit8(0xB1);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void push_reg(Reg reg) {
        emit_rex(false, 0, 0, static_cast<std::uint8_t>(reg));
        emit8(static_cast<std::uint8_t>(0x50u + (static_cast<std::uint8_t>(reg) & 7u)));
    }

    void push_imm8(std::uint8_t imm) {
        emit8(0x6A);
        emit8(imm);
    }

    void push_imm32(std::uint32_t imm) {
        emit8(0x68);
        emit32(imm);
    }

    void pop_reg(Reg reg) {
        emit_rex(false, 0, 0, static_cast<std::uint8_t>(reg));
        emit8(static_cast<std::uint8_t>(0x58u + (static_cast<std::uint8_t>(reg) & 7u)));
    }

    void pushf() {
        emit8(0x9C);
    }

    void cmc() {
        emit8(0xF5);
    }

    void clc() {
        emit8(0xF8);
    }

    void stc() {
        emit8(0xF9);
    }

    void cld() {
        emit8(0xFC);
    }

    void std_() {
        emit8(0xFD);
    }

    void lahf() {
        emit8(0x9F);
    }

    void sahf() {
        emit8(0x9E);
    }

    void enter(std::uint16_t size) {
        emit8(0xC8);
        emit16(size);
        emit8(0x00);
    }

    void leave() {
        emit8(0xC9);
    }

    void cbw() {
        emit8(0x66);
        emit8(0x98);
    }

    void cwde() {
        emit8(0x98);
    }

    void cdqe() {
        emit8(0x48);
        emit8(0x98);
    }

    void cwd() {
        emit8(0x66);
        emit8(0x99);
    }

    void cdq() {
        emit8(0x99);
    }

    void cqo() {
        emit8(0x48);
        emit8(0x99);
    }

    void mov_r32_imm(Reg reg, std::uint32_t imm) {
        emit_rex(false, 0, 0, static_cast<std::uint8_t>(reg));
        emit8(static_cast<std::uint8_t>(0xB8u + (static_cast<std::uint8_t>(reg) & 7u)));
        emit32(imm);
    }

    void mov_r8_imm(Reg reg, std::uint8_t imm) {
        emit_rex(false, 0, 0, static_cast<std::uint8_t>(reg));
        emit8(static_cast<std::uint8_t>(0xB0u + (static_cast<std::uint8_t>(reg) & 7u)));
        emit8(imm);
    }

    void rep() {
        emit8(0xF3);
    }

    void movsb() { emit8(0xA4); }
    void movsw() { emit8(0x66); emit8(0xA5); }
    void movsd() { emit8(0xA5); }
    void movsq() { emit8(0x48); emit8(0xA5); }
    void lodsb() { emit8(0xAC); }
    void lodsq() { emit8(0x48); emit8(0xAD); }
    void stosb() { emit8(0xAA); }
    void stosq() { emit8(0x48); emit8(0xAB); }
    void cmpsb() { emit8(0xA6); }
    void scasb() { emit8(0xAE); }
    void xlat() { emit8(0xD7); }

    void movdqu_load(Reg dst, Label label) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0xF3);
        emit8(0x0F);
        emit8(0x6F);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movdqu_store(Label label, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(src), 0, 5);
        emit8(0xF3);
        emit8(0x0F);
        emit8(0x7F);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void vmovups_ymm_load(Reg dst, Label label) {
        emit_vex3(false, 0x01, 0x00, true, 0x00, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x10);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void vmovups_ymm_store(Label label, Reg src) {
        emit_vex3(false, 0x01, 0x00, true, 0x00, static_cast<std::uint8_t>(src), 0, 5);
        emit8(0x11);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void sha256rnds2(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0x38);
        emit8(0xCB);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void sha256rnds2_mem(Reg dst, Label label) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0x38);
        emit8(0xCB);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void sha256msg1(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0x38);
        emit8(0xCC);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void sha256msg1_mem(Reg dst, Label label) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0x38);
        emit8(0xCC);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void sha256msg2(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0x38);
        emit8(0xCD);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void sha256msg2_mem(Reg dst, Label label) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0x38);
        emit8(0xCD);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void pblendw(Reg dst, Reg src, std::uint8_t imm) {
        emit8(0x66);
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0x3A);
        emit8(0x0E);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
        emit8(imm);
    }

    void pblendw_mem(Reg dst, Label label, std::uint8_t imm) {
        emit8(0x66);
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0x3A);
        emit8(0x0E);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label, 1);
        emit8(imm);
    }

    void vpblendw_xmm(Reg dst, Reg src1, Reg src2, std::uint8_t imm) {
        emit_vex3(false, 0x03, static_cast<std::uint8_t>(src1), false, 0x01, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src2));
        emit8(0x0E);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src2));
        emit8(imm);
    }

    void vpblendw_xmm_mem(Reg dst, Reg src1, Label label, std::uint8_t imm) {
        emit_vex3(false, 0x03, static_cast<std::uint8_t>(src1), false, 0x01, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0E);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label, 1);
        emit8(imm);
    }

    void vpblendw_ymm(Reg dst, Reg src1, Reg src2, std::uint8_t imm) {
        emit_vex3(false, 0x03, static_cast<std::uint8_t>(src1), true, 0x01, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src2));
        emit8(0x0E);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src2));
        emit8(imm);
    }

    void vpblendw_ymm_mem(Reg dst, Reg src1, Label label, std::uint8_t imm) {
        emit_vex3(false, 0x03, static_cast<std::uint8_t>(src1), true, 0x01, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0E);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label, 1);
        emit8(imm);
    }

    void vinsertf128(Reg dst, Reg src1, Reg src2, std::uint8_t imm) {
        emit_vex3(false, 0x03, static_cast<std::uint8_t>(src1), true, 0x01, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src2));
        emit8(0x18);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src2));
        emit8(imm);
    }

    void vinsertf128_mem(Reg dst, Reg src1, Label label, std::uint8_t imm) {
        emit_vex3(false, 0x03, static_cast<std::uint8_t>(src1), true, 0x01, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x18);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label, 1);
        emit8(imm);
    }

    void vfmadd132sd(Reg dst, Reg src2, Reg src3) {
        emit_vex3(true, 0x02, static_cast<std::uint8_t>(src2), false, 0x01, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src3));
        emit8(0x99);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src3));
    }

    void vfmadd132sd_mem(Reg dst, Reg src2, Label label) {
        emit_vex3(true, 0x02, static_cast<std::uint8_t>(src2), false, 0x01, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x99);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void vfmadd213sd(Reg dst, Reg src2, Reg src3) {
        emit_vex3(true, 0x02, static_cast<std::uint8_t>(src2), false, 0x01, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src3));
        emit8(0xA9);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src3));
    }

    void vfmadd213sd_mem(Reg dst, Reg src2, Label label) {
        emit_vex3(true, 0x02, static_cast<std::uint8_t>(src2), false, 0x01, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0xA9);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void vfmadd231sd(Reg dst, Reg src2, Reg src3) {
        emit_vex3(true, 0x02, static_cast<std::uint8_t>(src2), false, 0x01, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src3));
        emit8(0xB9);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src3));
    }

    void vfmadd231sd_mem(Reg dst, Reg src2, Label label) {
        emit_vex3(true, 0x02, static_cast<std::uint8_t>(src2), false, 0x01, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0xB9);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void paddq(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0xD4);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void pxor(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0xEF);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void pand(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0xDB);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void por(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0xEB);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void movd_mm_reg(Reg dst, Reg src) {
        emit_rex(false, 0, 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void movq_mm_reg(Reg dst, Reg src) {
        emit_rex(true, 0, 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void movd_mm_mem(Reg dst, Label label) {
        emit_rex(false, 0, 0, 5);
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movq_mm_mem(Reg dst, Label label) {
        emit_rex(true, 0, 0, 5);
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movd_mem_mm(Label label, Reg src) {
        emit_rex(false, 0, 0, 5);
        emit8(0x0F);
        emit8(0x7E);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void movq_mem_mm(Label label, Reg src) {
        emit_rex(true, 0, 0, 5);
        emit8(0x0F);
        emit8(0x7E);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void movd_xmm_reg(Reg dst, Reg src) {
        emit8(0x66);
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void movq_xmm_reg(Reg dst, Reg src) {
        emit8(0x66);
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void movd_xmm_mem(Reg dst, Label label) {
        emit8(0x66);
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movq_xmm_mem(Reg dst, Label label) {
        emit8(0x66);
        emit_rex(true, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label);
    }

    void movd_mem_xmm(Label label, Reg src) {
        emit8(0x66);
        emit_rex(false, static_cast<std::uint8_t>(src), 0, 5);
        emit8(0x0F);
        emit8(0x7E);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void movq_mem_xmm(Label label, Reg src) {
        emit8(0x66);
        emit_rex(true, static_cast<std::uint8_t>(src), 0, 5);
        emit8(0x0F);
        emit8(0x7E);
        emit_modrm(0, static_cast<std::uint8_t>(src), 5);
        rip_rel32(label);
    }

    void punpcklbw(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x60);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void punpcklwd(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x61);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void punpckldq(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x62);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void punpcklqdq(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x6C);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void pcmpeqb(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x74);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void pcmpeqw(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x75);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void pcmpeqd(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x76);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void pshufd(Reg dst, Reg src, std::uint8_t imm) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x70);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
        emit8(imm);
    }

    void pshufd_mem(Reg dst, Label label, std::uint8_t imm) {
        emit8(0x66);
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, 5);
        emit8(0x0F);
        emit8(0x70);
        emit_modrm(0, static_cast<std::uint8_t>(dst), 5);
        rip_rel32(label, 1);
        emit8(imm);
    }

    void pslldq(Reg reg, std::uint8_t imm) {
        emit_rex(false, 7, 0, static_cast<std::uint8_t>(reg));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x73);
        emit_modrm(3, 7, static_cast<std::uint8_t>(reg));
        emit8(imm);
    }

    void psrldq(Reg reg, std::uint8_t imm) {
        emit_rex(false, 3, 0, static_cast<std::uint8_t>(reg));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x73);
        emit_modrm(3, 3, static_cast<std::uint8_t>(reg));
        emit8(imm);
    }

    void pshufb(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x38);
        emit8(0x00);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void aesenc(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x38);
        emit8(0xDC);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void aesenclast(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x38);
        emit8(0xDD);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void aesdec(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x38);
        emit8(0xDE);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void aesdeclast(Reg dst, Reg src) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x38);
        emit8(0xDF);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
    }

    void aeskeygenassist(Reg dst, Reg src, std::uint8_t imm) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x3A);
        emit8(0xDF);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
        emit8(imm);
    }

    void roundsd(Reg dst, Reg src, std::uint8_t imm) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x3A);
        emit8(0x0B);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
        emit8(imm);
    }

    void roundss(Reg dst, Reg src, std::uint8_t imm) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x3A);
        emit8(0x0A);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
        emit8(imm);
    }

    void pcmpestri(Reg dst, Reg src, std::uint8_t imm) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x3A);
        emit8(0x61);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
        emit8(imm);
    }

    void pcmpestrm(Reg dst, Reg src, std::uint8_t imm) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x3A);
        emit8(0x60);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
        emit8(imm);
    }

    void pcmpistrm(Reg dst, Reg src, std::uint8_t imm) {
        emit_rex(false, static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        emit8(0x66);
        emit8(0x0F);
        emit8(0x3A);
        emit8(0x62);
        emit_modrm(3, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src));
        emit8(imm);
    }

    bool finalize() {
        for (const Patch& patch : patches_) {
            const std::size_t target = labels_[patch.label_id];
            if (target == static_cast<std::size_t>(-1)) {
                return false;
            }
            const std::int64_t next = static_cast<std::int64_t>(patch.offset + patch.size + patch.trailing_size);
            const std::int64_t disp = static_cast<std::int64_t>(target) - next;
            if (patch.size == 4) {
                const std::int32_t value = static_cast<std::int32_t>(disp);
                for (int shift = 0; shift < 32; shift += 8) {
                    bytes_[patch.offset + shift / 8] = static_cast<std::uint8_t>((value >> shift) & 0xFF);
                }
            }
            else {
                const std::int8_t value = static_cast<std::int8_t>(disp);
                bytes_[patch.offset] = static_cast<std::uint8_t>(value);
            }
        }
        return true;
    }

    const std::vector<std::uint8_t>& bytes() const {
        return bytes_;
    }

private:
    struct Patch {
        std::size_t label_id;
        std::size_t offset;
        std::size_t size;
        std::size_t trailing_size;
    };

    std::vector<std::uint8_t> bytes_;
    std::vector<std::size_t> labels_;
    std::vector<Patch> patches_;
};

