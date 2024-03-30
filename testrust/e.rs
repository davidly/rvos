const DIGITS_TO_FIND: usize = 200; //9009;

fn main()
{
    println!( "testing finding e" );
    let mut limit: usize = DIGITS_TO_FIND;
    let mut x: usize = 0;
    let mut a: [ usize; DIGITS_TO_FIND ] = [ 1; DIGITS_TO_FIND ];
    
    a[1] = 2;
    a[0] = 0;
    while limit > 9 {
        limit = limit - 1;
        let mut n: usize = limit;
        while 0 != n {
            a[n] = x % n;
            x = 10 * a[ n - 1 ] + x / n;
            n = n - 1;
        }
        print!( "{}", x );
    }

    println!( "\nexiting finding e with great success" );
} //main

