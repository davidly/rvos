fn first_implementation()
{
    const TOTAL: usize = 100000;
    let mut sofar: f64 = 0.0;
    let mut prev: usize = 1;
    let mut _i: usize;

    for _i in 1..TOTAL {
        sofar += 1.0 as f64 / ( _i * _i * _i ) as f64;

        if _i == ( prev * 10 ) {
            prev = _i;
            println!( "  at {} iterations: {}", _i, sofar );
        }
    }
} //first_implementation

fn main()
{
    println!( "starting, should tend towards 1.2020569031595942854...\n" );

    first_implementation();

    println!( "exiting finding ap with great success" );
} //main


