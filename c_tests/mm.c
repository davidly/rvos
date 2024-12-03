/* BYTE magazine October 1982. Jerry Pournelle. */
/* ported to C and added test cases by David Lee */
/* various bugs not found because dimensions are square fixed by David Lee */
/* expected result for 20/20/20: 4.65880E+05 */
/* 20/20/20 float version runs in 13 seconds on the original PC */

/* why so many matrix size and datatype variants? g++ produces different code to optimize for each case */

#define LINT_ARGS
#pragma GCC diagnostic ignored "-Wformat="

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#define matrix_test( ftype, dim ) \
    ftype A_##ftype##dim[ dim ][ dim ]; \
    ftype B_##ftype##dim[ dim ][ dim ]; \
    ftype C_##ftype##dim[ dim ][ dim ]; \
    void fillA_##ftype##dim() \
    { \
        for ( int i = 0; i < dim; i++ ) \
            for ( int j = 0; j < dim; j++ ) \
                A_##ftype##dim[ i ][ j ] = (ftype) ( i + j + 2 ); \
    } \
    void fillB_##ftype##dim() \
    { \
        for ( int i = 0; i < dim; i++ ) \
            for ( int j = 0; j < dim; j++ ) \
                B_##ftype##dim[ i ][ j ] = (ftype) ( ( i + j + 2 ) / ( j + 1 ) ); \
    } \
    void fillC_##ftype##dim() \
    { \
        for ( int i = 0; i < dim; i++ ) \
            for ( int j = 0; j < dim; j++ ) \
                C_##ftype##dim[ i ][ j ] = (ftype) 0; \
    } \
    __attribute__((noinline)) void print_array_##ftype##dim( ftype a[dim][dim] ) \
    { \
        printf( "array: \n" ); \
        for ( int i = 0; i < dim; i++ ) \
        { \
            for ( int j = 0; j < dim; j++ ) \
                printf( " %lf", (double) a[ i ][ j ] ); \
            printf( "\n" ); \
        } \
    } \
    void matmult_##ftype##dim() \
    { \
        /* this debugging line causes the compiler to optimize code differently (tbl/zip)! syscall( 0x2002, 1 ); */ \
        for ( int i = 0; i < dim; i++ ) \
            for ( int j = 0; j < dim; j++ ) \
                for ( int k = 0; k < dim; k++ ) \
                    C_##ftype##dim[ i ][ j ] += A_##ftype##dim[ i ][ k ] * B_##ftype##dim[ k ][ j ]; \
    } \
    ftype sum_##ftype##dim() \
    { \
        ftype result = (ftype) 0; \
        for ( int i = 0; i < dim; i++ ) \
            for ( int j = 0; j < dim; j++ ) \
                result += C_##ftype##dim[ i ][ j ]; \
        return result; \
    } \
    ftype run_##ftype##dim() \
    { \
        fillA_##ftype##dim(); \
        fillB_##ftype##dim(); \
        fillC_##ftype##dim(); \
        matmult_##ftype##dim(); \
        /*print_array_##ftype##dim( A_##ftype##dim );*/ \
        /*print_array_##ftype##dim( B_##ftype##dim );*/ \
        /*print_array_##ftype##dim( C_##ftype##dim );*/ \
        return sum_##ftype##dim(); \
    }

#define declare_matrix_tests( type ) \
    matrix_test( type, 1 ); \
    matrix_test( type, 2 ); \
    matrix_test( type, 3 ); \
    matrix_test( type, 4 ); \
    matrix_test( type, 5 ); \
    matrix_test( type, 6 ); \
    matrix_test( type, 7 ); \
    matrix_test( type, 8 ); \
    matrix_test( type, 9 ); \
    matrix_test( type, 10 ); \
    matrix_test( type, 11 ); \
    matrix_test( type, 12 ); \
    matrix_test( type, 13 ); \
    matrix_test( type, 14 ); \
    matrix_test( type, 15 ); \
    matrix_test( type, 16 ); \
    matrix_test( type, 17 ); \
    matrix_test( type, 18 ); \
    matrix_test( type, 19 ); \
    matrix_test( type, 20 );

declare_matrix_tests( float );
declare_matrix_tests( double );
declare_matrix_tests( int8_t );
declare_matrix_tests( uint8_t );
declare_matrix_tests( int16_t );
declare_matrix_tests( uint16_t );
declare_matrix_tests( int32_t );
declare_matrix_tests( uint32_t );
declare_matrix_tests( int64_t );
declare_matrix_tests( uint64_t );

#define run_tests( type, format ) \
    printf( "matrix %s 1: " format "\n", #type, run_##type##1() ); \
    printf( "matrix %s 2: " format "\n", #type, run_##type##2() ); \
    printf( "matrix %s 3: " format "\n", #type, run_##type##3() ); \
    printf( "matrix %s 4: " format "\n", #type, run_##type##4() ); \
    printf( "matrix %s 5: " format "\n", #type, run_##type##5() ); \
    printf( "matrix %s 6: " format "\n", #type, run_##type##6() ); \
    printf( "matrix %s 7: " format "\n", #type, run_##type##7() ); \
    printf( "matrix %s 8: " format "\n", #type, run_##type##8() ); \
    printf( "matrix %s 9: " format "\n", #type, run_##type##9() ); \
    printf( "matrix %s 10: " format "\n", #type, run_##type##10() ); \
    printf( "matrix %s 11: " format "\n", #type, run_##type##11() ); \
    printf( "matrix %s 12: " format "\n", #type, run_##type##12() ); \
    printf( "matrix %s 13: " format "\n", #type, run_##type##13() ); \
    printf( "matrix %s 14: " format "\n", #type, run_##type##14() ); \
    printf( "matrix %s 15: " format "\n", #type, run_##type##15() ); \
    printf( "matrix %s 16: " format "\n", #type, run_##type##16() ); \
    printf( "matrix %s 17: " format "\n", #type, run_##type##17() ); \
    printf( "matrix %s 18: " format "\n", #type, run_##type##18() ); \
    printf( "matrix %s 19: " format "\n", #type, run_##type##19() ); \
    printf( "matrix %s 20: " format "\n", #type, run_##type##20() );

int main( int argc, char * argv[] )
{
    run_tests( float, "%f");
    run_tests( double, "%lf");
    run_tests( int8_t, "%d");
    run_tests( uint8_t, "%u");
    run_tests( int16_t, "%d");
    run_tests( uint16_t, "%u");
    run_tests( int32_t, "%d");
    run_tests( uint32_t, "%u");
    run_tests( int64_t, "%lld");
    run_tests( uint64_t, "%llu");

    printf( "matrix multiply test completed with great success\n" );
    return 0;
} //main