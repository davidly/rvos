
use std::fs::File;
use std::io::{BufReader, BufRead, Write};

fn main() -> std::io::Result<()>
{
    let mut vec = vec![];

    {
        let file_in = File::open( "words.txt" )?;
        let reader = BufReader::new( file_in );
    
        for line in reader.lines() {
            let line = line?;
            if line.len() > 0 {
                vec.push( line );
            }
        }
    }

    println!( "element count in vector: {}", vec.len() );

    vec.sort();
    vec.dedup();

    println!( "element count after dedup: {}", vec.len() );

    let mut file_out = File::create( "sorted.txt" )?;

    for n in 0..vec.len()-1 {
        write!( file_out, "{}\n", vec[ n ] )?;
    }

    file_out.sync_data()?;

    Ok(())
}
