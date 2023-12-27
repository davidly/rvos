/*
    This is a simplistic 64-bit RISC-V emulator.
    Only physical memory is supported.
    The core set of instructions "rv64imadfc" are implemented: integer, multiply/divide, atomic, double, float, compressed
    I tested with a variety of C and C++ apps compiled with four different versions of g++ (each exposed different bugs)
    I also tested with the BASIC test suite for my compiler BA, which targets risc-v.
    It's slightly faster than the 400Mhz K210 processor on my AMD 5950x machine.

    Written by David Lee in February 2023

    Useful: https://luplab.gitlab.io/rvcodecjs/#q=c00029f3&abi=false&isa=AUTO
            https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-cc.adoc#abi-lp64d
            https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#:~:text=In%20computing%2C%20the%20Executable%20and,shared%20libraries%2C%20and%20core%20dumps.
            https://jemu.oscc.cc/AUIPC
            https://inst.eecs.berkeley.edu/~cs61c/resources/su18_lec/Lecture7.pdf
            https://riscv.org/wp-content/uploads/2019/06/riscv-spec.pdf
*/

#include <stdint.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <chrono>

#include <djl_128.hxx>
#include <djltrace.hxx>

#include "riscv.hxx"

using namespace std;
using namespace std::chrono;

// conditional move and conditional return instruction extensions

#define USE_DJL_RISCV_EXTENSIONS 0

// set to 1 to use the instruction decompression lookup table
// set to 0 to use the C code or to generate the table

#define USE_RVCTABLE 1

// these instruction types are mostly just useful for debugging

const uint8_t IllType = 0;
const uint8_t UType = 1;
const uint8_t JType = 2;
const uint8_t IType = 3;
const uint8_t BType = 4;
const uint8_t SType = 5;
const uint8_t RType = 6;
const uint8_t CsrType = 7;
const uint8_t R4Type = 8;
const uint8_t ShiftType = 9;
const uint8_t CType = 10; // risc-v extension for cmvxx instructions

static const char instruction_types[] =
{
    '!', 'U', 'J', 'I', 'B', 'S', 'R', 'C', 'r', 's', 'c',
};

static uint32_t g_State = 0;

const uint32_t stateTraceInstructions = 1;
const uint32_t stateEndEmulation = 2;

bool RiscV::trace_instructions( bool t )
{
    bool prev = ( 0 != ( g_State & stateTraceInstructions ) );
    if ( t )
        g_State |= stateTraceInstructions;
    else
        g_State &= ~stateTraceInstructions;
    return prev;
} //trace_instructions

void RiscV::end_emulation() { g_State |= stateEndEmulation; }

// for the 32 opcode_types ( ( opcode >> 2 ) & 0x1f )

