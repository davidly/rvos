#pragma once

#include <djl_os.hxx>

#ifdef _MSC_VER

    //#define __inline_perf __declspec(noinline)
    #define __inline_perf __forceinline

#else // MacOS + Linux

    #define __inline_perf

#endif // _MSC_VER

struct RiscV;

// callbacks when instructions are executed

extern void riscv_invoke_ecall( RiscV & cpu );                             // called when the ecall instruction is executed
extern const char * riscv_symbol_lookup( uint64_t address );               // returns the best guess for a symbol name for the address
extern void riscv_hard_termination( RiscV & cpu, const char *pcerr, uint64_t error_value ); // show an error and exit
extern void riscv_check_ptracenow( RiscV & cpu );

struct RiscV
{
    static const size_t zero = 0;
    static const size_t ra = 1;
    static const size_t sp = 2;
    static const size_t gp = 3;
    static const size_t tp = 4;
    static const size_t t0 = 5;
    static const size_t t1 = 6;
    static const size_t t2 = 7;
    static const size_t s0 = 8;
    static const size_t s1 = 9;
    static const size_t a0 = 10;
    static const size_t a1 = 11;
    static const size_t a2 = 12;
    static const size_t a3 = 13;
    static const size_t a4 = 14;
    static const size_t a5 = 15;
    static const size_t a6 = 16;
    static const size_t a7 = 17;
    static const size_t s2 = 18;
    static const size_t s3 = 19;
    static const size_t s4 = 20;
    static const size_t s5 = 21;
    static const size_t s6 = 22;
    static const size_t s7 = 23;
    static const size_t s8 = 24;
    static const size_t s9 = 25;
    static const size_t s10 = 26;
    static const size_t s11 = 27;
    static const size_t t3 = 28;
    static const size_t t4 = 29;
    static const size_t t5 = 30;
    static const size_t t6 = 31;

    bool trace_instructions( bool trace );                // enable/disable tracing each instruction
    void end_emulation( void );                           // make the emulator return at the start of the next instruction
    static bool generate_rvc_table( const char * path );  // generate a 64k x 32-bit rvc lookup table

    RiscV( vector<uint8_t> & memory, uint64_t base_address, uint64_t start, bool compressed_rvc, uint64_t stack_commit, uint64_t top_of_stack )
    {
        memset( this, 0, sizeof( *this ) );
        pc = start;
        stack_size = stack_commit;                 // remember how much of the top of RAM is allocated to the stack
        stack_top = top_of_stack;                  // where the stack started
        regs[ sp ] = top_of_stack;                 // points at argc with argv, penv, and aux records above it
        base = base_address;                       // lowest valid address in the app's address space, maps to offset 0 in mem
        mem = memory.data();                       // save the pointer, but don't take ownership
        mem_size = memory.size();
        beyond = mem + memory.size();              // addresses at beyond and later are illegal
        membase = mem - base;                      // real pointer to the start of the app's memory (prior to offset)
        rvc = compressed_rvc;
    } //RiscV

    uint64_t run( uint64_t max_cycles );
    const char * reg_name( uint64_t reg );
    const char * freg_name( uint64_t reg );

    union floating
    {
        float f;
        double d;
    };

    uint64_t regs[ 32 ]; // x0 through x31
    floating fregs[ 32 ]; // f0 through f31
    uint64_t pc;
    uint64_t cycles_so_far;
    uint64_t csr_mstatus;
    uint64_t csr_sstatus;
    uint64_t csr_stvec;
    uint64_t csr_sscratch;
    uint64_t csr_sepc;
    uint64_t csr_scause;
    uint64_t csr_stval;
    uint64_t csr_sip;
    uint64_t csr_satp;
    uint64_t csr_misa;
    uint64_t csr_medeleg;
    uint64_t csr_mideleg;
    uint64_t csr_mie;
    uint64_t csr_mtvec;
    uint64_t csr_mcounteren;
    uint64_t csr_mscratch;
    uint64_t csr_mepc;
    uint64_t csr_mcause;
    uint64_t csr_mtval;
    uint64_t csr_mip;
    uint64_t csr_mdeleg;
    uint64_t csr_sie;
    uint64_t csr_pmpaddr0;
    uint64_t csr_pmpcfg0;

    uint8_t * mem;
    uint8_t * beyond;
    uint64_t base;
    uint8_t * membase;              // host pointer to base of vm's memory
    uint64_t stack_size;
    uint64_t stack_top;
    uint64_t mem_size;
    bool rvc;

    uint64_t getoffset( uint64_t address )
    {
        return address - base;
    } //getoffset

    uint64_t get_vm_address( uint64_t offset )
    {
        return base + offset;
    } //get_vm_address

    uint64_t host_to_vm_address( void * p )
    {
        uint8_t * p8 = (uint8_t *) p;

        return (uint64_t) ( p8 - mem + base );
    } //host_to_vm_address

    uint8_t * getmem( uint64_t offset )
    {
        #ifdef NDEBUG

            return membase + offset;

        #else

            uint8_t * r = membase + offset;

            if ( r >= beyond )
                riscv_hard_termination( *this, "memory reference beyond address space:", offset );

            if ( r < mem )
                riscv_hard_termination( *this, "memory reference prior to address space:", offset );

            return r;

        #endif
    } //getmem

    uint64_t getui64( uint64_t o ) { return * (uint64_t *) getmem( o ); }
    uint32_t getui32( uint64_t o ) { return * (uint32_t *) getmem( o ); }
    uint16_t getui16( uint64_t o ) { return * (uint16_t *) getmem( o ); }
    uint8_t getui8( uint64_t o ) { return * (uint8_t *) getmem( o ); }
    float getfloat( uint64_t o ) { return * (float *) getmem( o ); }
    double getdouble( uint64_t o ) { return * (double *) getmem( o ); }

