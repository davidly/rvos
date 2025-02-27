fn main()
{
    println!( "testing real number operations" );

    let r32 : f32 = 1.0 / 3.14159;
    println!( "r32: {}", r32 );
    let r64 : f64 = 1.0 / 2.71828;
    println!( "r64: {}", r64 );

    let r32sin : f32 = r32.sin();
    println!( "r32 sin: {}", r32sin );
    let r64sin : f64 = r64.sin();
    println!( "r64 sin: {}", r64sin );

    let r32cos : f32 = r32.cos();
    println!( "r32 cos: {}", r32cos );
    let r64cos : f64 = r64.cos();
    println!( "r64 cos: {}", r64cos );

    let r32tan : f32 = r32.tan();
    println!( "r32 tan: {}", r32tan );
    let r64tan : f64 = r64.tan();
    println!( "r64 tan: {}", r64tan );

    let r32asin : f32 = r32.asin();
    println!( "r32 asin: {}", r32asin );
    let r64asin : f64 = r64.asin();
    println!( "r64 asin: {}", r64asin );

    let r32acos : f32 = r32.acos();
    println!( "r32 acos: {}", r32acos );
    let r64acos : f64 = r64.acos();
    println!( "r64 acos: {}", r64acos );

    let r32atan : f32 = r32.atan();
    println!( "r32 atan: {}", r32atan );
    let r64atan : f64 = r64.atan();
    println!( "r64 atan: {}", r64atan );

    let mut _r32x : f32 = 1.6666666666667;
    for x in 0..=800
    {
        _r32x += x as f32 / 200f32;
        _r32x *= 1.002020202;
    }

    let mut _r64x : f64 = 1.6666666666667;
    for x in 0..=800
    {
        _r64x += x as f64 / 200f64;
        _r64x *= 1.002020202;
    }

    println!( "_r32x = {_r32x}" );
    println!( "_r64x = {_r64x}" );
    _r64x += _r32x as f64;
    println!( "sum: = {_r64x}" );

    let mut r32_total : f32 = 0.0;
    let mut r64_total : f64 = 0.0;
    let mut r32_loop : f32 = 0.05;
    let mut r64_loop : f64 = 0.05;  

    for _x in 0..=30
    {
        r32_loop += 0.104;
        r64_loop += 0.104;
        r32_total += r32_loop.sin();
        r64_total += r64_loop.sin();
        r32_total += r32_loop.cos();
        r64_total += r64_loop.cos();
        r32_total += r32_loop.tan();
        r64_total += r64_loop.tan();
    }         

    println!( "32 total {}, 64 total {}", r32_total, r64_total );
    println!( "exiting real number testing with great success" );
} //main

