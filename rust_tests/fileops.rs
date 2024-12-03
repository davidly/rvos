use std::process;
use std::fs::File;
use std::io::Read;
use std::io::Write;
use std::io::Seek;
use std::io::SeekFrom;

fn main() -> std::io::Result<()>
{
    let block_count = 128;
    let block_len = 256;
    let data_len = 128;
    let mut data : Vec<u8> = Vec::with_capacity( data_len );
    let mut val: u8 = 0x10;

    for _i in 0..data_len {
        data.push( val );
    }

    {
        let mut test_file = File::create( "fileops.dat" )?;

        for _o in 0..block_count {
            test_file.seek( SeekFrom::Start( _o * block_len ) )?;

            val += 1;
            for _i in 0..data_len {
                data[ _i ] = val;
            }
    
            test_file.write_all( &data )?;
            test_file.flush()?;
        }
    }

    {
        val = 0x91;
        let mut test_file = File::open( "fileops.dat" )?;

        for _o in (0..block_count).rev() {
            test_file.seek( SeekFrom::Start( _o * block_len ) )?;

            val -= 1;
            test_file.read_exact( & mut data )?;

            for _i in 0..data_len {
                if val != data[ _i ] {
                    println!( "data value at _o {} and _i {}: {} isn't {}", _o, _i, data[ _i ], val );
                    process::exit( 0 );
                }
            }
        }
    }

    println!( "exiting fileops with great success" );
    Ok(())
}

