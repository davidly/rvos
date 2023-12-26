rustc --edition 2021 --emit asm -O $1.rs
rustc --edition 2021 -O -C target-feature=+crt-static $1.rs