static const uint8_t riscv_types[ 32 ] =
{
    IType,   //  0
    IType,   //  1
    CType,   //  2  // risc-v extension for cmvxx instructions
    IType,   //  3
    IType,   //  4
    UType,   //  5
    IType,   //  6
    IllType, //  7
    SType,   //  8
    SType,   //  9
    RType,   //  a // risc-v extension for jrxx instructions
    RType,   //  b
    RType,   //  c
    UType,   //  d
    RType,   //  e
    IllType, //  f
    RType,   // 10
    RType,   // 11
    RType,   // 12
    IllType, // 13
    RType,   // 14
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

#pragma warning(disable: 4100)
void RiscV::assert_type( uint8_t t ) { assert( t == riscv_types[ opcode_type ] ); }

static const char * register_names[ 32 ] =
{
    "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

static const char * fregister_names[ 32 ] =
{
    "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
    "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
    "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
    "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11",
};

const char * RiscV::reg_name( uint64_t reg )
{
    if ( reg >= 32 )
        return "invalid register";

    return register_names[ reg ];
} //reg_name

const char * RiscV::freg_name( uint64_t reg )
{
    if ( reg >= 32 )
        return "invalid f register";

    return fregister_names[ reg ];
} //freg_name

static void unhandled_op16( uint16_t x )
{
    printf( "compressed opcode not handled: %04x\n", x );
    tracer.Trace( "compressed opcode not handled: %04x\n", x );
    exit( 1 );
} //unhandled_op16

static bool g_failOnUncompressError = true;

#if USE_RVCTABLE

    #include "rvctable.txt"
    
    uint32_t RiscV::uncompress_rvc( uint16_t x )
    {
        uint32_t op32 = rvc_lookup[ x ];
        if ( 0 == op32 )
            unhandled_op16( x );
    
        return op32;
    } //uncompress_rvc

#else // code, not a table

static uint32_t compose_I( uint32_t funct3, uint32_t rd, uint32_t rs1, uint32_t imm, uint32_t opcode_type )
{
    //if ( g_State & stateTraceInstructions )
    //    tracer.Trace( "  composing I funct3 %02x, rd %x, rs1 %x, imm %d, opcode_type %x\n", funct3, rd, rs1, imm, opcode_type );

    return ( funct3 << 12 ) | ( rd << 7 ) | ( rs1 << 15 ) | ( imm << 20 ) | ( opcode_type << 2 ) | 0x3;
} //compose_I

static uint32_t compose_R( uint32_t funct3, uint32_t funct7, uint32_t rd, uint32_t rs1, uint32_t rs2, uint32_t opcode_type )
{
    //if ( g_State & stateTraceInstructions )
    //    tracer.Trace( "  composing R funct3 %02x, funct7 %02x, rd %x, rs1 %x, rs2 %x, opcode_type %x\n", funct3, funct7, rd, rs1, rs2, opcode_type );

    return ( funct3 << 12 ) | ( funct7 << 25 ) | ( rd << 7 ) | ( rs1 << 15 ) | ( rs2 << 20 ) | ( opcode_type << 2 ) | 0x3;
} //compose_R

static uint32_t compose_S( uint32_t funct3, uint32_t rs1, uint32_t rs2, uint32_t imm, uint32_t opcode_type )
{
    //if ( g_State & stateTraceInstructions )
    //    tracer.Trace( "  composing S funct3 %02x, rs1 %x, rs2 %x, imm %d, opcode_type %x\n", funct3, rs1, rs2, imm, opcode_type );

    uint32_t i = ( ( imm << 7 ) & 0xf80 ) | ( ( imm << 20 ) & 0xfe000000 );

    return ( funct3 << 12 ) | ( rs1 << 15 ) | ( rs2 << 20 ) | i | ( opcode_type << 2 ) | 0x3;
} //compose_S

static uint32_t compose_U( uint32_t rd, uint32_t imm, uint32_t opcode_type )
{
    return ( rd << 7 ) | ( imm << 12 ) | ( opcode_type << 2 ) | 0x3;
} //compose_U

static uint32_t compose_J( uint32_t offset, uint32_t opcode_type )
{
    //                                            31 30   20 19
    // j itself decodes from upper 20 bits as imm[20|10:1|11|19:12]

    offset = ( ( offset << 11 ) & 0x80000000 ) |
             ( ( offset << 20 ) & 0x7fe00000 ) |
             ( ( offset << 9 )  & 0x00100000 ) |
             ( offset &           0x000ff000 );
    //tracer.Trace( "j offset re-encoded as %x = %d\n", offset, offset );

    return offset | ( opcode_type << 2 ) | 0x3;
} //compose_J

static uint32_t compose_B( uint32_t funct3, uint32_t rs1, uint32_t rs2, uint32_t imm, uint32_t opcode_type )
{
    // offset 12..1

    uint32_t offset = ( ( imm << 19 ) & 0x80000000 ) | ( ( imm << 20 ) & 0x7e000000 ) |
                      ( ( imm << 7 ) & 0xf00 )       | ( ( imm >> 4 ) & 0x80 );
    //tracer.Trace( "offset before %x and after composing: %x\n", imm, offset );
    return ( funct3 << 12 ) | ( rs1 << 15 ) | ( rs2 << 20 ) | offset | ( opcode_type << 2 ) | 0x3;
} //compose_B

uint32_t RiscV::uncompress_rvc( uint16_t x )
{
    uint32_t op32 = 0;
    uint16_t op2 = x & 0x3;
    const uint32_t rprime_offset = 8; // add this to r' to get a final r
    uint16_t p_funct3 = ( x >> 13 ) & 0x7;   // p_ for prime -- the compressed version
    uint16_t bit12 = ( x >> 12 ) & 1;

    if ( g_State & stateTraceInstructions )
        tracer.Trace( "rvc op %04x op2 %d funct3 %d bit12 %d\n", x, op2, p_funct3, bit12 );

    /*
        From https://riscv.org/wp-content/uploads/2019/06/riscv-spec.pdf
        There are many exceptions to these rules.

        Format Meaning           15  14 13  12  11 10 9 8 7  6 5 4 3 2  1 0
        CR Register              funct3         rd/rs1       rs2        op
        CI Immediate             funct3     imm rd/rs1       imm        op
        CSS Stack-relative Store funct3     imm              rs2        op
        CIW Wide Immediate       funct3     imm                  rd     op
        CL Load                  funct3     imm       rs1 x  imm rd     op
        CS Store                 funct3     imm       rs1 x  imm rs2    op
        CB Branch                funct3     offset    rs1    offset     op
        CJ Jump                  funct3     jump-target                 op
    */

    switch ( op2 )
    {
        case 0:
        {
            uint32_t p_imm = ( ( x >> 7 ) & 0x38 ) | ( ( x << 1 ) & 0xc0 );
            uint32_t p_rs1 = ( ( x >> 7 ) & 0x7 ) + rprime_offset;
            uint32_t p_rdrs2 = ( ( x >> 2 ) & 0x7 ) + rprime_offset;

            switch( p_funct3 )
            {
                case 0: // c.addi4spn
                {
                    uint32_t amount = ( ( x >> 7 ) & 0x30 ) | ( ( x >> 1 ) & 0x3c0 ) | ( ( x >> 4 ) & 0x4 ) | ( ( x >> 2 ) & 0x8 );
                    //tracer.Trace( "adjusting pointer to sp-offset using addi, amount %d\n", amount );

                    // addi funct3 = 0, rd = p_rdrs2, rs1 = sp, i_imm = amount
                    op32 = compose_I( 0, p_rdrs2, sp, amount, 0x4 );
                    break;
                }
                case 1: // c.fld
                {
                    op32 = compose_I( 3, p_rdrs2, p_rs1, p_imm, 1 );
                    break;
                }
                case 2: // c.lw
                {
                    p_imm = ( ( x >> 7 ) & 0x38 ) | ( ( x >> 4 ) & 0x4 ) | ( ( x << 1 ) & 0x40 );
                    op32 = compose_I( 2, p_rdrs2, p_rs1, p_imm, 0 );
                    break;
                }
                case 3: // c.ld
                {
                    //tracer.Trace( "composing ld %d, %d(%d)\n", p_rdrs2, p_imm, p_rs1 );
                    op32 = compose_I( 3, p_rdrs2, p_rs1, p_imm, 0 );
                    break;
                }
                case 4: // reserved
                {
                    break;
                }
                case 5: // c.fsd
                {
                    op32 = compose_S( 3, p_rs1, p_rdrs2, p_imm, 9 );
                    break;
                }
                case 6: // c.sw
                {
                    p_imm = ( ( x >> 7 ) & 0x38 ) | ( ( x >> 4 ) & 0x4 ) | ( ( x << 1 ) & 0x40 );
                    op32 = compose_S( 2, p_rs1, p_rdrs2, p_imm, 8 );
                    break;
                }
                case 7: // c.sd
                {
                    op32 = compose_S( 3, p_rs1, p_rdrs2, p_imm, 8 );
                    break;
                }
            }
            break;
        }
        case 1:
        {
            uint32_t p_imm = sign_extend( ( ( x >> 7 ) & 0x20 ) | ( ( x >> 2 ) & 0x1f ), 5 );
            uint32_t p_rs1rd = ( ( x >> 7 ) & 0x1f );

            switch( p_funct3 )
            {
                case 0: // c.addi
                {
                    op32 = compose_I( 0, p_rs1rd, p_rs1rd, p_imm, 4 );
                    break;
                }
                case 1: // c.addiw
                {
                    op32 = compose_I( 0, p_rs1rd, p_rs1rd, p_imm, 6);
                    break;
                }
                case 2: // c.li
                {
                    op32 = compose_I( 0, p_rs1rd, zero, p_imm, 4 ); // addi rd, zero, imm
                    break;
                }
                case 3:
                {
                    if ( 2 == p_rs1rd ) // c.addi16sp
                    {
                        uint32_t amount = ( ( x >> 3 ) & 0x200 ) | ( ( x >> 2 ) & 0x10 ) | ( ( x << 1 ) & 0x40 ) |
                                          ( ( x << 4 ) & 0x180 ) | ( ( x << 3 ) & 0x20 );
                        //tracer.Trace( "addi16sp amount %d\n", amount );
                        amount = sign_extend( amount, 9 );
                        //tracer.Trace( "addi16sp extended amount %d\n", amount );
                        op32 = compose_I( 0, sp, sp, amount, 0x4 );
                    }
                    else // c.lui
                    {
                        uint32_t amount = ( ( x << 5 ) & 0x20000 ) | ( ( x << 10 ) & 0x1f000 );
                        amount = sign_extend( amount, 17 );
                        amount >>= 12;
                        op32 = compose_U( p_rs1rd, amount, 0xd );
                    }
                    break;
                }
                case 4: // many
                {
                    uint16_t funct11_10 = ( x >> 10 ) & 0x3;
                    uint32_t p_rs1rd = ( ( x >> 7 ) & 0x7 ) + rprime_offset;
                    uint32_t p_rs2 = ( ( x >> 2 ) & 0x7 ) + rprime_offset;

                    switch ( funct11_10 )
                    {
                        case 0: // c.srli + c.srli64
                        {
                            uint32_t amount = ( ( x >> 7 ) & 0x20 ) | ( ( x >> 2 ) & 0x1f );
                            //tracer.Trace( "srli shift amount: %d\n", amount );
                            op32 = compose_I( 5, p_rs1rd, p_rs1rd, amount, 4 );
                            break;
                        }
                        case 1: // c.srai + c.srai64
                        {
                            uint32_t amount = ( ( x >> 7 ) & 0x20 ) | ( ( x >> 2 ) & 0x1f );
                            //tracer.Trace( "srai shift amount: %d\n", amount );
                            amount |= 0x400; // set this bit so it's srai, not srli
                            op32 = compose_I( 5, p_rs1rd, p_rs1rd, amount, 4 );
                            break;
                        }
                        case 2: // c.andi
                        {
                            op32 = compose_I( 7, p_rs1rd, p_rs1rd, p_imm, 4 );
                            break;
                        }
                        case 3:
                        {
                            uint16_t funct6_5 = ( x >> 5 ) & 0x3;

                            if ( 0 == bit12 )
                            {
                                if ( 0 == funct6_5 ) // c.sub
                                    op32 = compose_R( 0, 0x20, p_rs1rd, p_rs1rd, p_rs2, 0xc );
                                else if ( 1 == funct6_5 ) // c.xor
                                    op32 = compose_R( 4, 0, p_rs1rd, p_rs1rd, p_rs2, 0xc );
                                else if ( 2 == funct6_5 ) // c.or
                                    op32 = compose_R( 6, 0, p_rs1rd, p_rs1rd, p_rs2, 0xc );
                                else if ( 3 == funct6_5 ) // c.and
                                    op32 = compose_R( 7, 0, p_rs1rd, p_rs1rd, p_rs2, 0xc );
                            }
                            else
                            {
                                if ( 0 == funct6_5 ) // c.subw
                                    op32 = compose_R( 0, 0x20, p_rs1rd, p_rs1rd, p_rs2, 0xe );
                                else if ( 1 == funct6_5 ) // c.addw
                                    op32 = compose_R( 0, 0, p_rs1rd, p_rs1rd, p_rs2, 0xe );
                            }
                        }
                    }
                    break;
                }
                case 5: // c.j
                {
                    // j offset. J type. opcode_type 0x1b
                    //                                             12 11 10 8  7 6 5   2
                    // bits 12:2 should be decoded as 12 bits: imm[11|4|9:8|10|6|7|3:1|5]

                    uint32_t offset = ( ( x >> 1 ) & 0x800 ) | ( ( x >> 7 ) & 0x10 ) | ( ( x >> 1 ) & 0x300 ) | ( ( x << 2 ) & 0x400 ) |
                                      ( ( x >> 1 ) & 0x40 )  | ( ( x << 1 ) & 0x80 ) | ( ( x >> 2 ) & 0xe )   | ( ( x << 3 ) & 0x20 );
                    //tracer.Trace( "j offset decoded as %x = %d\n", offset, offset );
                    offset = sign_extend( offset, 11 );
                    //tracer.Trace( "j offset extended as %x = %d\n", offset, offset );
                    op32 = compose_J( offset, 0x1b );
                    break;
                }
                case 6: // c.beqz
                {
                    uint32_t p_rs1 = ( ( x >> 7 ) & 0x7 ) + rprime_offset;
                    uint32_t offset = ( ( x >> 4 ) & 0x100 ) | ( ( x >> 7 ) & 0x18 ) | ( ( x << 1 ) & 0xc0 ) |
                                      ( ( x >> 2 ) & 0x6 )   | ( ( x << 3 ) & 0x20 );
                    offset = sign_extend( offset, 8 );
                    op32 = compose_B( 0, p_rs1, zero, offset, 0x18 );
                    break;
                }
                case 7: // c.bnez
                {
                    uint32_t p_rs1 = ( ( x >> 7 ) & 0x7 ) + rprime_offset;
                    uint32_t offset = ( ( x >> 4 ) & 0x100 ) | ( ( x >> 7 ) & 0x18 ) | ( ( x << 1 ) & 0xc0 ) |
                                      ( ( x >> 2 ) & 0x6 )   | ( ( x << 3 ) & 0x20 );
                    offset = sign_extend( offset, 8 );
                    op32 = compose_B( 1, p_rs1, zero, offset, 0x18 );
                    break;
                }
            }
            break;
        }
        case 2:
        {
            uint32_t p_rs1rd = ( ( x >> 7 ) & 0x1f );
            uint32_t p_rs2 = ( ( x >> 2 ) & 0x1f );

            switch( p_funct3 )
            {
                case 0: // c.cslli
                {
                    if ( 0 == bit12 && 0 == p_rs2 ) // slli64
                    {
                        tracer.Trace( "warning: ignoring slli64\n" );
                    }
                    else // slli
                    {
                        uint32_t amount = ( ( x >> 7 ) & 0x20 ) | p_rs2;
                        op32 = compose_I( 1, p_rs1rd, p_rs1rd, amount, 4 );
                    }
                    break;
                }
                case 1: // c.fldsp
                {
                    uint32_t i = ( ( x >> 7 ) & 0x20 ) | ( ( x >> 2 ) & 0x18 ) | ( ( x << 4 ) & 0x1c0 );
                    op32 = compose_I( 3, p_rs1rd, sp, i, 1 );                    
                    break;
                }
                case 2: // c.lwsp
                {
                    uint32_t i = ( ( x >> 7 ) & 0x20 ) | ( ( x >> 2 ) & 0x1c ) | ( ( x << 4 ) & 0x0c0 );
                    op32 = compose_I( 2, p_rs1rd, sp, i, 0 );
                    break;
                }
                case 3: // c.ldsp
                {
                    uint32_t i = ( ( x >> 7 ) & 0x20 ) | ( ( x >> 2 ) & 0x18 ) | ( ( x << 4 ) & 0x1c0 );
                    op32 = compose_I( 3, p_rs1rd, sp, i, 0 );
                    break;
                }
                case 4: // several
                {
                    uint32_t p_rs1rd = ( ( x >> 7 ) & 0x1f );
                    uint32_t p_rs2 = ( ( x >> 2 ) & 0x1f );

                    if ( 0 == bit12 )
                    {
                        if ( 0 == p_rs2 ) // c.jr
                            op32 = compose_I( 0, 0, p_rs1rd, 0, 0x19 ); // I type jalr (p_rs1rd) + 0
                        else // c.mv
                            op32 = compose_I( 0, p_rs1rd, p_rs2, 0, 4 ); // add rd, rs, zero
                    }
                    else
                    {
                        if ( 0 == p_rs1rd ) // c.ebreak;
                            op32 = 0x00100073;
                        else
                        {
                            if ( 0 == p_rs2 ) // c.jalr
                                op32 = compose_I( 0, 1, p_rs1rd, 0, 0x19 ); // always store ra in ra/x1
                            else // c.add
                                op32 = compose_R( 0, 0, p_rs1rd, p_rs1rd, p_rs2, 0xc );
                        }
                    }
                    break;
                }
                case 5: // c.fsdsp
                {
                    uint32_t p_imm = ( ( x >> 7 ) & 0x38 ) | ( ( x >> 1 ) & 0x1c0 );
                    uint32_t p_rs2 = ( ( x >> 2 ) & 0x1f );
                    op32 = compose_S( 3, sp, p_rs2, p_imm, 9 );
                    break;
                }
                case 6: // c.swsp
                {
                    uint32_t p_imm = ( ( x >> 7 ) & 0x3c ) | ( ( x >> 1 ) & 0xc0 );
                    uint32_t p_rs2 = ( ( x >> 2 ) & 0x1f );
                    op32 = compose_S( 2, sp, p_rs2, p_imm, 8 );
                    break;
                }
                case 7: // c.sdsp
                {
                    uint32_t p_imm = ( ( x >> 7 ) & 0x38 ) | ( ( x >> 1 ) & 0x1c0 );
                    uint32_t p_rs2 = ( ( x >> 2 ) & 0x1f );
                    op32 = compose_S( 3, sp, p_rs2, p_imm, 8 );
                    break;
                }
            }
            break;
        }
        default:
        {
            assert( "opcode does not appear to be compressed\n" );
            break;
        }
    }

    if ( 0 == op32 && g_failOnUncompressError )
        unhandled_op16( x );

    return op32;
} //uncompress_rvc

#endif // use code for rvc decompression, not a table

bool RiscV::generate_rvc_table( const char * path )
{
    g_failOnUncompressError = false;

    FILE * fp = fopen( path, "w" );
    if ( fp )
    {
        fprintf( fp, "static const uint32_t rvc_lookup[65536] = \n{" );

        for ( uint32_t i = 0; i <= 0xffff; i++ )
        {
            if ( 0 == ( i % 16 ) )
                fprintf( fp, "\n  " );

            uint32_t op32 = 0;
            if ( 0x3 != ( i & 0x3 ) )
                op32 = uncompress_rvc( (uint16_t) i );

            fprintf( fp, ( 0 == op32 ) ? "%x, " : "0x%x, ", op32 );
        }

        fprintf( fp, "\n};" );
        fclose( fp );
    }
    else
        return false;

    return true;
} //generate_rvc_table

static const char * comparison_types[] =
{
    "eq", "ne", "error-le?", "error-gt?", "lt", "ge", "ltu", "geu", 
};

static const char * cmp_type( uint64_t t )
{
    assert( t < _countof( comparison_types ) );
    return comparison_types[ t ];
} //cmp_type

void RiscV::trace_state()
{
    uint8_t optype = riscv_types[ opcode_type ];

    //tracer.TraceBinaryData( getmem( 0x7e8d0 ), 16, 2 );

    static char acExtra[ 1024 ];
    acExtra[ 0 ] = 0;
    sprintf( acExtra, "t0 %llx t1 %llx s0 %llx s1 %llx, s7 %llx", regs[ t0 ], regs[ t1 ], regs[ s0 ], regs[ s1 ], regs[ s7 ] );

    static const char * previous_symbol = 0;
    const char * symbol_name = riscv_symbol_lookup( pc );
    if ( symbol_name == previous_symbol )
        symbol_name = "";
    else
        previous_symbol = symbol_name;

    tracer.Trace( "pc %8llx %s op %8llx a0 %llx a1 %llx a2 %llx a3 %llx a4 %llx a5 %llx gp %llx %s ra %llx sp %llx t %2llx %c => ",
                  pc, symbol_name, op, regs[ a0 ], regs[ a1 ], regs[ a2 ], regs[ a3 ], regs[ a4 ], regs[ a5 ], regs[ gp ], acExtra,
                  regs[ ra ], regs[ sp ], opcode_type, instruction_types[ optype ] );

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
                tracer.Trace( "lui     %s, %lld  # %llx\n", reg_name( rd ), u_imm << 12, u_imm << 12 );
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
            else if ( 1 == opcode_type )
            {
                if ( 2 == funct3 )
                    tracer.Trace( "flw     %s, %d(%s)  # %.2f\n", freg_name( rd ), i_imm, reg_name( rs1 ), getfloat( i_imm + regs[ rs1 ] ) );
                else if ( 3 == funct3 )
                    tracer.Trace( "fld     %s, %d(%s)  # %.2f\n", freg_name( rd ), i_imm, reg_name( rs1 ), getdouble( i_imm + regs[ rs1 ] ) );
            }
            else if ( 3 == opcode_type )
            {
                if ( 0 == funct3 || 1 == funct3 )
                    tracer.Trace( "fence\n" );
            }
            else if ( 4 == opcode_type )
            {
                decode_I_shift();

                switch( funct3 )
                {
                    case 0: tracer.Trace( "addi    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm ); break;
                    case 1: tracer.Trace( "slli    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_shamt6 ); break;
                    case 2: tracer.Trace( "slti    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm ); break;
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
            else if ( 6 == opcode_type )
            {
                decode_I_shift();

                if ( 0 == funct3 )
                {
                    uint32_t x = (uint32_t) ( ( 0xffffffff & regs[ rs1 ] ) + i_imm );
                    uint64_t ext = sign_extend( x, 31 );
                    tracer.Trace( "addiw   %s, %s, %lld  # %d + %d = %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm, regs[ rs1 ], i_imm, ext );
                }
                else if ( 1 == funct3 )
                {
                    if ( 0 == i_top2 )
                    {
                        uint32_t val = (uint32_t) regs[ rs1 ];
                        //uint32_t result = val << i_shamt5;
                        //uint64_t result64 = sign_extend( result, 31 );
                        tracer.Trace( "slliw   %s, %s, %lld  # %x << %d\n", reg_name( rd ), reg_name( rs1 ), i_shamt5, val, i_shamt5 );
                    }
                }
                else if ( 5 == funct3 )
                {
                    if ( 0 == i_top2 )
                        tracer.Trace( "srliw   %s, %s, %lld  # %x >> %d = %x\n", reg_name( rd ), reg_name( rs1 ), i_shamt5, regs[ rs1 ], i_shamt5,
                                      ( ( 0xffffffff & regs[ rs1 ] ) >> i_shamt5 )  );
                    else if ( 1 == i_top2 )
                        tracer.Trace( "sraiw   %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_shamt5  );
                }
            }
            else if ( 0x19 == opcode_type )
            {
                if ( 0 == funct3 )
                    tracer.Trace( "jalr    %s, %s, %lld\n", reg_name( rd ), reg_name( rs1 ), i_imm );
            }
            else if ( 0x1c == opcode_type )
            {
                uint64_t csr = i_imm_u;

                // funct3
                //    000  system (ecall / ebreak / sfence / hfence)
                //    001  csrrw write csr
                //    010  csrrs read csr
                //    011  csrrc clear bits in csr
                //    101  csrrwi write csr immediate
                //    110  csrrsi set bits in csr immediate
                //    111  csrrci clear bits in csr immediate

                if ( 0 == funct3 ) // system
                {
                    if ( 0x73 == op ) 
                        tracer.Trace( "ecall\n" );
                    else if ( 0x100073 == op )
                        tracer.Trace( "ebreak\n" );
                    else
                        tracer.Trace( "unknown system e instruction\n" );
                }
                else if ( 1 == funct3 ) // write csr
                {
                    if ( 1 == csr )
                        tracer.Trace( "csrrw   %s, fflags, %s  # read fp exception flags\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 2 == csr )
                        tracer.Trace( "csrrw   %s, frm, %s  # read fp rounding mode\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xc00 == csr )
                        tracer.Trace( "csrrw   %s, cycle, %s\n", reg_name( rd ), reg_name( rs1 ) );
                    else
                        tracer.Trace( "csrrw unknown\n" );
                }
                else if ( 2 == funct3 ) // read csr
                {
                    if ( 1 == csr )
                        tracer.Trace( "csrrs   %s, fflags, %s  # read fp exception flags\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 2 == csr )
                        tracer.Trace( "csrrs   %s, frm, %s  # read fp rounding mode\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x100 == csr )
                        tracer.Trace( "csrrs   %s, sstatus, %s  # supervisor status\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x104 == csr )
                        tracer.Trace( "csrrs   %s, sie, %s  # supervisor interrupt-enable\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x105 == csr )
                        tracer.Trace( "csrrs   %s, stvec, %s  # supervisor trap handler base address\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x140 == csr )
                        tracer.Trace( "csrrs   %s, sscratch, %s  # supervisor scratch register for trap handlers\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x141 == csr )
                        tracer.Trace( "csrrs   %s, sepc, %s  # supervisor exception program counter\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x142 == csr )
                        tracer.Trace( "csrrs   %s, scause, %s  # supervisor trap cause\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x143 == csr )
                        tracer.Trace( "csrrs   %s, stval, %s  # supervisor bad address or instruction\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x180 == csr )
                        tracer.Trace( "csrrs   %s, satp, %s  # supervisor adress translation and protection\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x302 == csr )
                        tracer.Trace( "csrrs   %s, medeleg, %s  # machine exception delegation\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x303 == csr )
                        tracer.Trace( "csrrs   %s, mideleg, %s  # machine interrupt delegation\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x304 == csr )
                        tracer.Trace( "csrrs   %s, mie, %s  # machine interrupt-enable\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x305 == csr )
                        tracer.Trace( "csrrs   %s, mtvec, %s  # machine trap-handler base addess\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x306 == csr )
                        tracer.Trace( "csrrs   %s, mcounteren, %s  # machine counter enable\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x340 == csr )
                        tracer.Trace( "csrrs   %s, mscratch, %s  # machine scratch for trap handlers\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0x341 == csr )
                        tracer.Trace( "csrrs   %s, mepc, %s  # machine exception program counter\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xb00 == csr )
                        tracer.Trace( "csrrs   %s, mcycle, %s  # rdmcycle read machine cycle counter\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xb02 == csr )
                        tracer.Trace( "csrrs   %s, minstret, %s  # rdminstret read machine instrutions retired\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xc00 == csr )
                        tracer.Trace( "csrrs   %s, cycle, %s  # rdcycle read cycle counter\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xc01 == csr )
                        tracer.Trace( "csrrs   %s, time, %s  # rdtime read time\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xc02 == csr )
                        tracer.Trace( "csrrs   %s, instret, %s  # rdinstret read instrutions retired\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xf11 == csr ) // mvendorid vendor
                        tracer.Trace( "csrrs   %s, mvendorid, %s  # vendor id\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xf12 == csr ) // marchid architecture
                        tracer.Trace( "csrrs   %s, marchid, %s  # architecture id\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 0xf13 == csr ) // mimpid implementation
                        tracer.Trace( "csrrs   %s, mimpid, %s  # implementation id\n", reg_name( rd ), reg_name( rs1 ) );
                    else
                        tracer.Trace( "csrrs unknown\n" );
                }
                else if ( 6 == funct3 ) // set bits in csr immediate
                {
                    if ( 1 == csr )
                        tracer.Trace( "csrrsi %s, fflags, %d  # set fp crs flags\n", reg_name( rd ), rs1 );
                    else
                        tracer.Trace( "unknown csr set bit instruction\n" );
                }
            }
            break;
        }
        case BType:
        {
            decode_B();
            if ( 0x18 == opcode_type )
                tracer.Trace( "b%s     %s, %s, %lld  # %8llx\n", cmp_type( funct3 ), reg_name( rs1 ), reg_name( rs2 ), b_imm, pc + b_imm );
            break;
        }
        case SType:
        {
            decode_S();
            //tracer.Trace( "s_imm %llx, rs2 %s, rs1 %s, funct3 %llx\n", s_imm, reg_name( rs2 ), reg_name( rs1 ), funct3 );

            if ( 8 == opcode_type )
            {
                if ( 0 == funct3 )
                    tracer.Trace( "sb      %s, %lld(%s)  #  %2x, %lld(%llx)\n", reg_name( rs2 ), s_imm, reg_name( rs1 ), (uint8_t) regs[ rs2 ], s_imm, regs[ rs1 ] );
                else if ( 1 == funct3 )
                    tracer.Trace( "sh      %s, %lld(%s)  #  %4x, %lld(%llx)\n", reg_name( rs2 ), s_imm, reg_name( rs1 ), (uint16_t) regs[ rs2 ], s_imm, regs[ rs1 ] );
                else if ( 2 == funct3 )
                    tracer.Trace( "sw      %s, %lld(%s)\n", reg_name( rs2 ), s_imm, reg_name( rs1 ) );
                else if ( 3 == funct3 )
                    tracer.Trace( "sd      %s, %lld(%s)  # %lld(%llx)\n", reg_name( rs2 ), s_imm, reg_name( rs1 ), s_imm, regs[ rs1 ] );
            }
            else if ( 9 == opcode_type )
            {
                if ( 2 == funct3 )
                    tracer.Trace( "fsw     %s, %lld(%s)  # %.2f\n", freg_name( rs2 ), s_imm, reg_name( rs1 ), fregs[ rs2 ].f );
                else if ( 3 == funct3 )
                    tracer.Trace( "fsd     %s, %lld(%s), # %.2f\n", freg_name( rs2 ), s_imm, reg_name( rs1 ), fregs[ rs2 ].d );
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

            if ( 0xa == opcode_type )
            {
                if ( 0 == funct7 )
                    tracer.Trace( "jr%s    %s, %s, %s\n", cmp_type( funct3 ), reg_name( rs1 ), reg_name( rs2 ), reg_name( rd ) );
            }
            else if ( 0xb == opcode_type )
            {
                uint32_t top5 = (uint32_t) ( funct7 >> 2 );
                if ( 0 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amoadd.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amoadd.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 1 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amoswap.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amoswap.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 2 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "lr.w %s, (%s)\n", reg_name( rd ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "lr.d %s, (%s)\n", reg_name( rd ), reg_name( rs1 ) );
                }
                else if ( 3 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "sc.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                   else if ( 3 == funct3 )
                        tracer.Trace( "sc.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 4 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amoxor.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amoxor.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 8 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amoor.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amoor.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 0xc == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amoand.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amoand.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 0x10 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amomin.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amomin.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 0x14 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amomax.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amomax.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 0x18 == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amominu.w %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amominu.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
                else if ( 0x1c == top5 )
                {
                    if ( 2 == funct3 )
                        tracer.Trace( "amomaxu.w %s, %s, (%s)  # max( %u, %u )\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ),
                                      (uint32_t) regs[ rs2 ], getui32( regs[ rs1 ] ) );
                    else if ( 3 == funct3 )
                        tracer.Trace( "amomaxu.d %s, %s, (%s)\n", reg_name( rd ), reg_name( rs2 ), reg_name( rs1 ) );
                }
            }
            else if ( 0xc == opcode_type )
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
                        tracer.Trace( "mul     %s, %s, %s # %llx * %llx\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ] );
                    else if ( 1 == funct3 )
                        tracer.Trace( "mulh    %s, %s, %s  # %lld * %lld\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ] );
                    else if ( 2 == funct3 )
                        tracer.Trace( "mulhsu  %s, %s, %s  # %lld * %llu\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ] );
                    else if ( 3 == funct3 )
                        tracer.Trace( "mulhu   %s, %s, %s  # %llu * %llu\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ] );
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
                    {
                        int32_t val = (int32_t) ( 0xffffffff & regs[ rs1 ] ) + (int32_t) ( 0xffffffff & regs[ rs2 ] );
                        uint64_t result = sign_extend( (uint32_t) val, 31 );
                        tracer.Trace( "addw    %s, %s, %s  # %d + %d = %lld\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ),
                                      regs[ rs1 ], regs[ rs2 ], result );
                    }
                    else if ( 1 == funct3 )
                    {
                        uint32_t val = (uint32_t) regs[ rs1 ];
                        uint32_t amount = 0x1f & regs[ rs2 ];
                        uint32_t result = val << amount;
                        uint64_t result64 = sign_extend( result, 31 );
                        tracer.Trace( "sllw    %s, %s, %s  # %x << %d = %llx\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), val, amount, result64 );
                    }
                    else if ( 5 == funct3 )
                        tracer.Trace( "srlw    %s, %s, %s\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ) );
                }
                else if ( 1 == funct7 )
                {
                    if ( 0 == funct3 )
                    {
                        uint32_t x = ( 0xffffffff & regs[ rs1 ] ) * ( 0xffffffff & regs[ rs2 ] );
                        uint64_t ext = sign_extend( x, 31 );
                        tracer.Trace( "mulw    %s, %s, %s  # %d * %d = %lld\n", reg_name( rd ), reg_name( rs1 ), reg_name( rs2 ), regs[ rs1 ], regs[ rs2 ], ext );
                    }
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
            else if ( 0x10 == opcode_type )
            {
                uint32_t rs3 = ( ( funct7 >> 2 ) & 0x1f );
                uint32_t fmt = ( funct7 & 0x3 );

                if ( 0 == fmt )
                {
                    float result = ( fregs[ rs1 ].f * fregs[ rs2 ].f ) + fregs[ rs3 ].f;
                    tracer.Trace( "fmadd.s  %s, %s, %s, %s  # %.2f = %.2f * %.2f + %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ), freg_name( rs3 ),
                                  result, fregs[ rs1 ].f, fregs[ rs2 ].f, fregs[ rs3 ].f );
                }
                else if ( 1 == fmt )
                {
                    double result = ( fregs[ rs1 ].d * fregs[ rs2 ].d ) + fregs[ rs3 ].d;
                    tracer.Trace( "fmadd.d  %s, %s, %s, %s  # %.2f = %.2f * %.2f + %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ), freg_name( rs3 ),
                                  result, fregs[ rs1 ].d, fregs[ rs2 ].d, fregs[ rs3 ].d );
                }
            }
            else if ( 0x11 == opcode_type )
            {
                uint32_t rs3 = ( ( funct7 >> 2 ) & 0x1f );
                uint32_t fmt = ( funct7 & 0x3 );

                if ( 0 == fmt )
                {
                    float result = ( fregs[ rs1 ].f * fregs[ rs2 ].f ) - fregs[ rs3 ].f;
                    tracer.Trace( "fmsub.s  %s, %s, %s, %s  # %.2f = %.2f * %.2f - %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ), freg_name( rs3 ),
                                  result, fregs[ rs1 ].f, fregs[ rs2 ].f, fregs[ rs3 ].f );
                }
                else if ( 1 == fmt )
                {
                    double result = ( fregs[ rs1 ].d * fregs[ rs2 ].d ) - fregs[ rs3 ].d;
                    tracer.Trace( "fmsub.d  %s, %s, %s, %s  # %.2f = %.2f * %.2f - %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ), freg_name( rs3 ),
                                  result, fregs[ rs1 ].d, fregs[ rs2 ].d, fregs[ rs3 ].d );                        
                }
            }
            else if ( 0x12 == opcode_type ) 
            {
                uint32_t rs3 = ( ( funct7 >> 2 ) & 0x1f );
                uint32_t fmt = ( funct7 & 0x3 );

                // fnmsub == add rs3

                if ( 0 == fmt )
                {
                    float result = ( -1.0f * ( fregs[ rs1 ].f * fregs[ rs2 ].f ) ) + fregs[ rs3 ].f;
                    tracer.Trace( "fnmsub.s  %s, %s, %s, %s  # %.2f = %.2f * %.2f + %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ), freg_name( rs3 ),
                                  result, fregs[ rs1 ].f, fregs[ rs2 ].f, fregs[ rs3 ].f );                        
                }
                else if ( 1 == fmt )
                {
                    double result = ( -1.0 * ( fregs[ rs1 ].d * fregs[ rs2 ].d ) ) + fregs[ rs3 ].d;
                    tracer.Trace( "fnmsub.d  %s, %s, %s, %s  # %.2f = %.2f * %.2f + %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ), freg_name( rs3 ),
                                  result, fregs[ rs1 ].d, fregs[ rs2 ].d, fregs[ rs3 ].d );                        
                }
            }
            else if ( 0x13 == opcode_type ) 
            {
                uint32_t rs3 = ( ( funct7 >> 2 ) & 0x1f );
                uint32_t fmt = ( funct7 & 0x3 );

                // fnmsub == subtract rs3

                if ( 0 == fmt )
                {
                    float result = ( -1.0f * ( fregs[ rs1 ].f * fregs[ rs2 ].f ) ) - fregs[ rs3 ].f;
                    tracer.Trace( "fnmadd.s  %s, %s, %s, %s  # %.2f = %.2f * %.2f - %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ), freg_name( rs3 ),
                                  result, fregs[ rs1 ].f, fregs[ rs2 ].f, fregs[ rs3 ].f );                        
                }
                else if ( 1 == fmt )
                {
                    double result = ( -1.0 * ( fregs[ rs1 ].d * fregs[ rs2 ].d ) ) - fregs[ rs3 ].d;
                    tracer.Trace( "fnmadd.d  %s, %s, %s, %s  # %.2f = %.2f * %.2f - %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ), freg_name( rs3 ),
                                  result, fregs[ rs1 ].d, fregs[ rs2 ].d, fregs[ rs3 ].d );                        
                }
            }
            else if ( 0x14 == opcode_type )
            {
                if ( 0 == funct7 )
                    tracer.Trace( "fadd.s  %s, %s, %s  # %.2f = %.2f + %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ),
                                  fregs[ rs1 ].f + fregs[ rs2 ].f, fregs[ rs1 ].f, fregs[ rs2 ].f );
                else if ( 1 == funct7 )
                    tracer.Trace( "fadd.d  %s, %s, %s  # %.2f = %.2f + %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ),
                                  fregs[ rs1 ].d + fregs[ rs2 ].d, fregs[ rs1 ].d, fregs[ rs2 ].d );
                else if ( 4 == funct7 )
                    tracer.Trace( "fsub.s  %s, %s, %s  # %.2f = %.2f - %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ),
                                   fregs[ rs1 ].f - fregs[ rs2 ].f, fregs[ rs1 ].f, fregs[ rs2 ].f );
                else if ( 5 == funct7 )
                    tracer.Trace( "fsub.d  %s, %s, %s  # %.2f = %.2f - %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ),
                                   fregs[ rs1 ].d - fregs[ rs2 ].d, fregs[ rs1 ].d, fregs[ rs2 ].d );
                else if ( 8 == funct7 )
                    tracer.Trace( "fmul.s  %s, %s, %s\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) );
                else if ( 9 == funct7 )
                    tracer.Trace( "fmul.d  %s, %s, %s  # %.2f = %.2f * %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ),
                                  fregs[ rs1 ].d * fregs[ rs2 ].d, fregs[ rs1 ].d, fregs[ rs2 ].d );
                else if ( 0xc == funct7 )
                    tracer.Trace( "fdiv.s  %s, %s, %s  # %.2f == %.2f / %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ),
                                  fregs[ rs1 ].f / fregs[ rs2 ].f, fregs[ rs1 ].f, fregs[ rs2 ].f );
                else if ( 0xd == funct7 )
                    tracer.Trace( "fdiv.d  %s, %s, %s  # %.2f == %.2f / %.2f\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ),
                                  fregs[ rs1 ].d / fregs[ rs2 ].d, fregs[ rs1 ].d, fregs[ rs2 ].d );
                else if ( 0x10 == funct7 )
                {
                    if ( 0 == funct3 )
                    {
                        // put rs1's absolute value in rd and use rs2's sign bit

                        float f = fabsf( fregs[ rs1 ].f );
                        if ( fregs[ rs2 ].f < 0.0 )
                            f = -f;
                        tracer.Trace( "fsgnj.s %s, %s, %s  # %.2f\n", freg_name( rd), freg_name( rs1) , freg_name( rs2 ), f );
                    }
                    else if ( 1 == funct3 )
                    {
                        // put rs1's absolute value in rd and use opposeite of rs2's sign bit

                        float f = fabsf( fregs[ rs1 ].f );
                        if ( fregs[ rs2 ].f >= 0.0 )
                            f = -f;
                        tracer.Trace( "fsgnjn.s %s, %s, %s  # %.2f\n", freg_name( rd), freg_name( rs1) , freg_name( rs2 ), f );
                    }
                    else if ( 2 == funct3 )
                    {
                        // put rs1's absolute value in rd and use the xor of the two sign bits

                        bool resultneg = ( ( fregs[ rs1 ].f < 0.0 ) ^ ( fregs[ rs2 ].f < 0.0 ) );
                        float f = fabsf( fregs[ rs1 ].f );
                        if ( resultneg )
                            f = -f;
                        tracer.Trace( "fsgnjnx.s %s, %s, %s  # %.2f\n", freg_name( rd), freg_name( rs1) , freg_name( rs2 ), f );
                    }
                }
                else if ( 0x11 == funct7 )
                {
                    if ( 0 == funct3 )
                    {
                        // put rs1's absolute value in rd and use rs2's sign bit

                        double d = fabs( fregs[ rs1 ].d );
                        if ( fregs[ rs2 ].d < 0.0 )
                            d = -d;
                        tracer.Trace( "fsgnj.d %s, %s, %s  # %.2f\n", freg_name( rd), freg_name( rs1) , freg_name( rs2 ), d );
                    }
                    else if ( 1 == funct3 )
                    {
                        // put rs1's absolute value in rd and use opposeite of rs2's sign bit

                        double d = fabs( fregs[ rs1 ].d );
                        if ( fregs[ rs2 ].d >= 0.0 )
                            d = -d;
                        tracer.Trace( "fsgnjn.d %s, %s, %s  # %.2f\n", freg_name( rd), freg_name( rs1) , freg_name( rs2 ), d );
                    }
                    else if ( 2 == funct3 )
                    {
                        // put rs1's absolute value in rd and use the xor of the two sign bits

                        bool resultneg = ( ( fregs[ rs1 ].d < 0.0 ) ^ ( fregs[ rs2 ].d < 0.0 ) );
                        double d = fabs( fregs[ rs1 ].d );
                        if ( resultneg )
                            d = -d;
                        tracer.Trace( "fsgnjnx.d %s, %s, %s  # %.2f\n", freg_name( rd ), freg_name( rs1 ) , freg_name( rs2 ), d );
                    }
                }
                else if ( 0x14 == funct7 )
                {
                    if ( 0 == funct3 )
                        tracer.Trace( "fmin.s %s, %s, %s\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) );
                    else if ( 1 == funct3 )
                        tracer.Trace( "fmin.s %s, %s, %s\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) );
                }
                else if ( 0x15 == funct7 )
                {
                    if ( 0 == funct3 )
                        tracer.Trace( "fmin.d %s, %s, %s\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) );
                    else if ( 1 == funct3 )
                        tracer.Trace( "fmin.d %s, %s, %s\n", freg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) );
                }
                else if ( 0x20 == funct7 )
                {
                    if ( 1 == rs2 )
                        tracer.Trace( "fcvt.s.d %s, %s\n", freg_name( rd ), freg_name( rs1 ) ); // convert double to single
                }                
                else if ( 0x21 == funct7 )
                {
                    if ( 0 == rs2 )
                        tracer.Trace( "fcvt.d.s %s, %s\n", freg_name( rd ), freg_name( rs1 ) ); // convert single to double
                }                
                else if ( 0x2c == funct7 )
                {
                    if ( 0 == rs2 )
                        tracer.Trace( "fsqrt.s %s, %s\n", freg_name( rd ), freg_name( rs1 ) );
                }
                else if ( 0x2d == funct7 )
                {
                    if ( 0 == rs2 )
                        tracer.Trace( "fsqrt.d %s, %s\n", freg_name( rd ), freg_name( rs1 ) );
                }
                else if ( 0x50 == funct7 )
                {
                    if ( 0 == funct3 )
                        tracer.Trace( "fle.s %s, %s, %s\n", reg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) ); // float less than or equal
                    else if ( 1 == funct3 )
                        tracer.Trace( "flt.s %s, %s, %s\n", reg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) ); // float less than
                    else if ( 2 == funct3 )
                        tracer.Trace( "feq.s %s, %s, %s\n", reg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) ); // float eq
                }
                else if ( 0x51 == funct7 )
                {
                    if ( 0 == funct3 )
                        tracer.Trace( "fle.d %s, %s, %s\n", reg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) ); // double less than or equal
                    else if ( 1 == funct3 )
                        tracer.Trace( "flt.d %s, %s, %s\n", reg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) ); // double less than
                    else if ( 2 == funct3 )
                        tracer.Trace( "feq.d %s, %s, %s\n", reg_name( rd ), freg_name( rs1 ), freg_name( rs2 ) ); // double eq
                }                
                else if ( 0x60 == funct7 )
                {
                    if ( 0 == rs2 )
                        tracer.Trace( "fcvt.w.s %s, %s  # %ld = %.2f\n", reg_name( rd ), freg_name( rs1 ), (int32_t) fregs[ rs1 ].f, fregs[ rs1 ].f ); // float to i32
                    else if ( 1 == rs2 )
                        tracer.Trace( "fcvt.wu.s %s, %s  # %lu = %.2f\n", reg_name( rd ), freg_name( rs1 ), (uint32_t) fregs[ rs1 ].f, fregs[ rs1 ].f ); // float to ui32
                    else if ( 2 == rs2 )
                        tracer.Trace( "fcvt.l.s %s, %s  # %lld = %.2f\n", reg_name( rd ), freg_name( rs1 ), (int64_t) fregs[ rs1 ].f, fregs[ rs1 ].f ); // float to i64
                    else if ( 3 == rs2 )
                        tracer.Trace( "fcvt.lu.s %s, %s  # %llu = %.2f\n", reg_name( rd ), freg_name( rs1 ), (uint64_t) fregs[ rs1 ].f, fregs[ rs1 ].f ); // float to ui64
                }
                else if ( 0x61 == funct7 )
                {
                    if ( 0 == rs2 )
                        tracer.Trace( "fcvt.w.d %s, %s  # %lld = %.2f\n", reg_name( rd ), freg_name( rs1 ), (int64_t) fregs[ rs1 ].d, fregs[ rs1 ].d ); // double to i32
                    else if ( 1 == rs2 )
                        tracer.Trace( "fcvt.wu.d %s, %s  # %llu = %.2f\n", reg_name( rd ), freg_name( rs1 ), (uint64_t) fregs[ rs1 ].d, fregs[ rs1 ].d ); // double to ui32
                    else if ( 2 == rs2 )
                        tracer.Trace( "fcvt.l.d %s, %s  # %lld = %.2f\n", reg_name( rd ), freg_name( rs1 ), (int64_t) fregs[ rs1 ].d, fregs[ rs1 ].d ); // double to i64
                    else if ( 3 == rs2 )
                        tracer.Trace( "fcvt.lu.d %s, %s  # %llu = %.2f\n", reg_name( rd ), freg_name( rs1 ), (uint64_t) fregs[ rs1 ].d, fregs[ rs1 ].d ); // double to ui64
                }                
                else if ( 0x68 == funct7 )
                {
                    if ( 0 == rs2 )
                        tracer.Trace( "fcvt.s.w %s, %s  # %.2f = %d\n", freg_name( rd ), reg_name( rs1 ), (float) regs[ rs1 ], (int32_t) regs[ rs1 ] ); // i32 to float
                    else if ( 1 == rs2 )
                        tracer.Trace( "fcvt.s.wu %s, %s  # %.2f = %u\n", freg_name( rd ), reg_name( rs1 ), (float) regs[ rs1 ], (uint32_t) regs[ rs1 ] ); // u32 to float
                    else if ( 2 == rs2 )
                        tracer.Trace( "fcvt.s.l %s, %s  # %.2f = %lld\n", freg_name( rd ), reg_name( rs1 ), (float) regs[ rs1 ], regs[ rs1 ] ); // i64 to float
                    else if ( 3 == rs2 )
                        tracer.Trace( "fcvt.s.lu %s, %s  # %.2f = %llu\n", freg_name( rd ), reg_name( rs1 ), (float) regs[ rs1 ], regs[ rs1 ] ); // ui64 to float
                }
                else if ( 0x69 == funct7 )
                {
                    if ( 0 == rs2 )
                        tracer.Trace( "fcvt.d.w %s, %s  # %ld = %.2f\n", freg_name( rd ), reg_name( rs1 ), (int32_t) regs[ rs1 ], (double) (int32_t) regs[ rs1 ] );
                    else if ( 1 == rs2 )
                        tracer.Trace( "fcvt.d.wu %s, %s  # %lu = %.2f\n", freg_name( rd ), reg_name( rs1 ), (uint32_t) regs[ rs1 ], (double) (int32_t) regs[ rs1 ] );
                    else if ( 2 == rs2 )
                        tracer.Trace( "fcvt.d.l %s, %s  # %lld = %.2f\n", freg_name( rd ), reg_name( rs1 ), (int64_t) regs[ rs1 ], (double) (int64_t) regs[ rs1 ] );
                    else if ( 3 == rs2 )
                        tracer.Trace( "fcvt.d.lu %s, %s  # %llu = %.2f\n", freg_name( rd ), reg_name( rs1 ), (uint64_t) regs[ rs1 ], (double) (int64_t) regs[ rs1 ] );
                }
                else if ( 0x70 == funct7 )
                {
                    if ( 0 == rs2 )
                    {
                        if ( 0 == funct3 )
                            tracer.Trace( "fmv.x.w %s, %s  # %.2f\n", reg_name( rd ), freg_name( rs1 ), fregs[ rs1 ].f );
                        else if ( 1 == funct3 )
                            tracer.Trace( "fclass.s %s, %s  # %.2f\n", reg_name( rd ), freg_name( rs1 ), fregs[ rs1 ].f );
                     }
                }
                else if ( 0x71 == funct7 )
                {
                    if ( 0 == rs2 && 0 == funct3 )
                        tracer.Trace( "fmv.x.d %s, %s  # %.2f\n", reg_name( rd ), freg_name( rs1 ), fregs[ rs1 ].d );
                }
                else if ( 0x78 == funct7 )
                {
                    if ( 0 == rs2 && 0 == funct3 )
                    {
                        float f;
                        memcpy( &f, & regs[ rs1 ], 4 );
                        tracer.Trace( "fmv.w.x %s, %s  # %x = %.2f\n", freg_name( rd ), reg_name( rs1 ), regs[ rs1 ], f ); // copy ui32 to float as-is bit for bit
                    }
                }
                else if ( 0x79 == funct7 )
                {
                    if ( 0 == rs2 && 0 == funct3 )
                    {
                        double d;
                        memcpy( &d, & regs[ rs1 ], 8 );
                        tracer.Trace( "fmv.d.x %s, %s  # %x = %.2f\n", freg_name( rd ), reg_name( rs1 ), regs[ rs1 ], d ); // copy ui64 to double as-is bit for bit
                    }
                }
            }
            break;
        }
#if USE_DJL_RISCV_EXTENSIONS
        case CType: // risc-v extension cmvxx instructions
        {
            decode_C();

            int64_t rs1_imm = sign_extend( rs1, 4 );
            int64_t rc2_imm = sign_extend( c_rc2, 4 );

            if ( 0 == c_imm_flags )
                tracer.Trace( "cmv%s %s, %s, %s, %s\n", cmp_type( funct3 ), reg_name( rd ), reg_name( rs1 ), reg_name( c_rc1 ), reg_name( c_rc2 ) );
            else if ( 1 == c_imm_flags )
                tracer.Trace( "cmv%s %s, %lld, %s, %s\n", cmp_type( funct3 ), reg_name( rd ), rs1_imm, reg_name( c_rc1 ), reg_name( c_rc2 ) );
            else if ( 2 == c_imm_flags )
            {
                if ( funct3 >= 6 )
                    tracer.Trace( "cmv%s %s, %s, %s, %lud\n", cmp_type( funct3 ), reg_name( rd ), reg_name( rs1 ), reg_name( c_rc1 ), c_rc2 );
                else
                    tracer.Trace( "cmv%s %s, %s, %s, %lld\n", cmp_type( funct3 ), reg_name( rd ), reg_name( rs1 ), reg_name( c_rc1 ), rc2_imm );
            }
            else // ( 3 == c_imm_flags )
            {
                if ( funct3 >= 6 )
                    tracer.Trace( "cmv%s %s, %lld, %s, %llu\n", cmp_type( funct3 ), reg_name( rd ), rs1_imm, reg_name( c_rc1 ), c_rc2 );
                else
                    tracer.Trace( "cmv%s %s, %lld, %s, %lld\n", cmp_type( funct3 ), reg_name( rd ), rs1_imm, reg_name( c_rc1 ), rc2_imm );
            }
            
            break;
        }
#else
        case CType: // generic risc-v extensions
        {
            tracer.Trace( "unknown risc-v extension\n" );
            break;
        }
#endif
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

#ifdef _WIN32
__declspec(noinline)
#endif
void RiscV::unhandled()
{
    printf( "unhandled op %llx optype %llx == %c\n", op, opcode_type, instruction_types[ riscv_types[ opcode_type ] ] );
    tracer.Trace( "unhandled op %llx optype %llx == %c\n", op, opcode_type, instruction_types[ riscv_types[ opcode_type ] ] );
    riscv_hard_termination( *this, "opcode not handed:", op );
} //unhandled

uint64_t RiscV::run( uint64_t max_cycles )
{
    uint64_t start_cycles = cycles_so_far;
    uint64_t target_cycles = cycles_so_far + max_cycles;

    do
    {
        #ifndef NDEBUG
            if ( 0 != regs[ 0 ] )
                riscv_hard_termination( *this, "zero register isn't 0:", regs[ zero ] );

            if ( regs[ sp ] <= ( stack_top - stack_size ) )
                riscv_hard_termination( *this, "stack pointer is below stack memory:", regs[ sp ] );

            if ( regs[ sp ] > stack_top )
                riscv_hard_termination( *this, "stack pointer is above the top of its starting point:", regs[ sp ] );

            if ( pc < base )
                riscv_hard_termination( *this, "pc is lower than memory:", pc );

            if ( pc >= ( base + mem_size - stack_size ) )
                riscv_hard_termination( *this, "pc is higher than it should be:", pc );

            if ( 0 != ( regs[ sp ] & 0xf ) ) // by convention, risc-v stacks are 16-byte aligned
                riscv_hard_termination( *this, "the stack pointer isn't 16-byte aligned:", regs[ sp ] );
        #endif

        uint64_t pcnext = decode();   // 18% of runtime

        if ( 0 != g_State )           // 1.1% of runtime
        {
            if ( g_State & stateEndEmulation )
            {
                g_State &= ~stateEndEmulation;
                break;
            }

            if ( g_State & stateTraceInstructions )
                trace_state();
        }

        switch( opcode_type )         // 18.5% of runtime setting up for the jump table
        {
            case 0:
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
            case 1:
            {
                assert_type( IType );
                decode_I();
    
                if ( 2 == funct3 ) // flw rd, i_imm(rs1)
                    fregs[ rd ].f = getfloat( regs[ rs1 ] + i_imm );
                else if ( 3 == funct3 ) // fld rd, i_imm(rs1)
                    fregs[ rd ].d = getdouble( regs[ rs1 ] + i_imm );
                else
                    unhandled();
                break;
            }
#if USE_DJL_RISCV_EXTENSIONS
            case 2: // risc-v extension cmvxx conditional move
            {
                assert_type( CType );
                decode_C();
    
                if ( 0 == rd )
                    break;
    
                // cmvXX are risc-v extension conditional-move instructions. They yield about 1% faster perf for some benchmarks
                // cmvXX rd, rs1, rc1, rc2  -- if ( rc1 XX rc2 ) rd = rs1
                // 31: 1 if rc2 is an immediate value (signed/unsigned depending on the compare) or 0 if a register
                // 30: 1 if rs1 is an immediate value (always signed) or 0 if a register
                // 29-25: rc1
                // 24-20: rc2
                // 19-15: rs1
                // 14-12: funct3 XX comparison 0 = eq, 1 = ne, 4 = lt, 5 = ge, 6 = ltu, 7 = geu
                // 11-7:  rd
                // 6-2:   2 (opcode type cmv)
                // 1-0:   3 (4-byte instruction)

                uint64_t source = ( 0 == ( c_imm_flags & 1 ) ) ? regs[ rs1 ] : sign_extend( rs1, 4 );
                uint64_t cmp_right = ( 0 == ( c_imm_flags & 2 ) ) ? regs[ c_rc2 ] : ( 6 == funct3 || 7 == funct3 ) ? c_rc2 : sign_extend( c_rc2, 4 );

                if ( 0 == funct3 ) // cmveq
                {
                    if ( regs[ c_rc1 ] == cmp_right )
                        regs[ rd ] = source;
                }
                else if ( 1 == funct3 ) // cmvne
                {
                    if ( regs[ c_rc1 ] != cmp_right )
                        regs[ rd ] = source;
                }
                else if ( 4 == funct3 ) // cmvlt
                {
                    if ( (int64_t) regs[ c_rc1 ] < (int64_t) cmp_right )
                        regs[ rd ] = source;
                }
                else if ( 5 == funct3 ) // cmvge
                {
                    if ( (int64_t) regs[ c_rc1 ] >= (int64_t) cmp_right )
                        regs[ rd ] = source;
                }
                else if ( 6 == funct3 ) // cmvltu
                {
                    if ( regs[ c_rc1 ] >= cmp_right )
                        regs[ rd ] = source;
                }
                else if ( 7 == funct3 ) // cmvgtu
                {
                    if ( regs[ c_rc1 ] >= cmp_right )
                        regs[ rd ] = source;
                }
                else
                    unhandled();
                
                break;
            }
#else
            case 2: //risc-v extension instruction.
            {
                // The rust compiler for risc-v uses these instructions in its runtime and I don't know what they're for.
                // The instructions modify t4, a5, a3, t2, and t1 but can be ignored and the app runs ok.
                // Specifically: 0x01a0000b, 0x0244000b, 0x0245800b, 0x02460000b, 0x0247000b, and 0x0249800b
                // are used when setting up and running libc exit handlers.
                // This should probably just exit rvos instead.

                break;
            }
#endif
            case 3:
            {
                assert_type( IType );
                decode_I();
    
                if ( 0 == funct3 || 1 == funct3 ) // fence or fence.i
                {
                    // fence -- do nothing
                }
                else
                    unhandled();
                break;
            }
            case 4:
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
                    {
                        // The old g++ for RISC-V doesn't sign extend on right shifts for signed integers.
                        // The new g++ does for amd64 code gen. work around this.
    
                        uint64_t result = regs[ rs1 ] >> i_shamt6;
                        regs[ rd ] = sign_extend( result, 63 - i_shamt6 );
                    }
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
            case 5:
            {
                assert_type( UType );
                decode_U();
                if ( 0 == rd )
                    break;
    
                // auipc imm.    rd <= pc + ( imm << 12 )
    
                regs[ rd ] = pc + ( u_imm << 12 );
                break;
            }
            case 6:
            {
                assert_type( IType );
                decode_I();
                decode_I_shift();
                if ( 0 == rd )
                    break;
    
                if ( 0 == funct3 ) // addiw rd, rs1, i_imm  (sign-extend both i_imm and rd)
                {
                    int32_t val = 0xffffffff & regs[ rs1 ];
                    int32_t imm = (int32_t) i_imm;
                    int32_t result = val + imm;
                    regs[ rd ] = sign_extend( (uint32_t) result, 31 );
                }
                else if ( 1 == funct3 )
                {
                    // 5, not 6 per https://riscv.org/wp-content/uploads/2019/06/riscv-spec.pdf
                    if ( 0 == i_top2 )
                    {
                        uint32_t val = (uint32_t) regs[ rs1 ];
                        uint32_t result = val << i_shamt5;
                        uint64_t result64 = sign_extend( result, 31 );
                        regs[ rd ] = result64;  // slliw rd, rs1, i_shamt5
                    }
                    else
                        unhandled();
                }
                else if ( 5 == funct3 )
                {
                    if ( 0 == i_top2 )
                        regs[ rd ] = ( ( 0xffffffff & regs[ rs1 ] ) >> i_shamt5 ); // srliw rd, rs1, i_imm
                    else if ( 1 == i_top2 )
                    {
                        // the old g++ compiler that targets RISC-V doesn't sign-extend right shifts on signed numbers.
                        // msvc and the g++ compiler that targets AMD64 both do sign-extend right shifts on signed numbers
                        // work around this by manually sign extending the result.
    
                        uint32_t t = regs[ rs1 ] & 0xffffffff;
                        uint64_t result = sign_extend( t >> i_shamt5, 31 - i_shamt5 );
                        regs[ rd ] = result; // sraiw rd, rs1, i_imm
                    }
                    else
                        unhandled();
                }
                else
                    unhandled();
                break;
            }
            case 8:
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
            case 9:
            {
                assert_type( SType );
                decode_S();
    
                if ( 2 == funct3 ) // fsw   rs2, imm(rs1)
                    setfloat( regs[ rs1 ] + s_imm, fregs[ rs2 ].f );
                else if ( 3 == funct3 ) // fsd  rs2, imm(rs1)
                    setdouble( regs[ rs1 ] + s_imm, fregs[ rs2 ].d );
                else
                    unhandled();
                break;
            }
#if USE_DJL_RISCV_EXTENSIONS
            case 0xa:
            {
                assert_type( RType );
                decode_R();

                // conditional return. If the condition is true, jump to the address in rreturn (typically ra)
                // jrXX rleft, rright, rreturn
                // if ( rleft XX rright ) pc = rreturn
                // R-type instruction
                //    rs1 -- rleft
                //    rs2 -- rright
                //    rd -- typically ra
                //    funct3 -- 0 = eq, 1 = ne, 4 = lt, 5 = ge, 6 = ltu, 7 = gtu
                //    funct7 -- 0
                //    opcode -- lower 7 bits 0x2b. opcode type -- 0xa

                if ( 0 == funct7 )
                {
                    if ( 0 == funct3 ) // jreq
                    {
                        if ( regs[ rs1 ] == regs[ rs2 ] )
                            pcnext = regs[ rd ];
                    }
                    else if ( 1 == funct3 ) // jrne
                    {
                        if ( regs[ rs1 ] != regs[ rs2 ] )
                            pcnext = regs[ rd ];
                    }
                    else if ( 4 == funct3 ) // jrlt
                    {
                        if ( (int64_t) regs[ rs1 ] < (int64_t) regs[ rs2 ] )
                            pcnext = regs[ rd ];
                    }
                    else if ( 5 == funct3 ) // jrge
                    {
                        if ( (int64_t) regs[ rs1 ] >= (int64_t) regs[ rs2 ] )
                            pcnext = regs[ rd ];
                    }
                    else if ( 6 == funct3 ) // jrltu
                    {
                        if ( regs[ rs1 ] < regs[ rs2 ] )
                            pcnext = regs[ rd ];
                    }
                    else if ( 7 == funct3 ) // jrgtu
                    {
                        if ( regs[ rs1 ] > regs[ rs2 ] )
                            pcnext = regs[ rd ];
                    }
                    else
                        unhandled();
                }
                else
                    unhandled();
                
                break;
            }
#endif
            case 0xb:
            {
                assert_type( RType );
                decode_R();
    
                uint32_t top5 = (uint32_t) ( funct7 >> 2 );

                // with just one hart in this emulator, these can be ignored
                //bool aq = ( 0 != ( 2 & funct7 ) );
                //bool rl = ( 0 != ( 1 & funct7 ) );
    
                if ( 0 == top5 )
                {
                    if ( 2 == funct3 ) // amoadd.w rd, rs2, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        setui32( regs[ rs1 ], (uint32_t) ( regs[ rs2 ] + memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 );
                    }
                    else if ( 3 == funct3 ) // amoadd.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        setui64( regs[ rs1 ], regs[ rs2 ] + memval );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 1 == top5 )
                {
                    if ( 2 == funct3 ) // amoswap.w rd, rs2, (rs1)
                    {
                        uint64_t memval = sign_extend( getui32( regs[ rs1 ] ), 31 );
                        uint32_t regval = (uint32_t) regs[ rs2 ];
                        setui32( regs[ rs1 ], regval );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else if ( 3 == funct3 ) // amoswap.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        uint64_t regval = regs[ rs2 ];
                        setui64( regs[ rs1 ], regval );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 2 == top5 )
                {
                    if ( 2 == funct3 ) // lr.w rd, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 );
                    }
                    else if ( 3 == funct3 ) // lr.d rd, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 3 == top5 )
                {
                    if ( 2 == funct3 ) // sc.w rd, rs2, (rs1)
                    {
                        setui32( regs[ rs1 ], (uint32_t) regs[ rs2 ] );
                        if ( 0 != rd )
                            regs[ rd ] = 0;
                    }
                    else if ( 3 == funct3 ) // sc.d rd, rs2, (rs1)
                    {
                        setui64( regs[ rs1 ], regs[ rs2 ] );
                        if ( 0 != rd )
                            regs[ rd ] = 0;
                    }
                    else
                        unhandled();
                }
                else if ( 4 == top5 )
                {
                    if ( 2 == funct3 ) // amoxor.w rd, rs2, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        setui32( regs[ rs1 ], (uint32_t) ( regs[ rs2 ] ^ memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 );
                    }
                    else if ( 3 == funct3 ) // amoxor.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        setui64( regs[ rs1 ], regs[ rs2 ] ^ memval );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 8 == top5 )
                {
                    if ( 2 == funct3 ) // amoor.w rd, rs2, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        setui32( regs[ rs1 ], (uint32_t) ( regs[ rs2 ] | memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 );
                    }
                    else if ( 3 == funct3 ) // amoor.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        setui64( regs[ rs1 ], regs[ rs2 ] | memval );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 0xc == top5 )
                {
                    if ( 2 == funct3 ) // amoand.w rd, rs2, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        setui32( regs[ rs1 ], regs[ rs2 ] & memval );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 );
                    }
                    else if ( 3 == funct3 ) // amoand.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        setui64( regs[ rs1 ], regs[ rs2 ] & memval );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 0x10 == top5 )
                {
                    if ( 2 == funct3 ) // amomin.w rd, rs2, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        setui32( regs[ rs1 ], get_min( (int32_t) regs[ rs2 ], (int32_t) memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 );
                    }
                    else if ( 3 == funct3 ) // amomin.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        setui64( regs[ rs1 ], get_min( (int64_t) regs[ rs2 ], (int64_t) memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 0x14 == top5 )
                {
                    if ( 2 == funct3 ) // amomax.w rd, rs2, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        setui32( regs[ rs1 ], get_max( (int32_t) regs[ rs2 ], (int32_t) memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 );
                    }
                    else if ( 3 == funct3 ) // amomax.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        setui64( regs[ rs1 ], get_max( (int64_t) regs[ rs2 ], (int64_t) memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 0x18 == top5 )
                {
                    if ( 2 == funct3 ) // amominu.w rd, rs2, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        setui32( regs[ rs1 ], get_min( (uint32_t) regs[ rs2 ], memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 ); // AMOs always sign-extend value placed in rd
                    }
                    else if ( 3 == funct3 ) // amominu.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        setui64( regs[ rs1 ], get_min( regs[ rs2 ], memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
                else if ( 0x1c == top5 )
                {
                    if ( 2 == funct3 ) // amomaxu.w rd, rs2, (rs1)
                    {
                        uint32_t memval = getui32( regs[ rs1 ] );
                        setui32( regs[ rs1 ], get_max( (uint32_t) regs[ rs2 ], memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = sign_extend( memval, 31 ); // AMOs always sign-extend value placed in rd
                    }
                    else if ( 3 == funct3 ) // amomaxu.d rd, rs2, (rs1)
                    {
                        uint64_t memval = getui64( regs[ rs1 ] );
                        setui64( regs[ rs1 ], get_max( regs[ rs2 ], memval ) );
                        if ( 0 != rd )
                            regs[ rd ] = memval;
                    }
                    else
                        unhandled();
                }
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
                        //  n.b. the risc-v spec says lower 5 bits of rs2. Gnu floating point code assumes lower 6 bits.
                        regs[ rd ] = ( regs[ rs1 ] << ( regs[ rs2 ] & 0x3f ) ); // sll rd, rs1, rs2  // slr rd, rs1, rs2
                    else if ( 2 == funct3 )
                        regs[ rd ] = ( (int64_t) regs[ rs1 ] < (int64_t) regs[ rs2 ] ); // slt rd, rs1, rs2
                    else if ( 3 == funct3 )
                        regs[ rd ] = ( regs[ rs1 ] < regs[ rs2 ] ); // sltu rd, rs1, rs2
                    else if ( 4 == funct3 )
                        regs[ rd ] = ( regs[ rs1 ] ^ regs[ rs2 ] ); // xor rd, rs1, rs2
                    else if ( 5 == funct3 )
                        //  n.b. the risc-v spec says lower 5 bits of rs2. Gnu floating point code assumes lower 6 bits.
                        regs[ rd ] = ( regs[ rs1 ] >> ( regs[ rs2 ] & 0x3f ) ); // slr rd, rs1, rs2
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
                    else if ( 1 == funct3 ) // mulh rd, rs1, rs2     signed * signed
                    {
                        int64_t high;
                        CMultiply128::mul_s64_s64( regs[ rs1 ], regs[ rs2 ], &high );
                        regs[ rd ] = high;
                    }
                    else if ( 2 == funct3 ) // mulhsu rd, rs1, rs2    signed rs1 * unsigned rs2
                    {
                        int64_t reg1 = regs[ rs1 ];
                        bool negative = ( reg1 < 0 );
                        uint64_t high;
                        CMultiply128::mul_u64_u64( regs[ rs1 ], regs[ rs2 ], &high );
                        int64_t result = (int64_t) high;
                        if ( negative )
                            result = -result;
                        regs[ rd ] = result;
                    }
                    else if ( 3 == funct3 ) // mulhu rd, rs1, rs2    unsigned * unsigned
                    {
                        uint64_t high;
                        CMultiply128::mul_u64_u64( regs[ rs1 ], regs[ rs2 ], &high );
                        regs[ rd ] = high;
                    }
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
                    {
                        uint64_t shift = ( 0x3f & regs[ rs2 ] );
                        uint64_t result = regs[ rs1 ] >> shift;
                        regs[ rd ] = sign_extend( result, 63 - shift ); // sra rd, rs1, rs2
                    }
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
                    {
                        int32_t val = (int32_t) ( 0xffffffff & regs[ rs1 ] ) + (int32_t) ( 0xffffffff & regs[ rs2 ] );
                        uint64_t result = sign_extend( (uint32_t) val, 31 );
                        regs[ rd ] = result; // addw rd, rs1, rs2
                    }
                    else if ( 1 == funct3 )
                    {
                        uint32_t val = (uint32_t) regs[ rs1 ];
                        uint32_t amount = 0x1f & regs[ rs2 ];
                        uint32_t result = val << amount;
                        uint64_t result64 = sign_extend( result, 31 );
                        regs[ rd ] = result64; // sllw rd, rs1, rs2
                    }
                    else if ( 5 == funct3 )
                        regs[ rd ] = ( 0xffffffff & regs[ rs1 ] ) >> ( 0x1f & regs[ rs2 ] ); // srlw rd, rs1, rs2
                    else
                        unhandled();
                }
                else if ( 1 == funct7 )
                {
                    if ( 0 == funct3 )
                    {
                        uint32_t x = ( 0xffffffff & regs[ rs1 ] ) * ( 0xffffffff & regs[ rs2 ] );
                        uint64_t ext = sign_extend( x, 31 );
                        regs[ rd ] = ext; // mulw rd, rs1, rs2
                    }
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
                    {
                        uint64_t shift = ( 0x1f & regs[ rs2 ] );
                        uint64_t result = ( 0xffffffff & regs[ rs1 ] ) >> shift;
                        result = sign_extend( result, 31 - shift );
                        regs[ rd ] = result; // sraw rd, rs1, rs2
                    }
                    else
                        unhandled();
                }
                else
                    unhandled();
                break;
            }
            case 0x10:
            {
                assert_type( RType );
                decode_R();
    
                uint32_t rs3 = ( ( funct7 >> 2 ) & 0x1f );
                uint32_t fmt = ( funct7 & 0x3 );
    
                if ( 0 == fmt )
                    fregs[ rd ].f = ( fregs[ rs1 ].f * fregs[ rs2 ].f ) + fregs[ rs3 ].f; // fmadd.s frd, frs1, frs2, frs3
                else if ( 1 == fmt )
                    fregs[ rd ].d = ( fregs[ rs1 ].d * fregs[ rs2 ].d ) + fregs[ rs3 ].d; // fmadd.d frd, frs1, frs2, frs3
                else
                    unhandled();
                break;
            }
            case 0x11:
            {
                assert_type( RType );
                decode_R();
    
                uint32_t rs3 = ( ( funct7 >> 2 ) & 0x1f );
                uint32_t fmt = ( funct7 & 0x3 );
    
                if ( 0 == fmt )
                    fregs[ rd ].f = ( fregs[ rs1 ].f * fregs[ rs2 ].f ) - fregs[ rs3 ].f; // fmsub.s frd, frs1, frs2, frs3
                else if ( 1 == fmt )
                    fregs[ rd ].d = ( fregs[ rs1 ].d * fregs[ rs2 ].d ) - fregs[ rs3 ].d; // fmsub.d frd, frs1, frs2, frs3
                else
                    unhandled();
                break;
            }
            case 0x12:
            {
                assert_type( RType );
                decode_R();
    
                uint32_t rs3 = ( ( funct7 >> 2 ) & 0x1f );
                uint32_t fmt = ( funct7 & 0x3 );
    
                if ( 0 == fmt )
                    fregs[ rd ].f = ( -1.0f * ( fregs[ rs1 ].f * fregs[ rs2 ].f ) ) + fregs[ rs3 ].f; // fnmsub.s frd, frs1, frs2, frs3
                else if ( 1 == fmt )
                    fregs[ rd ].d = ( -1.0 * ( fregs[ rs1 ].d * fregs[ rs2 ].d ) ) + fregs[ rs3 ].d; // fnmsub.d frd, frs1, frs2, frs3
                else
                    unhandled();
                break;
            }
            case 0x13:
            {
                assert_type( RType );
                decode_R();
    
                uint32_t rs3 = ( ( funct7 >> 2 ) & 0x1f );
                uint32_t fmt = ( funct7 & 0x3 );
    
                if ( 0 == fmt )
                    fregs[ rd ].f = ( -1.0f * ( fregs[ rs1 ].f * fregs[ rs2 ].f ) ) - fregs[ rs3 ].f; // fnmadd.s frd, frs1, frs2, frs3
                else if ( 1 == fmt )
                    fregs[ rd ].d = ( -1.0 * ( fregs[ rs1 ].d * fregs[ rs2 ].d ) ) - fregs[ rs3 ].d; // fnmadd.d frd, frs1, frs2, frs3
                else
                    unhandled();
                break;
            }
            case 0x14:
            {
                assert_type( RType );
                decode_R();
    
                if ( 0 == funct7 )
                    fregs[ rd ].f = fregs[ rs1 ].f + fregs[ rs2 ].f; // fadd.s frd, frs1, frs2
                else if ( 1 == funct7 )
                    fregs[ rd ].d = fregs[ rs1 ].d + fregs[ rs2 ].d; // fadd.d frd, frs1, frs2
                else if ( 4 == funct7 )
                    fregs[ rd ].f = fregs[ rs1 ].f - fregs[ rs2 ].f; // fsub.s frd, frs1, frs2
                else if ( 5 == funct7 )
                    fregs[ rd ].d = fregs[ rs1 ].d - fregs[ rs2 ].d; // fsub.d frd, frs1, frs2
                else if ( 8 == funct7 )
                    fregs[ rd ].f = fregs[ rs1 ].f * fregs[ rs2 ].f; // fmul.s frd, frs1, frs2
                else if ( 9 == funct7 )
                    fregs[ rd ].d = fregs[ rs1 ].d * fregs[ rs2 ].d; // fmul.d frd, frs1, frs2
                else if ( 0xc == funct7 )
                    fregs[ rd ].f = fregs[ rs1 ].f / fregs[ rs2 ].f; // fdiv.s frd, frs1, frs2
                else if ( 0xd == funct7 )
                    fregs[ rd ].d = fregs[ rs1 ].d / fregs[ rs2 ].d; // fdiv.d frd, frs1, frs2
                else if ( 0x10 == funct7 )
                {
                    if ( 0 == funct3 )
                    {
                        // put rs1's absolute value in rd and use rs2's sign bit
    
                        float f = fabsf( fregs[ rs1 ].f );
                        if ( fregs[ rs2 ].f < 0.0 )
                            f = -f;
                        fregs[ rd ].f = f; // fsgnj.s rd, rs1, rs2
                    }
                    else if ( 1 == funct3 )
                    {
                        // put rs1's absolute value in rd and use opposeite of rs2's sign bit
    
                        float f = fabsf( fregs[ rs1 ].f );
                        if ( fregs[ rs2 ].f >= 0.0 )
                            f = -f;
                        fregs[ rd ].f = f; // fsgnjn.s rd, rs1, rs2
                    }
                    else if ( 2 == funct3 )
                    {
                        // put rs1's absolute value in rd and use the xor of the two sign bits
    
                        bool resultneg = ( ( fregs[ rs1 ].f < 0.0) ^ ( fregs[ rs2 ].f < 0.0 ) );
                        float f = fabsf( fregs[ rs1 ].f );
                        if ( resultneg )
                            f = -f;
                        fregs[ rd ].f = f; // fsgnjnx.s rd, rs1, rs2
                    }
                    else
                        unhandled();
                }
                else if ( 0x11 == funct7 )
                {
                    if ( 0 == funct3 )
                    {
                        // put rs1's absolute value in rd and use rs2's sign bit
    
                        double d = fabs( fregs[ rs1 ].d );
                        if ( fregs[ rs2 ].d < 0.0 )
                            d = -d;
                        fregs[ rd ].d = d; // fsgnj.d rd, rs1, rs2
                    }
                    else if ( 1 == funct3 )
                    {
                        // put rs1's absolute value in rd and use opposeite of rs2's sign bit
    
                        double d = fabs( fregs[ rs1 ].d );
                        if ( fregs[ rs2 ].d >= 0.0 )
                            d = -d;
                        fregs[ rd ].d = d; // fsgnjn.d rd, rs1, rs2
                    }
                    else if ( 2 == funct3 )
                    {
                        // put rs1's absolute value in rd and use the xor of the two sign bits
    
                        bool resultneg = ( ( fregs[ rs1 ].d < 0.0) ^ ( fregs[ rs2 ].d < 0.0 ) );
                        double d = fabs( fregs[ rs1 ].d );
                        if ( resultneg )
                            d = -d;
                        fregs[ rd ].d = d; // fsgnjnx.d rd, rs1, rs2
                    }
                    else
                        unhandled();
                }
                else if ( 0x14 == funct7 )
                {
                    if ( 0 == funct3 )
                        fregs[ rd ].f = get_min( fregs[ rs1 ].f, fregs[ rs2 ].f ); // fmin.s rd, rs1, rs2
                    else if ( 1 == funct3 )
                        fregs[ rd ].f = get_max( fregs[ rs1 ].f, fregs[ rs2 ].f ); // fmax.s rd, rs1, rs2
                }
                else if ( 0x15 == funct7 )
                {
                    if ( 0 == funct3 )
                        fregs[ rd ].d = get_min( fregs[ rs1 ].d, fregs[ rs2 ].d ); // fmin.d rd, rs1, rs2
                    else if ( 1 == funct3 )
                        fregs[ rd ].d = get_max( fregs[ rs1 ].d, fregs[ rs2 ].d ); // fmax.d rd, rs1, rs2
                }
                else if ( 0x20 == funct7 )
                {
                    if ( 1 == rs2 )
                        fregs[ rd ].f = (float) fregs[ rs1 ].d; // fcvt.s.d rd, rs1
                    else
                        unhandled();
                }                
                else if ( 0x21 == funct7 )
                {
                    if ( 0 == rs2 )
                        fregs[ rd ].d = fregs[ rs1 ].f; // fcvt.d.s rd, rs1
                    else
                        unhandled();
                }     
                else if ( 0x2c == funct7 )
                {
                    if ( 0 == rs2 )
                        fregs[ rd ].f = (float) sqrt( fregs[ rs1 ].f ); // fsqrt.s frd, frs1
                    else
                        unhandled();
                }
                else if ( 0x2d == funct7 )
                {
                    if ( 0 == rs2 )
                        fregs[ rd ].d = sqrt( fregs[ rs1 ].d ); // fsqrt.d rd, rs1
                    else
                        unhandled();
                }
                else if ( 0x50 == funct7 )
                {
                    if ( 0 == funct3 )
                        regs[ rd ] = ( fregs[ rs1 ].f <= fregs[ rs2 ].f ); // fle.s rd, frs1, frs2
                    else if ( 1 == funct3 )
                        regs[ rd ] = ( fregs[ rs1 ].f < fregs[ rs2 ].f ); // flt.s rd, frs1, frs2
                    else if ( 2 == funct3 )
                        regs[ rd ] = ( fregs[ rs1 ].f == fregs[ rs2 ].f ); // feq.s rd, frs1, frs2
                    else
                        unhandled();
                }
                else if ( 0x51 == funct7 )
                {
                    if ( 0 == funct3 )
                        regs[ rd ] = ( fregs[ rs1 ].d <= fregs[ rs2 ].d ); // fle.d rd, frs1, frs2
                    else if ( 1 == funct3 )
                        regs[ rd ] = ( fregs[ rs1 ].d < fregs[ rs2 ].d ); // flt.d rd, frs1, frs2
                    else if ( 2 == funct3 )
                        regs[ rd ] = ( fregs[ rs1 ].d == fregs[ rs2 ].d ); // feq.d rd, frs1, frs2
                    else
                        unhandled();
                }            
                else if ( 0x60 == funct7 )
                {
                    if ( 0 == rs2 )
                        regs[ rd ] = (int32_t) fregs[ rs1 ].f; // fcvt.w.s rd, frs1
                    else if ( 1 == rs2 )
                        regs[ rd ] = (uint32_t) fregs[ rs1 ].f; // fcvt.wus rd, frs1
                    else if ( 2 == rs2 )
                        regs[ rd ] = (int64_t) fregs[ rs1 ].f; // fcvt.l.s rd, frs1
                    else if ( 3 == rs2 )
                        regs[ rd ] = (uint64_t) fregs[ rs1 ].f; // fcvt.lu.s rd, frs1
                    else
                        unhandled();
                }
                else if ( 0x61 == funct7 )
                {
                    if ( 0 == rs2 )
                        regs[ rd ] = (int32_t) fregs[ rs1 ].d; // fcvt.w.d rd, frs1
                    else if ( 1 == rs2 )
                        regs[ rd ] = (uint32_t) fregs[ rs1 ].d; // fcvt.wu.d rd, frs1
                    else if ( 2 == rs2 )
                        regs[ rd ] = (int64_t) fregs[ rs1 ].d; // fcvt.l.d rd, frs1
                    else if ( 3 == rs2 )
                        regs[ rd ] = (uint64_t) fregs[ rs1 ].d; // fcvt.lu.d rd, frs1
                    else
                        unhandled();
                }
                else if ( 0x68 == funct7 )
                {
                    if ( 0 == rs2 )
                        fregs[ rd ].f = (float) (int32_t) ( 0xffffffff & regs[ rs1 ] ); // fcvt.s.w frd, rs1   -- converts i32 to float
                    else if ( 1 == rs2 )
                        fregs[ rd ].f = (float) (uint32_t) ( 0xffffffff & regs[ rs1 ] ); // fcvt.s.wu frd, rs1   -- converts ui32 to float
                    else if ( 2 == rs2 )
                        fregs[ rd ].f = (float) (int64_t) regs[ rs1 ]; // fcvt.s.l frd, rs1   -- converts i64 to float
                    else if ( 3 == rs2 )
                        fregs[ rd ].f = (float) regs[ rs1 ]; // fcvt.s.lu frd, rs1   -- converts ui64 to float
                    else
                        unhandled();
                }
                else if ( 0x69 == funct7 )
                {
                    if ( 0 == rs2 ) // fcvt.d.w rd, rs1   -- converts i32 to double
                        fregs[ rd ].d = (double) (int32_t) regs[ rs1 ];
                    else if ( 1 == rs2 ) // fcvt.d.wu rd, rs1   -- converts ui32 to double
                        fregs[ rd ].d = (double) (uint32_t) regs[ rs1 ];
                    else if ( 2 == rs2 ) // fcvt.d.l rd, rs1   -- converts i64 to double
                        fregs[ rd ].d = (double) (int64_t) regs[ rs1 ];
                    else if ( 3 == rs2 ) // fcvt.d.lu rd, rs1   -- converts ui64 to double
                        fregs[ rd ].d = (double) (uint64_t) regs[ rs1 ];
                    else
                        unhandled();
                }
                else if ( 0x70 == funct7 )
                {
                    if ( 0 == rs2 )
                    {
                        if ( 0 == funct3 ) // fmv.x.w rd, frs1
                            memcpy( & regs[ rd ], & fregs[ rs1 ].f, 4 );
                        else if ( 1 == funct3 ) // fclass.s
                        {
                            // rd bit Meaning
                            // 0 rs1 is - infinity
                            // 1 rs1 is a negative normal number.
                            // 2 rs1 is a negative subnormal number.
                            // 3 rs1 is -0
                            // 4 rs1 is +0
                            // 5 rs1 is a positive subnormal number.
                            // 6 rs1 is a positive normal number.
                            // 7 rs1 is + infinity
                            // 8 rs1 is a signaling NaN.
                            // 9 rs1 is a quiet NaN
    
                            uint64_t result = 0;
                            float f = fregs[ rs1 ].f;
                            if ( isnan( f ) )
                            {
                                #if !defined(_MSC_VER) && !defined(OLDGCC) && !defined(__APPLE__)
                                if ( issignaling( f ) )
                                    result = 0x100;
                                else
                                #endif
                                    result = 0x200;
                            }
                            #if !defined(_MSC_VER) && !defined(OLDGCC) && !defined(__APPLE__)
                            else if ( issubnormal( f ) )
                            {
                                if ( f >= 0.0 )
                                    result = 0x20;
                                else
                                    result = 4;
                            }
                            #endif
                            else if ( !isfinite( f ) )
                            {
                                if ( f >= 0.0 )
                                    result = 0x80;
                                else
                                    result = 1;
                            }
                            else
                            {
                                if ( f >= 0.0 )
                                    result = 0x40;
                                else
                                    result = 2;
                            }
                        }
                        else
                            unhandled();
                    }
                    else
                        unhandled();
                }
                else if ( 0x71 == funct7 )
                {
                    if ( 0 == rs2 )
                    {
                        if ( 0 == funct3 ) // fmv.x.d rd, frs1
                            memcpy( & regs[ rd ], & fregs[ rs1 ].d, 8 );
                        else if ( 1 == funct3 ) // fclass.d
                        {
                            uint64_t result = 0;
                            double d = fregs[ rs1 ].d;
                            if ( isnan( d ) )
                            {
                                #if !defined(_MSC_VER) && !defined(OLDGCC) && !defined(__APPLE__)
                                if ( issignaling( d ) )
                                    result = 0x100;
                                else
                                #endif
                                    result = 0x200;
                            }
                            #if !defined(_MSC_VER) && !defined(OLDGCC) && !defined(__APPLE__)
                            else if ( issubnormal( d ) )
                            {
                                if ( d >= 0.0 )
                                    result = 0x20;
                                else
                                    result = 4;
                            }
                            #endif
                            else if ( !isfinite( d ) )
                            {
                                if ( d >= 0.0 )
                                    result = 0x80;
                                else
                                    result = 1;
                            }
                            else
                            {
                                if ( d >= 0.0 )
                                    result = 0x40;
                                else
                                    result = 2;
                            }
                        }
                        else
                            unhandled();
                    }
                    else
                        unhandled();
                }            
                else if ( 0x78 == funct7 )
                {
                    if ( 0 == rs2 && 0 == funct3 )
                        memcpy( & fregs[ rd ].f, & regs[ rs1 ], 4 ); // fmv.w.x frd, rs1    rs1 is probably r0
                    else
                        unhandled();
                }
                else if ( 0x79 == funct7 )
                {
                    if ( 0 == rs2 && 0 == funct3 )
                        memcpy( & fregs[ rd ].d, & regs[ rs1 ], 8 ); // fmv.d.x frd, rs1    rs1 is probably r0
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
                bool branch = false;
    
                if ( 0 == funct3 )  // beq rs1, rs2, bimm
                    branch = ( regs[ rs1 ] == regs[ rs2 ] );
                else if ( 1 == funct3 )  // bne rs1, rs2, bimm
                    branch = ( regs[ rs1 ] != regs[ rs2 ] );
                else if ( 4 == funct3 )  // blt rs1, rs2, bimm
                    branch = ( (int64_t) regs[ rs1 ] < (int64_t) regs[ rs2 ] );
                else if ( 5 == funct3 ) // bge rs1, rs2, b_imm
                    branch = ( (int64_t) regs[ rs1 ] >= (int64_t) regs[ rs2 ] );
                else if ( 6 == funct3 )  // bltu rs1, rs2, bimm
                    branch = ( regs[ rs1 ] < regs[ rs2 ] );
                else if ( 7 == funct3 )  // bgeu rs1, rs2, bimm
                    branch = ( regs[ rs1 ] >= regs[ rs2 ] );
                else
                    unhandled();
    
                if ( branch )
                    pcnext = pc + b_imm;
                break;
            }
            case 0x19:
            {
                assert_type( IType );
                decode_I();
    
                if ( 0 == funct3 )
                {
                    uint64_t temp = ( regs[ rs1 ] + i_imm ); // jalr (rs1) + i_imm
                    if ( 0 != rd )
                        regs[ rd ] = pcnext;
                    pcnext = temp;
                }
                else
                    unhandled();
                break;
            }
            case 0x1b:
            {
                assert_type( JType );
                decode_J();
    
                // jal offset
    
                if ( 0 != rd )
                    regs[ rd ] = pcnext;
    
                int64_t offset = j_imm_u;
                pcnext = pc + offset;
                break;
            }
            case 0x1c:
            {
                assert_type( IType );
                decode_I();
                uint64_t csr = i_imm_u;

                // funct3
                //    000  system (ecall / ebreak)
                //    001  csrrw write csr
                //    010  csrrs read csr
                //    011  csrrc clear bits in csr
                //    101  csrrwi write csr immediate
                //    110  csrrsi set bits in csr immediate
                //    111  csrrci clear bits in csr immediate

                if ( 0 == funct3 ) // system
                {
                    if ( 0x73 == op )
                        riscv_invoke_ecall( *this ); // ecall. don't route through mtvec as a simplification
                    else if ( 0x100073 == op )
                    {
                        // ebreak.  Ignore for now
                    }
                    else
                        unhandled();
                }
                else if ( 1 == funct3 ) // csrrw. csr write
                {
                    if ( 0x1 == csr )
                        regs[ rd ] = 0; // csrrw   rd, fflags, rs1.  read fp exception flags. 0 means all clear
                    else if ( 0x2 == csr )
                        regs[ rd ] = 0; // csrrw   rd, frm, rs1.  read rounding mode. 0 means nearest
                    else if ( 0xc00 == csr ) // csrrw rd, cycle, rs1
                    {
                        if ( 0 != rd )
                            regs[ rd ] = 1000 * clock(); // fake microseconds
                    }
                    else
                    {
                        tracer.Trace( "attempt to read csr %x\n", csr );
                        unhandled();
                    }
                }
                else if ( 2 == funct3 ) // csrrs. csr read
                {
                    if ( 0 == rd )
                        break;

                    if ( 0x1 == csr )
                        regs[ rd ] = 0; // csrrs   rd, fflags, rs1.  read fp exception flags. 0 means all clear
                    else if ( 0x2 == csr )
                        regs[ rd ] = 0; // csrrs   rd, frm, rs1.  read rounding mode. 0 means nearest
                    else if ( 0xb00 == csr ) // csrrs rd, mcycle, rs1. rdmcycle
                        regs[ rd ] = cycles_so_far;
                    else if ( 0xb02 == csr ) // csrrs rd, minstret, rs1. rdminstret
                        regs[ rd ] = cycles_so_far; // assumes one cycle per instruction
                    else if ( 0xc00 == csr ) // csrrs rd, cycle, rs1. rdcycle
                        regs[ rd ] = cycles_so_far;
                    else if ( 0xc01 == csr ) // csrrs rd, time, rs1. rdtime
                    {
                        system_clock::duration d = system_clock::now().time_since_epoch();
                        regs[ rd ] = duration_cast<nanoseconds>( d ).count();
                    }
                    else if ( 0xc02 == csr ) // csrrs rd, instret, rs1. rdinstret
                        regs[ rd ] = cycles_so_far; // assumes one cycle per instruction
                    else if ( 0xf11 == csr ) // mvendorid vendor
                        regs[ rd ] = 0xbeabad00bee;
                    else if ( 0xf12 == csr ) // marchid architecture
                        regs[ rd ] = 0xbeabad00bee;
                    else if ( 0xf13 == csr ) // mimpid implementation
                        regs[ rd ] = 0xbeabad00bee;
                    else if ( 0xf14 == csr ) // mhardid
                        regs[ rd ] = 0; // only one hart is supported and one must have id 0
                    else
                    {
                        tracer.Trace( "attempt to read csr %x\n", csr );
                        unhandled();
                    }
                }
                else if ( 6 == funct3 ) // csrrsi. set bits in csr immediate
                {
                    if ( 1 == csr )
                    {
                        // csrrsi rd, fflags, rs1 --- set fp csr flags like rounding mode (ignore). also, return the flags

                        if ( 0 != rd )
                            regs[ rd ] = 0;
                    }
                    else
                    {
                        tracer.Trace( "attempt to set bits in csr %x\n", csr );
                        unhandled();
                    }
                }
                else
                    unhandled();
                break;
            }
            default:
                unhandled();
        } // switch( opcode_type )

        pc = pcnext;
        cycles_so_far++;
    } while ( cycles_so_far < target_cycles );

    return cycles_so_far - start_cycles;
} //run

