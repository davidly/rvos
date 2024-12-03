use std::cmp;

// don't take a dependency on the rand crate, so it builds with rustc

extern "C" {
    fn srand( seed: usize );
    fn rand() -> usize;
}
                
fn my_rand() -> usize {
    unsafe {
        return rand();
    }
}

fn my_srand( seed: usize ) {
    unsafe {
        srand( seed );
    }
}

fn rand32() -> u32 {
    // rand() in the C runtime generally returns numbers <= 32767

    let a : u32 = my_rand() as u32;
    let b : u32 = my_rand() as u32;
    return a | ( b << 16 );
}

fn first_implementation()
{
    println!( "first implementation..." );

    const TOTAL: u64 = 1000000;
    let mut sofar: f64 = 0.0;
    let mut prev_times_ten: u64 = 10;
    let mut _i: u64;

    for _i in 1..TOTAL+1 {
        let _f: f64 = _i as f64;
        sofar += 1.0 / ( _f * _f * _f );

        if _i == ( prev_times_ten ) {
            prev_times_ten = _i * 10;
            println!( "  {:<22} at {} iterations", sofar, _i );
        }
    }
} //first_implementation

fn gcd( m: u32, n: u32 ) -> u32
{
    let mut a: u32;
    let mut b: u32 = cmp::max( m, n );
    let mut r: u32 = cmp::min( m, n );

    while 0 != r
    {
        a = b;
        b = r;
        r = a % b;
    }

    return b;
} //gcd

fn second_implementation()
{
    println!( "second implementation..." );

    my_srand( 0xbaebeabad00bee );
    const TOTAL: u64 = 100000;
    let mut total_coprimes: u64 = 0;
    let mut prev_times_ten: u64 = 10;
    let mut _i: u64;

    for _i in 1..TOTAL+1 {
        let a: u32 = rand32();
        let b: u32 = rand32();
        let c: u32 = rand32();

        let greatest: u32 = gcd( a, gcd( b, c ) );
        if 1 == greatest {
            total_coprimes = 1 + total_coprimes;
        }

        if _i == ( prev_times_ten ) {
            prev_times_ten = _i * 10;
            println!( "  {:<22} at {} iterations", _i as f64 / total_coprimes as f64, _i );
        }
    }
} //second_implementation

fn main()
{
    println!( "starting, should tend towards 1.2020569031595942854..." );

    first_implementation();
    second_implementation();

    println!( "exiting finding ap with great success" );
} //main


