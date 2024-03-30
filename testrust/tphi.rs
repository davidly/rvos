const LIMIT: u64 = 40;

fn main()
{
    println!( "phi should tend towards 1.61803398874989484820458683436563811772030" );
    let mut prev2: u128 = 1;
    let mut prev1: u128 = 1;
    let mut last_shown: u64 = 0;
    let mut _i: u64;

    for _i in 1..LIMIT+1 {
        let next: u128 = prev1 + prev2;
        prev2 = prev1;
        prev1 = next;

        if _i == ( last_shown + 5 )
        {
            last_shown = _i;
            println!( "  at {:4} iterations: {}", _i, prev1 as f64 / prev2 as f64 );
        }
    }

    println!( "exiting finding phi with great success" );
}