    void setui64( uint64_t o, uint64_t val ) { * (uint64_t *) getmem( o ) = val; }
    void setui32( uint64_t o, uint32_t val ) { * (uint32_t *) getmem( o ) = val; }
    void setui16( uint64_t o, uint16_t val ) { * (uint16_t *) getmem( o ) = val; }
    void setui8( uint64_t o, uint8_t val ) { * (uint8_t *) getmem( o ) = val; }
    void setfloat( uint64_t o, float val ) { * (float *) getmem( o ) = val; }
    void setdouble( uint64_t o, double val ) { * (double *) getmem( o ) = val; }

  private:

    uint64_t op;
    uint64_t opcode_type;
    uint64_t funct3;
    uint64_t funct7;
    uint64_t rd;
    uint64_t rs1;
    uint64_t rs2;
    uint64_t i_imm_u;
    int64_t i_imm;
    uint64_t i_shamt5;
    uint64_t i_shamt6;
    uint64_t i_top2;
    int64_t s_imm;
    int64_t u_imm;
    uint64_t u_imm_u;
    uint64_t j_imm_u;
    int64_t b_imm;
    uint64_t c_rc1;
    uint64_t c_rc2;
    uint64_t c_imm_flags;

    void unhandled( void );

    static uint32_t uncompress_rvc( uint16_t x );

    __inline_perf uint64_t decode()
    {
        op = getui32( pc );
        uint64_t pc_next = pc + 4;

        if ( 3 != ( op & 0x3 ) )
        {
            assert( rvc );
            op = uncompress_rvc( (uint16_t) ( op & 0xffff ) );
            pc_next -= 2;
        }

        opcode_type = ( 0x1f & ( op >> 2 ) );
        return pc_next;
    } //decode

    // when inlined, the compiler uses btc for bits. when non-inlined it does the slow thing
    // bits is the 0-based high bit that will be extended to the left.

#ifdef _MSC_VER
    __forceinline
#endif
    static int64_t sign_extend( uint64_t x, uint64_t bits )
    {
        const int64_t m = ( (uint64_t) 1 ) << bits;
        return ( x ^ m ) - m;
    } //sign_extend

    __inline_perf void decode_J()
    {
        rd = ( op >> 7 ) & 0x1f;

        // upper 20 bits of uint32_t: imm[20|10:1|11|19:12]
    
        uint64_t r = ( op & 0xff000 ) | 
                     ( ( op >> 9 ) & 0x800 ) |
                     ( ( op >> 20 ) & 0x7fe ) |
                     ( ( op >> 11 ) & 0x100000 );
        j_imm_u = sign_extend( r, 20 );
    } //decode_J

    __inline_perf void decode_U()
    {
        rd = ( op >> 7 ) & 0x1f;
        u_imm_u = ( op >> 12 ) & 0xfffff;
        u_imm = sign_extend( u_imm_u, 19 );
    } //decode_U

    __inline_perf void decode_B()
    {
        uint64_t theop = op >> 12;
        funct3 = theop & 0x7;
        theop >>= 3;
        rs1 = theop & 0x1f;
        theop >>= 5;
        rs2 = theop & 0x1f;
        b_imm = sign_extend( ( ( op >> 7 ) & 0x1e ) | ( ( op << 4 ) & 0x800 ) | ( ( op >> 20 ) & 0x7e0 ) | ( ( op >> 19 ) & 0x1000 ), 12 );
    } //decode_B

    __inline_perf void decode_S()
    {
        funct3 = ( op >> 12 ) & 0x7;
        rs1 = ( op >> 15 ) & 0x1f;
        rs2 = ( op >> 20 ) & 0x1f;
        s_imm = sign_extend( ( ( op >> 20 ) & ( 0x7f << 5 ) ) | ( ( op >> 7 ) & 0x1f ), 11 );
    } //decode_S

    __inline_perf void decode_I()
    {
        uint64_t theop = op >> 7;
        rd = theop & 0x1f;
        theop >>= 5;
        funct3 = theop & 0x7;
        theop >>= 3;
        rs1 = theop & 0x1f;
        theop >>= 5;
        i_imm_u = theop & 0xfff;
        i_imm = sign_extend( i_imm_u, 11 );
    } //decode_I

    __inline_perf void decode_I_shift()
    {
        // 6 bit shift amount. srli, slliw, slli need this

        i_shamt6 = ( op >> 20 ) & 0x3f;
        
        // 5 bit shift amount. sraiw, srai, srliw need this

        i_shamt5 = i_shamt6 & 0x1f;
     
        i_top2 = ( op >> 30 );
    } //decode_I_shift

    __inline_perf void decode_R()
    {
        uint64_t theop = op >> 7;
        rd = theop & 0x1f;
        theop >>= 5;
        funct3 = theop & 0x7;
        theop >>= 3;
        rs1 = theop & 0x1f;
        theop >>= 5;
        rs2 = theop & 0x1f;
        theop >>= 5;
        funct7 = theop & 0x7f;
    } //decode_R

    __inline_perf void decode_C() // risc-v extension cmvxx instructions -- conditional move
    {
        uint64_t theop = op >> 7;
        rd = theop & 0x1f;
        theop >>= 5;
        funct3 = theop & 0x7;
        theop >>= 3;
        rs1 = theop & 0x1f;
        theop >>= 5;
        c_rc2 = theop & 0x1f;
        theop >>= 5;
        c_rc1 = theop & 0x1f;
        theop >>= 5;
        c_imm_flags = theop & 0x3;
    } //decode_C

    void assert_type( uint8_t t );
    void trace_state( void );                  // trace the machine current status
}; //RiscV


