use std::sync::atomic::
{
    AtomicI8, AtomicI16, AtomicI32, AtomicI64,
    AtomicU8, AtomicU16, AtomicU32, AtomicU64,
    AtomicBool, Ordering, // Mutex, MutexGuard, RWLockReadGuard, RWLockWriteGuard,
};

fn test_signed64( v: &mut AtomicI64 )
{
    assert_eq!( v.fetch_add( 7, Ordering::SeqCst ), 0 );
    assert_eq!( v.fetch_sub( 9, Ordering::SeqCst ), 7 );
    assert_eq!( v.load( Ordering::SeqCst ), -2 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_and( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0x20 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_or( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xf7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_xor( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xd7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_nand( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), -33 );

    v.store( -666, Ordering::SeqCst );
    assert_eq!( v.fetch_max( 777, Ordering::SeqCst ), -666 );
    assert_eq!( v.load( Ordering::SeqCst ), 777 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.fetch_min( -777, Ordering::SeqCst ), 666 );
    assert_eq!( v.load( Ordering::SeqCst ), -777 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( 666, 555, Ordering::Acquire, Ordering::Relaxed ), Ok( 666 ) );
} //test_signed64

fn test_unsigned64( v: &mut AtomicU64 )
{
    assert_eq!( v.fetch_add( 7, Ordering::SeqCst ), 0 );
    assert_eq!( v.fetch_sub( 9, Ordering::SeqCst ), 7 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xfffffffffffffffe );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_and( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0x20 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_or( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xf7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_xor( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xd7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_nand( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xffffffffffffffdf );

    v.store( 0xffffffffffffff00, Ordering::SeqCst );
    assert_eq!( v.fetch_max( 777, Ordering::SeqCst ), 0xffffffffffffff00 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xffffffffffffff00 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.fetch_min( 0xffffffffffffff00, Ordering::SeqCst ), 666 );
    assert_eq!( v.load( Ordering::SeqCst ), 666 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( 666, 555, Ordering::Acquire, Ordering::Relaxed ), Ok( 666 ) );
} //test_unsigned64

fn test_signed32( v: &mut AtomicI32)
{
    assert_eq!( v.fetch_add( 7, Ordering::SeqCst ), 0 );
    assert_eq!( v.fetch_sub( 9, Ordering::SeqCst ), 7 );
    assert_eq!( v.load( Ordering::SeqCst ), -2 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_and( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0x20 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_or( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xf7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_xor( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xd7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_nand( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), -33 );

    v.store( -666, Ordering::SeqCst );
    assert_eq!( v.fetch_max( 777, Ordering::SeqCst ), -666 );
    assert_eq!( v.load( Ordering::SeqCst ), 777 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.fetch_min( -777, Ordering::SeqCst ), 666 );
    assert_eq!( v.load( Ordering::SeqCst ), -777 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( 666, 555, Ordering::Acquire, Ordering::Relaxed ), Ok( 666 ) );
} //test_signed32

fn test_unsigned32( v: &mut AtomicU32 )
{
    assert_eq!( v.fetch_add( 7, Ordering::SeqCst ), 0 );
    assert_eq!( v.fetch_sub( 9, Ordering::SeqCst ), 7 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xfffffffe );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_and( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0x20 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_or( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xf7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_xor( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xd7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_nand( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xffffffdf );

    v.store( 0xffffff00, Ordering::SeqCst );
    assert_eq!( v.fetch_max( 777, Ordering::SeqCst ), 0xffffff00 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xffffff00 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.fetch_min( 0xffffff00, Ordering::SeqCst ), 666 );
    assert_eq!( v.load( Ordering::SeqCst ), 666 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( 666, 555, Ordering::Acquire, Ordering::Relaxed ), Ok( 666 ) );
} //test_unsigned32

fn test_signed16( v: &mut AtomicI16)
{
    assert_eq!( v.fetch_add( 7, Ordering::SeqCst ), 0 );
    assert_eq!( v.fetch_sub( 9, Ordering::SeqCst ), 7 );
    assert_eq!( v.load( Ordering::SeqCst ), -2 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_and( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0x20 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_or( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xf7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_xor( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xd7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_nand( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), -33 );

    v.store( -666, Ordering::SeqCst );
    assert_eq!( v.fetch_max( 777, Ordering::SeqCst ), -666 );
    assert_eq!( v.load( Ordering::SeqCst ), 777 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.fetch_min( -777, Ordering::SeqCst ), 666 );
    assert_eq!( v.load( Ordering::SeqCst ), -777 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( 666, 555, Ordering::Acquire, Ordering::Relaxed ), Ok( 666 ) );
} //test_signed16

fn test_unsigned16( v: &mut AtomicU16 )
{
    assert_eq!( v.fetch_add( 7, Ordering::SeqCst ), 0 );
    assert_eq!( v.fetch_sub( 9, Ordering::SeqCst ), 7 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xfffe );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_and( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0x20 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_or( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xf7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_xor( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xd7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_nand( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xffdf );

    v.store( 0xff00, Ordering::SeqCst );
    assert_eq!( v.fetch_max( 777, Ordering::SeqCst ), 0xff00 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xff00 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.fetch_min( 0xff00, Ordering::SeqCst ), 666 );
    assert_eq!( v.load( Ordering::SeqCst ), 666 );

    v.store( 666, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( 666, 555, Ordering::Acquire, Ordering::Relaxed ), Ok( 666 ) );
} //test_unsigned16

fn test_signed8( v: &mut AtomicI8)
{
    assert_eq!( v.fetch_add( 7, Ordering::SeqCst ), 0 );
    assert_eq!( v.fetch_sub( 9, Ordering::SeqCst ), 7 );
    assert_eq!( v.load( Ordering::SeqCst ), -2 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_and( -96, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0x20 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_or( -96, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), -9 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_xor( -96, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), -41 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_nand( -96, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), -33 );

    v.store( -66, Ordering::SeqCst );
    assert_eq!( v.fetch_max( 77, Ordering::SeqCst ), -66 );
    assert_eq!( v.load( Ordering::SeqCst ), 77 );

    v.store( 66, Ordering::SeqCst );
    assert_eq!( v.fetch_min( -77, Ordering::SeqCst ), 66 );
    assert_eq!( v.load( Ordering::SeqCst ), -77 );

    v.store( 66, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( 66, 55, Ordering::Acquire, Ordering::Relaxed ), Ok( 66 ) );
} //test_signed8

fn test_unsigned8( v: &mut AtomicU8 )
{
    assert_eq!( v.fetch_add( 7, Ordering::SeqCst ), 0 );
    assert_eq!( v.fetch_sub( 9, Ordering::SeqCst ), 7 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xfe );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_and( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0x20 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_or( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xf7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_xor( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xd7 );

    v.store( 0x77, Ordering::SeqCst );
    assert_eq!( v.fetch_nand( 0xa0, Ordering::SeqCst ), 0x77 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xdf );

    v.store( 0xf0, Ordering::SeqCst );
    assert_eq!( v.fetch_max( 77, Ordering::SeqCst ), 0xf0 );
    assert_eq!( v.load( Ordering::SeqCst ), 0xf0 );

    v.store( 66, Ordering::SeqCst );
    assert_eq!( v.fetch_min( 0xff, Ordering::SeqCst ), 66 );
    assert_eq!( v.load( Ordering::SeqCst ), 66 );

    v.store( 66, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( 66, 55, Ordering::Acquire, Ordering::Relaxed ), Ok( 66 ) );
} //test_unsigned8

fn test_bool( v: &mut AtomicBool )
{
    v.store( false, Ordering::SeqCst );
    assert_eq!( v.load( Ordering::SeqCst ), false );

    v.store( true, Ordering::SeqCst );
    assert_eq!( v.load( Ordering::SeqCst ), true );

    assert_eq!( v.swap( false, Ordering::SeqCst), true );
    assert_eq!( v.load( Ordering::SeqCst ), false );

    // deprecated
    // v.store( true, Ordering::SeqCst );
    // assert_eq!( v.compare_and_swap( true, false, Ordering::Relaxed ), true );
    // assert_eq!( v.load( Ordering::Relaxed), false);
    // assert_eq!( v.compare_and_swap( true, true, Ordering::Relaxed ), false );
    // assert_eq!( v.load( Ordering::Relaxed ), false );

    v.store( true, Ordering::SeqCst );
    assert_eq!( v.compare_exchange( true, false, Ordering::Acquire, Ordering::Relaxed ), Ok( true ) );
    assert_eq!( v.load( Ordering::Relaxed ), false );
    assert_eq!( v.compare_exchange( true, true, Ordering::SeqCst, Ordering::Acquire ), Err( false ) );
    assert_eq!( v.load( Ordering::Relaxed ), false );

    v.store( false, Ordering::SeqCst );
    let new = true;
    let mut old = v.load( Ordering::Relaxed );
    loop
    {
        match v.compare_exchange_weak( old, new, Ordering::SeqCst, Ordering::Relaxed )
        {
            Ok(_) => break, Err( x ) => old = x,
        }
    }
} //test_bool

fn main()
{
    println!( "testing atomic operations" );

    println!( "i64" );
    let mut val_i64: AtomicI64 = AtomicI64::new( 0 );
    test_signed64( &mut val_i64 );

    println!( "u64" );
    let mut val_u64: AtomicU64 = AtomicU64::new( 0 );
    test_unsigned64( &mut val_u64 );

    println!( "i32" );
    let mut val_i32: AtomicI32 = AtomicI32::new( 0 );
    test_signed32( &mut val_i32 );

    println!( "u32" );
    let mut val_u32: AtomicU32 = AtomicU32::new( 0 );
    test_unsigned32( &mut val_u32 );

    println!( "i16" );
    let mut val_i16: AtomicI16 = AtomicI16::new( 0 );
    test_signed16( &mut val_i16 );

    println!( "u16" );
    let mut val_u16: AtomicU16 = AtomicU16::new( 0 );
    test_unsigned16( &mut val_u16 );

    println!( "i8" );
    let mut val_i8: AtomicI8 = AtomicI8::new( 0 );
    test_signed8( &mut val_i8 );

    println!( "u8" );
    let mut val_u8: AtomicU8 = AtomicU8::new( 0 );
    test_unsigned8( &mut val_u8 );

    println!( "bool" );
    let mut val_bool: AtomicBool = AtomicBool::new( false );
    test_bool( &mut val_bool );

    println!( "exiting atomic testing with great success" );
} //main
