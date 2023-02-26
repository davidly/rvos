/*
    This is a simplistic 64-bit RISC-V emulator.
    Only physical memory is supported.
    Only a subset of instructions are implemented (enough to to run my test apps).
    Compressed instructions aren't supported.
    No floating point instructions are implemented.
    I tested with a variety of C apps compiled with g++ (using C runtime functions that don't call into the OS)
    I also tested with the BASIC test suite for my compiler BA, which targets risc-v.
    It's slightly faster than the 400Mhz K210 processor on my AMD 5950x machine.

    Written by David Lee in February 2023

    Useful: https://luplab.gitlab.io/rvcodecjs/#q=c00029f3&abi=false&isa=AUTO
            https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-cc.adoc#abi-lp64d
            https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#:~:text=In%20computing%2C%20the%20Executable%20and,shared%20libraries%2C%20and%20core%20dumps.
            https://jemu.oscc.cc/AUIPC
            https://inst.eecs.berkeley.edu/~cs61c/resources/su18_lec/Lecture7.pdf
*/

#include <stdint.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>

#include <djltrace.hxx>

#include "riscv.hxx"

const byte IllType = 0;
const byte UType = 1;
const byte JType = 2;
const byte IType = 3;
const byte BType = 4;
const byte SType = 5;
const byte RType = 6;
const byte CsrType = 7;
const byte R4Type = 8;
const byte ShiftType = 9;

static uint32_t g_State = 0;

const uint32_t stateTraceInstructions = 1;
const uint32_t stateEndEmulation = 2;

void RiscV::trace_instructions( bool t ) { if ( t ) g_State |= stateTraceInstructions; else g_State &= ~stateTraceInstructions; }
void RiscV::end_emulation() { g_State |= stateEndEmulation; }

char * append_hex_nibble( char * p, uint8_t val )
{
    assert( val <= 15 );
    *p++ = ( val <= 9 ) ? val + '0' : val - 10 + 'a';
    return p;
} //append_hex_nibble

char * append_hex_byte( char * p, uint8_t val )
{
    p = append_hex_nibble( p, ( val >> 4 ) & 0xf );
    p = append_hex_nibble( p, val & 0xf );
    return p;
} //append_hex_byte

char * append_hex_word( char * p, uint16_t val )
{
    p = append_hex_byte( p, ( val >> 8 ) & 0xff );
    p = append_hex_byte( p, val & 0xff );
    return p;
} //append_hex_word

void DumpBinaryData( uint8_t * pData, uint32_t length, uint32_t indent )
{
    __int64 offset = 0;
    __int64 beyond = length;
    const __int64 bytesPerRow = 32;
    uint8_t buf[ bytesPerRow ];
    char acLine[ 200 ];

    while ( offset < beyond )
    {
        char * pline = acLine;

        for ( uint32_t i = 0; i < indent; i++ )
            *pline++ = ' ';

        pline = append_hex_word( pline, (uint16_t) offset );
        *pline++ = ' ';
        *pline++ = ' ';

        __int64 cap = __min( offset + bytesPerRow, beyond );
        __int64 toread = ( ( offset + bytesPerRow ) > beyond ) ? ( length % bytesPerRow ) : bytesPerRow;

        memcpy( buf, pData + offset, toread );

        for ( __int64 o = offset; o < cap; o++ )
        {
            pline = append_hex_byte( pline, buf[ o - offset ] );
            *pline++ = ' ';
        }

        uint64_t spaceNeeded = ( bytesPerRow - ( cap - offset ) ) * 3;

        for ( uint64_t sp = 0; sp < ( 1 + spaceNeeded ); sp++ )
            *pline++ = ' ';

        for ( __int64 o = offset; o < cap; o++ )
        {
            char ch = buf[ o - offset ];

            if ( ch < ' ' || 127 == ch )
                ch = '.';

            *pline++ = ch;
        }

        offset += bytesPerRow;
        *pline = 0;

        tracer.TraceQuiet( "%s\n", acLine );
    }
} //DumpBinaryData

