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

    println!( "exiting real number testing with great success" );
} //main

