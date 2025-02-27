const ARRAY_DIM: usize = 20;

fn main()
{
    let mut _x: usize;
    let mut _i: usize;
    let mut _j: usize;
    let mut _k: usize;
    let mut a: [ [ f64; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut b: [ [ f64; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut c: [ [ f64; ARRAY_DIM ]; ARRAY_DIM ] =  [ [ 0.0; ARRAY_DIM ]; ARRAY_DIM ];
    let mut sum: f64 = 0.0;

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
    }

    println!( "sum: {}", sum );
    println!( "exiting matrix multiply testing with great success" );
} //main