const char * instruction_types[] =
{
    "!",
    "U",
    "J",
    "I",
    "B",
    "S",
    "R",
    "C",
    "r",
    "s",
};

struct RiscvInstruction
{
    char name[ 15 ];
    byte type;
};

// for the 32 opcodes ( ( op >> 2 ) & 0x1f )
byte riscv_types[ 32 ] =
{
    IType,   //  0
    IllType, //  1
    IllType, //  2
    IllType, //  3
    IType,   //  4
    UType,   //  5
    IType,   //  6
    IllType, //  7
    SType,   //  8
    IllType, //  9
    IllType, //  a
    IllType, //  b
    RType,   //  c
    UType,   //  d
    RType,   //  e
    IllType, //  f
    IllType, // 10
    IllType, // 11
    IllType, // 12
    IllType, // 13
    IllType, // 14
    IllType, // 15
    IllType, // 16
    IllType, // 17
    BType,   // 18
    IType,   // 19
    IllType, // 1a
    JType,   // 1b
    IType,   // 1c
    IllType, // 1d
    IllType, // 1e
    IllType, // 1f
};

void RiscV::assert_type( byte t ) { assert( t == riscv_types[ opcode_type ] ); }

const char * register_names[ 32 ] =
{
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

const char * RiscV::reg_name( uint64_t reg )
{
    if ( reg >= 32 )
        return "invalid register";

    return register_names[ reg ];
} //reg_name

uint32_t uncompress_rvc( uint16_t x )
{
    return 0;
} //uncompress_rvc

void RiscV::trace_state( uint64_t pcnext )
{
    byte optype = riscv_types[ opcode_type ];

//    DumpBinaryData( getmem( regs[ sp ] ), 256, 2 );
//    tracer.Trace( "t0 %8llx t1 %8llx t2 %8llx\n", regs[ t0 ], regs[ t1 ], regs[ t2 ] );
    tracer.Trace( "pc %8llx op %08llx a0 %08llx a1 %08llx a2 %08llx a5 %08llx ra %08llx sp %08llx opt %2llx %s => ",
                  pc, op, regs[ a0 ], regs[ a1 ], regs[ a2 ], regs[ a5 ], regs[ ra ], regs[ sp ], opcode_type, instruction_types[ optype ] );

    switch ( optype )
    {
        case IllType:
        {
            tracer.Trace( "illegal optype!\n" );
            break;
        }
        case UType:
        {
            decode_U();
            if ( 0x5 == opcode_type )
                tracer.Trace( "auipc   %s, %lld  # %llx\n", reg_name( rd ), (u_imm << 12 ), pc + ( u_imm << 12 ) );
            else if ( 0xd == opcode_type )
                tracer.Trace( "lui     %s, %lld  # %llx\n", reg_name( rd ), u_imm, u_imm << 12 );
            break;
        }
        case JType:
        {
            decode_J();
            if ( 0x1b == opcode_type )
                tracer.Trace( "jal     %lld # %8llx\n", j_imm_u, pc + j_imm_u );
            break;
        }
        case IType:
        {
            decode_I();
            //tracer.Trace( "\ni_imm %llx, i_imm_u %llx, rs1 %s, funct3 %llx, rd %s\n", i_imm, i_imm_u, reg_name( rs1 ), funct3, reg_name( rd ) );
            if ( 0x0 == opcode_type )
            {
                switch( funct3 )
                {
                    case 0: tracer.Trace( "lb      %s, %lld(%s)\n", reg_name( rd ), i_imm, reg_name( rs1 ) ); break;
                    case 1: tracer.Trace( "lh      %s, %lld(%s)\n", reg_name( rd ), i_imm, reg_name( rs1 ) ); break;
                    case 2: tracer.Trace( "lw      %s, %lld(%s)\n", reg_name( rd ), i_imm, reg_name( rs1 ) ); break;
                    case 3: tracer.Trace( "ld      %s, %lld(%s)  # %lld(%llx)\n", reg_name( rd ), i_imm, reg_name( rs1 ), i_imm, regs[ rs1 ] ); break;
                    case 4: tracer.Trace( "lbu     %s, %lld(%s)\n", reg_name( rd ), i_imm, reg_name( rs1 ) ); break;
                    case 5: tracer.Trace( "lhu     %s, %lld(%s)\n", reg_name( rd ), i_imm, reg_name( rs1 ) ); break;
                    case 6: tracer.Trace( "lwu     %s, %lld(%s)\n", reg_name( rd ), i_imm, reg_name( rs1 ) ); break;
                }
            }
            else if ( 0x4 == opcode_type )
            {
                decode_I_shift();

                switch( funct3 )
                {
                    case 0: tracer.Trace( "addi    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm ); break;
                    case 1: tracer.Trace( "slli    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_shamt6 ); break;
                    case 3: tracer.Trace( "sltiu   %s, %s, %llu\n", reg_name( rd ), reg_name( rs1 ), i_imm ); break;
                    case 4: tracer.Trace( "xori    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm ); break;
                    case 5:
                    {
                        if ( 0 == i_top2 )
                            tracer.Trace( "srli    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_shamt6 );
                        else if ( 1 == i_top2 )
                            tracer.Trace( "srai    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_shamt5 );
                        break;
                    }
                    case 6: tracer.Trace( "ori     %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm ); break;
                    case 7: tracer.Trace( "andi    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm ); break;
                }
            }
            else if ( 0x6 == opcode_type )
            {
                decode_I_shift();

                switch( funct3 )
                {
                    case 0: tracer.Trace( "addiw   %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm  ); break;
                    case 1:
                    {
                        if ( 0 == i_top2 )
                            tracer.Trace( "slliw   %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_shamt6 );
                        break;
                    }
                    case 5:
                    {
                        switch( i_top2 )
                        {
                            case 0: tracer.Trace( "srliw   %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_shamt5  ); break;
                            case 1: tracer.Trace( "sraiw   %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_shamt5  ); break;
                        }
                        break;
                    }
                }
            }
            else if ( 0x19 == opcode_type )
            {
                switch( funct3 )
                {
                    case 0: tracer.Trace( "jalr    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm ); break;
                }
            }
            else if ( 0x1c == opcode_type )
            {
                if ( 0x73 == op )
                    tracer.Trace( "ecall\n" );
                else
                {
                    switch( funct3 )
                    {
                        case 0x2:
                        {
                            if ( 0xc00 == i_imm_u )
                                tracer.Trace( "csrss   %s, cycle, %s\n", reg_name( rd ), reg_name( rs1 ) );
                            break;
                        }
                    }
                }
            }
            break;
        }
        case BType:
        {
            decode_B();
            if ( 0x18 == opcode_type )
            {
                if ( 0 == funct3 )
                    tracer.Trace( "beq     %s, %s, %lld  # %8llx\n", reg_name( rs1 ), reg_name( rs2 ), b_imm, pc + b_imm );
                else if ( 1 == funct3 )
                    tracer.Trace( "bne     %s, %s, %lld  # %8llx\n", reg_name( rs1 ), reg_name( rs2 ), b_imm, pc + b_imm );
                else if ( 4 == funct3 )
                    tracer.Trace( "blt     %s, %s, %lld  # %8llx\n", reg_name( rs1 ), reg_name( rs2 ), b_imm, pc + b_imm );
                else if ( 5 == funct3 )
                    tracer.Trace( "bge     %s, %s, %lld  # %8llx\n", reg_name( rs1 ), reg_name( rs2 ), b_imm, pc + b_imm );
                else if ( 6 == funct3 )
                    tracer.Trace( "bltu    %s, %s, %lld  # %8llx\n", reg_name( rs1 ), reg_name( rs2 ), b_imm, pc + b_imm );
                else if ( 7 == funct3 )
                    tracer.Trace( "bgeu    %s, %s, %lld  # %8llx\n", reg_name( rs1 ), reg_name( rs2 ), b_imm, pc + b_imm );
            }
            break;
        }
        case SType:
        {
            decode_S();
            //tracer.Trace( "s_imm %llx, rs2 %s, rs1 %s, funct3 %llx\n", s_imm, reg_name( rs2 ), reg_name( rs1 ), funct3 );

            if ( 8 == opcode_type )
            {
                switch ( funct3 )
                {
                    case 0: tracer.Trace( "sb      %s, %lld(%s)  #  %2x, %lld(%llx)\n", reg_name( rs2 ), s_imm, reg_name( rs1 ), (uint8_t) regs[ rs2 ], s_imm, regs[ rs1 ] ); break;
                    case 1: tracer.Trace( "sh      %s, %lld(%s)  #  %4x, %lld(%llx)\n", reg_name( rs2 ), s_imm, reg_name( rs1 ), (uint16_t) regs[ rs2 ], s_imm, regs[ rs1 ] ); break;
                    case 2: tracer.Trace( "sw      %s, %lld(%s)\n", reg_name( rs2 ), s_imm, reg_name( rs1 ) ); break;
                    case 3: tracer.Trace( "sd      %s, %lld(%s)  # %lld(%llx)\n", reg_name( rs2 ), s_imm, reg_name( rs1 ), s_imm, regs[ rs1 ] ); break;
                }
            }
            break;
        }
        case ShiftType:
        {
            break;
        }
        case RType:
        {
            decode_R();
            //tracer.Trace( "\nfunct7 %llx, rs2 %llx, rs1 %llx, funct3 %llx, rd %llx\n", funct7, rs2, rs1, funct3, rd );

            if ( 0x0c == opcode_type )
            {
                if ( 0 == funct7 )
                {
                    if ( 0 == funct3  )
                        tracer.Trace( "add     %s, %s, %s # %llx + %llx\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ] );
                    else if ( 1 == funct3  )
                        tracer.Trace( "sll     %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 2 == funct3 )
                        tracer.Trace( "slt     %s, %s, %s # %d == %lld < %lld\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ),
                                      (int64_t) regs[rs1] < (int64_t) regs[rs2], regs[rs1], regs[rs2] );
                    else if ( 3 == funct3  )
                        tracer.Trace( "sltu    %s, %s, %s # %d == %llu < %llu\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ),
                                      regs[rs1] < regs[rs2], regs[rs1], regs[rs2] );
                    else if ( 4 == funct3  )
                        tracer.Trace( "xor     %s, %s, %s # %d == %llx\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[rs1] ^ regs[rs2] );
                    else if ( 5 == funct3  )
                        tracer.Trace( "srl     %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 6 == funct3 )
                        tracer.Trace( "or      %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 7 == funct3 )
                        tracer.Trace( "and     %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                 }
                 else if ( 1 == funct7 )
                 {
                    if ( 0 == funct3 )
                        tracer.Trace( "mul     %s, %s, %s # %llx + %llx\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ] );
                    else if ( 4 == funct3 )
                        tracer.Trace( "div     %s, %s, %s  # %lld / %lld\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ] );
                    else if ( 5 == funct3 )
                        tracer.Trace( "udiv    %s, %s, %s  # %llu / %llu\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ] );
                    else if ( 6 == funct3 )
                        tracer.Trace( "rem     %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 7 == funct3 )
                        tracer.Trace( "remu    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                }
                else if ( 0x20 == funct7 )
                {
                    if ( 0 == funct3 )
                        tracer.Trace( "sub     %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 5== funct3 )
                        tracer.Trace( "sra     %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                }
            }
            else if ( 0x0e == opcode_type )
            {
                if ( 0 == funct7 )
                {
                    if ( 0 == funct3 )
                        tracer.Trace( "addw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 1 == funct3 )
                        tracer.Trace( "sllw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 5 == funct3 )
                        tracer.Trace( "srlw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                }
                else if ( 1 == funct7 )
                {
                    if ( 0 == funct3 )
                        tracer.Trace( "mulw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 4 == funct3 )
                        tracer.Trace( "divw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 5 == funct3 )
                        tracer.Trace( "divuw   %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 6 == funct3 )
                        tracer.Trace( "remw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 7 == funct3 )
                        tracer.Trace( "remuw   %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                }
                else if ( 0x20 == funct7 )
                {
                    if ( 0 == funct3 )
                        tracer.Trace( "subw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                    else if ( 5 == funct3 )
                        tracer.Trace( "sraw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                }
            }
            break;
        }
        case CsrType:
        {
            break;
        }
        case R4Type:
        {
            break;
        }
        default:
        {
            break;
        }
    }
} //trace_state

__declspec(noinline) void RiscV::unhandled()
{
    printf( "unhandled op %llx optype %llx == %s\n", op, opcode_type, instruction_types[ riscv_types[ opcode_type ] ] );
    tracer.Trace( "unhandled op %llx optype %llx == %s\n", op, opcode_type, instruction_types[ riscv_types[ opcode_type ] ] );
    exit( 1 );
} //unhandled

#ifndef DEBUG
__inline_perf
#endif
bool RiscV::execute_instruction( uint64_t pcnext )
{
    switch( opcode_type )
    {
        case 0x0:
        {
            assert_type( IType );
            decode_I();
            if ( 0 == rd )
                break;

            if ( 0 == funct3 ) // lb rd, imm(rs1)
                regs[ rd ] = (int8_t) getui8( regs[ rs1 ] + i_imm ); // sign extend
            else if ( 1 == funct3 ) // lh rd, imm(rs1)
                regs[ rd ] = (int16_t) getui16( regs[ rs1 ] + i_imm ); // sign extend
            else if ( 2 == funct3 ) // lw    rd, imm(rs1)
                regs[ rd ] = (int32_t) getui32( regs[ rs1 ] + i_imm ); // sign extend
            else if ( 3 == funct3 ) // ld    rd, imm(rs1)
                regs[ rd ] = getui64( regs[ rs1 ] + i_imm );
            else if ( 4 == funct3 ) // lbu   rd, imm(rs1)
                regs[ rd ] = getui8( regs[ rs1 ] + i_imm );
            else if ( 5 == funct3 ) // lhu rd, imm(rs1)
                regs[ rd ] = getui16( regs[ rs1 ] + i_imm );
            else if ( 6 == funct3 ) // lwu rd, imm(rs1)
                regs[ rd ] = getui32( regs[ rs1 ] + i_imm );
            else
                unhandled();
            break;
        }
        case 0x4:
        {
            assert_type( IType );
            decode_I();
            if ( 0 == rd )
                break;

            if ( 0 == funct3 ) // addi rd, rs1, imm
                regs[ rd ] =  i_imm + regs[ rs1 ];
            else if ( 1 == funct3 ) // slli rd, rs1, imm
            {
                decode_I_shift();
                regs[ rd ] =  regs[ rs1 ] << i_shamt6;
            }
            else if ( 2 == funct3 ) // slti rd, rs1, imm
                regs[ rd ] =  ( (int64_t) regs[ rs1 ] < i_imm );
            else if ( 3 == funct3 ) // sltiu rd, rs1, imm
                regs[ rd ] =  regs[ rs1 ] < i_imm_u;
            else if ( 4 == funct3 ) // xori rd, rs1, imm
                regs[ rd ] =  i_imm ^ regs[ rs1 ];
            else if ( 5 == funct3 )
            {
                decode_I_shift();

                if ( 0 == i_top2 ) // srli rd, rs1, i_shamt
                    regs[ rd ] = ( regs[ rs1 ] >> i_shamt6 );
                else if ( 1 == i_top2 ) // srai rd, rs1, i_shamt
                    regs[ rd ] = ( ( (int64_t) regs[ rs1 ] ) >> i_shamt5 );
                else
                    unhandled();
            }
            else if ( 6 == funct3 ) // ori rd, rs1, imm
                regs[ rd ] =  i_imm | regs[ rs1 ];
            else if ( 7 == funct3 ) // andi rd, rs1, imm
                regs[ rd ] =  i_imm & regs[ rs1 ];
            else
                unhandled();
            break;
        }
        case 0x5:
        {
            assert_type( UType );
            decode_U();
            if ( 0 == rd )
                break;

            // auipc imm.    rd <= pc + ( imm << 12 )

            regs[ rd ] = pc + ( u_imm << 12 );
            break;
        }
        case 0x6:
        {
            assert_type( IType );
            decode_I();
            decode_I_shift();
            if ( 0 == rd )
                break;

            if ( 0 == funct3 )
                regs[ rd ] = regs[ rs1 ] + i_imm;  // addiw rd, rs1, i_imm
            else if ( 1 == funct3 )
            {
                if ( 0 == i_top2 )
                    regs[ rd ] = ( regs[ rs1 ] << i_shamt6 );  // slliw rd, rs1, i_shamt6
                else
                    unhandled();
            }
            else if ( 5 == funct3 )
            {
                if ( 0 == i_top2 )
                    regs[ rd ] = ( ( 0xffffffff  & regs[ rs1 ] ) >> i_shamt5 ); // srliw rd, rs1, i_imm
                else if ( 1 == i_top2 )
                {
                    uint32_t t = regs[ rs1 ] & 0xffffffff;
                    regs[ rd ] = ( ( (int32_t) t ) >> i_shamt5 ); // sraiw rd, rs1, i_imm
                }
                else
                    unhandled();
            }
            else
                unhandled();
            break;
        }
        case 0x8:
        {
            assert_type( SType );
            decode_S();

            if ( 0 == funct3 ) // sb   rs2, imm(rs1)
                setui8( regs[ rs1 ] + s_imm, (uint8_t) regs[ rs2 ] );
            else if ( 1 == funct3 ) // sh   rs2, imm(rs1)
                setui16( regs[ rs1 ] + s_imm, (uint16_t) regs[ rs2 ] );
            else if ( 2 == funct3 ) // sw   rs2, imm(rs1)
                setui32( regs[ rs1 ] + s_imm, (uint32_t) regs[ rs2 ] );
            else if ( 3 == funct3 ) // sd   rs2, imm(rs1)
                setui64( regs[ rs1 ] + s_imm, regs[ rs2 ] );
            else
                unhandled();
            break;
        }
        case 0xc:
        {
            assert_type( RType );
            decode_R();

            if ( 0 == rd )
                break;

            if ( 0 == funct7 )
            {
                if ( 0 == funct3 )
                    regs[ rd ] = regs[ rs1 ] + regs[ rs2 ]; // add rd, rs1, rs2
                else if ( 1 == funct3 )
                    regs[ rd ] = ( regs[ rs1 ] << ( regs[ rs2 ] & 0x1f ) ); // sll rd, rs1, rs2
                else if ( 2 == funct3 )
                    regs[ rd ] = ( (int64_t) regs[ rs1 ] < (int64_t) regs[ rs2 ] ); // slt rd, rs1, rs2
                else if ( 3 == funct3 )
                    regs[ rd ] = ( regs[ rs1 ] < regs[ rs2 ] ); // sltu rd, rs1, rs2
                else if ( 4 == funct3 )
                    regs[ rd ] = ( regs[ rs1 ] ^ regs[ rs2 ] ); // xor rd, rs1, rs2
                else if ( 5 == funct3 )
                    regs[ rd ] = ( regs[ rs1 ] >> ( regs[ rs2 ] & 0x1f ) ); // slr rd, rs1, rs2
                else if ( 6 == funct3 )
                    regs[ rd ] = regs[ rs1 ] | regs[ rs2 ]; // or rd, rs1, rs2
                else if ( 7 == funct3 )
                    regs[ rd ] = regs[ rs1 ] & regs[ rs2 ]; // and rd, rs1, rs2
                else
                    unhandled();
            }
            else if ( 1 == funct7 )
            {
                if ( 0 == funct3 )
                    regs[ rd ] = regs[ rs1 ] * regs[ rs2 ]; // mul rd, rs1, rs2
                else if ( 4 == funct3 )
                {
                    if ( 0 != (int64_t) regs[ rs2 ] )
                        regs[ rd ] = (int64_t) regs[ rs1 ] / (int64_t) regs[ rs2 ]; // div rd, rs1, rs2
                }
                else if ( 5 == funct3 )
                {
                    if ( 0 != regs[ rs2 ] )
                        regs[ rd ] = regs[ rs1 ] / regs[ rs2 ]; // udiv rd, rs1, rs2
                }
                else if ( 6 == funct3 )
                {
                    if ( 0 != (int64_t) regs[ rs2 ] )
                        regs[ rd ] = (int64_t) regs[ rs1 ] % (int64_t) regs[ rs2 ]; // rem rd, rs1, rs2
                }
                else if ( 7 == funct3 )
                {
                    if ( 0 != regs[ rs2 ] )
                        regs[ rd ] = regs[ rs1 ] % regs[ rs2 ]; // remu rd, rs1, rs2
                }
                else
                    unhandled();
            }
            else if ( 0x20 == funct7 )
            {
                if ( 0 == funct3 )
                    regs[ rd ] = regs[ rs1 ] - regs[ rs2 ]; // sub rd, rs1, rs2
                else if ( 5 == funct3 )
                    regs[ rd ] = ( regs[ rs1 ] >> ( 0x1f & regs[ rs2 ] ) ); // sra rd, rs1, rs2
                else
                    unhandled();
            }
            else
                unhandled();
            break;
        }
        case 0xd: // lui rd, uimm
        {
            assert_type( UType );
            decode_U();
            if ( 0 == rd )
                break;

            regs[ rd ] = ( u_imm << 12 );
            break;
        }
        case 0xe:
        {
            assert_type( RType );
            decode_R();
            if ( 0 == rd )
                break;

            if ( 0 == funct7 )
            {
                if ( 0 == funct3 )
                    regs[ rd ] = (int32_t) ( 0xffffffff & regs[ rs1 ] ) + (int32_t) ( 0xffffffff & regs[ rs2 ] ); // addw rd, rs1, rs2
                else if ( 1 == funct3 )
                    regs[ rd ] = ( 0xffffffff & regs[ rs1 ] ) << ( 0xffffffff & regs[ rs2 ] ); // sllw rd, rs1, rs2
                else if ( 5 == funct3 )
                    regs[ rd ] = ( 0xffffffff & regs[ rs1 ] ) >> ( 0x1f & regs[ rs2 ] ); // srlw rd, rs1, rs2
                else
                    unhandled();
            }
            else if ( 1 == funct7 )
            {
                if ( 0 == funct3 )
                    regs[ rd ] = ( 0xffffffff & regs[ rs1 ] ) * ( 0xffffffff & regs[ rs2 ] ); // mulw rd, rs1, rs2
                else if ( 4 == funct3 )
                {
                    if ( 0 != (int32_t) ( 0xffffffff & regs[ rs2 ] ) )
                        regs[ rd ] = (int32_t) ( 0xffffffff & regs[ rs1 ] ) / (int32_t) ( 0xffffffff & regs[ rs2 ] ); // divw rd, rs1, rs2
                }
                else if ( 5 == funct3 )
                {
                    if ( 0 != (int32_t) ( 0xffffffff & regs[ rs2 ] ) )
                        regs[ rd ] = ( 0xffffffff & regs[ rs1 ] ) / ( 0xffffffff & regs[ rs2 ] ); // divuw rd, rs1, rs2
                }
                else if ( 6 == funct3 )
                {
                    if ( 0 != (int32_t) ( 0xffffffff & regs[ rs2 ] ) )
                        regs[ rd ] = (int32_t) ( 0xffffffff & regs[ rs1 ] ) % (int32_t) ( 0xffffffff & regs[ rs2 ] ); // remw rd, rs1, rs2
                }
                else if ( 7 == funct3 )
                {
                    if ( 0 != ( 0xffffffff & regs[ rs2 ] ) )
                        regs[ rd ] = ( 0xffffffff & regs[ rs1 ] ) % ( 0xffffffff & regs[ rs2 ] ); // remuw rd, rs1, rs2
                }
                else
                    unhandled();
            }
            else if ( 0x20 == funct7 )
            {
                if ( 0 == funct3 )
                    regs[ rd ] = (int64_t) ( (int32_t) ( 0xffffffff & regs[ rs1 ] ) - (int32_t) ( 0xffffffff & regs[ rs2 ] ) ); // subw rd, rs1, rs2
                else if ( 5 == funct3 )
                    regs[ rd ] = ( 0xffffffff & regs[ rs1 ] ) >> ( 0xffffffff & regs[ rs2 ] ); // sraw rd, rs1, rs2
                else
                    unhandled();
            }
            else
                unhandled();
            break;
        }
        case 0x18:
        {
            assert_type( BType );
            decode_B();

            if ( 0 == funct3 )  // beq rs1, rs2, bimm
            {
                if ( regs[ rs1 ] == regs[ rs2 ] )
                {
                    pc += b_imm;
                    return true;
                }
            }
            else if ( 1 == funct3 )  // bne rs1, rs2, bimm
            {
                if ( regs[ rs1 ] != regs[ rs2 ] )
                {
                    pc += b_imm;
                    return true;
                }
            }
            else if ( 4 == funct3 )  // blt rs1, rs2, bimm
            {
                if ( (int64_t) regs[ rs1 ] < (int64_t) regs[ rs2 ] )
                {
                    pc += b_imm;
                    return true;
                }
            }
            else if ( 5 == funct3 ) // bge rs1, rs2, b_imm
            {
                if ( (int64_t) regs[ rs1 ] >= (int64_t) regs[ rs2 ] )
                {
                    pc += b_imm;
                    return true;
                }
            }
            else if ( 6 == funct3 )  // bltu rs1, rs2, bimm
            {
                if ( regs[ rs1 ] < regs[ rs2 ] )
                {
                    pc += b_imm;
                    return true;
                }
            }
            else if ( 7 == funct3 )  // bgeu rs1, rs2, bimm
            {
                if ( regs[ rs1 ] >= regs[ rs2 ] )
                {
                    pc += b_imm;
                    return true;
                }
            }
            else
                unhandled();
            break;
        }
        case 0x19:
        {
            assert_type( IType );
            decode_I();

            if ( 0 == funct3 )
            {
                pc = ( regs[ rs1 ] + i_imm ); // jalr (rs1) + i_imm
                if ( 0 != rd )
                    regs[ rd ] = pcnext;
                return true;
            }
            else
                unhandled();
            break;
        }
        case 0x1b:
        {
            assert_type( JType );
            decode_J();
            if ( 0 != rd )
                regs[ rd ] = pcnext;

            // jal %offset

            int64_t offset = j_imm_u;
            pc = pc + offset;
            return true;
        }
        case 0x1c:
        {
            if ( 0x73 == op )
                riscv_invoke_ecall( *this ); // ecall
            else
            {
                assert_type( IType );
                decode_I();

                if ( 2 == funct3 )
                {
                    if ( 0xc00 == i_imm_u ) // csrss rd, cycle, rs1
                    {
                        if ( 0 != rd )
                            regs[ rd ] = 1000 * clock(); // fake microseconds
                    }
                    else
                        unhandled();
                }
                else
                    unhandled();
            }
            break;
        }
        default:
            unhandled();
    }

    return false;
} //execute_instruction

uint64_t RiscV::run( uint64_t max_cycles )
{
    uint64_t cycles = 0;
    assert( !rvc ); // to do later

    do
    {
        assert( 0 == regs[ 0 ] );
        assert( regs[ sp ] > ( base + mem_size - stack_size ) );
        assert( regs[ sp ] <= ( base + mem_size ) );
        cycles++;
        uint64_t pcnext = decode();

        if ( 0 != g_State )
        {
            if ( g_State & stateEndEmulation )
            {
                g_State &= ~stateEndEmulation;
                break;
            }

            if ( g_State & stateTraceInstructions )
                trace_state( pcnext );
        }

        bool jump = execute_instruction( pcnext );
        if ( !jump )
            pc = pcnext;
    } while ( cycles < max_cycles );

    return cycles;
} //run

