#![allow(arithmetic_overflow)]

const ARRAY_DIM: usize = 20;

/*
use std::ops:: {Add, AddAssign, Mul, MulAssign };
pub fn matrix_ops<T>() -> T
where
    T: Sized + Default + Add<T,Output=T>,
{
    let mut sum: T;
    let mut a: [ [ T; ARRAY_DIM ]; ARRAY_DIM ];
    let mut b: [ [ T; ARRAY_DIM ]; ARRAY_DIM ];
    let mut c: [ [ T; ARRAY_DIM ]; ARRAY_DIM ];

    sum = T::Zero();
    return sum;
}
*/

fn main()
{
/*    
    let r64sum: f64 = matrix_ops();
    println!( "sum: {}", r64sum );
*/

    let mut _x: usize;
    let mut _i: usize;
    let mut _j: usize;
    let mut _k: usize;

    let mut a: [ [ f64; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut b: [ [ f64; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut c: [ [ f64; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut sum: f64 = 0.0;

    let mut af32: [ [ f32; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut bf32: [ [ f32; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut cf32: [ [ f32; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut sumf32: f32 = 0.0;

    let mut au32: [ [ u32; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut bu32: [ [ u32; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut cu32: [ [ u32; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut sumu32: u32 = 0;

    let mut au16: [ [ u16; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut bu16: [ [ u16; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut cu16: [ [ u16; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut sumu16: u16 = 0;

    for _x in 0..100 {
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                a[ _i ][ _j ] = ( _i + _j + 2 ) as f64;
            }
        }

        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                b[ _i ][ _j ] = ( ( _i + _j + 2 ) / ( _j + 1 ) ) as f64;
            }
        }

        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                c[ _i ][ _j ] = 0.0;
            }
        }

        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                for _k in 0..ARRAY_DIM {
                    c[ _i ][ _j ] += a[ _i ][ _k ] * b[ _k ][ _j ];
                }
            }
        }

        sum = 0.0;
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                sum += c[ _i ][ _j ];
            }
        }
        
        if 465880.000000 != sum {
            println!( "invalid sum found!!" );
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                af32[ _i ][ _j ] = ( _i + _j + 2 ) as f32;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                bf32[ _i ][ _j ] = ( ( _i + _j + 2 ) / ( _j + 1 ) ) as f32;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                cf32[ _i ][ _j ] = 0.0;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                for _k in 0..ARRAY_DIM {
                    cf32[ _i ][ _j ] += af32[ _i ][ _k ] * bf32[ _k ][ _j ];
                }
            }
        }
        
        sumf32 = 0.0;
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                sumf32 += cf32[ _i ][ _j ];
            }
        }
        
        if 465880.000000 != sumf32 {
            println!( "invalid sumf32 found!!" );
        }
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                au32[ _i ][ _j ] = ( _i + _j + 2 ) as u32;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                bu32[ _i ][ _j ] = ( ( _i + _j + 2 ) / ( _j + 1 ) ) as u32;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                cu32[ _i ][ _j ] = 0;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                for _k in 0..ARRAY_DIM {
                    cu32[ _i ][ _j ] += au32[ _i ][ _k ] * bu32[ _k ][ _j ];
                }
            }
        }
        
        sumu32 = 0;
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                sumu32 += cu32[ _i ][ _j ];
            }
        }
        
        if 465880 != sumu32 {
            println!( "invalid sumu32 found!!" );
        }
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                au16[ _i ][ _j ] = ( _i + _j + 2 ) as u16;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                bu16[ _i ][ _j ] = ( ( _i + _j + 2 ) / ( _j + 1 ) ) as u16;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                cu16[ _i ][ _j ] = 0;
            }
        }
        
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                for _k in 0..ARRAY_DIM {
                    cu16[ _i ][ _j ] += au16[ _i ][ _k ] * bu16[ _k ][ _j ];
                }
            }
        }
        
        sumu16 = 0;
        for _i in 0..ARRAY_DIM {
            for _j in 0..ARRAY_DIM {
                sumu16 += cu16[ _i ][ _j ];
            }
        }
        
        if 7128 != sumu16 {
            println!( "invalid sumu16 found!! {}", sumu16 );
        }
    }
    
    println!( "sum f64: {}", sum );
    println!( "sum f32: {}", sumf32 );
    println!( "sum u32: {}", sumu32 );
    println!( "sum u16: {}", sumu16 );
    println!( "exiting matrix multiply testing with great success" );
} //main

