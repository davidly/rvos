#include <Sipeed_ST7789.h>

SPIClass spi_(SPI0); // MUST be SPI0 for Maix series on board LCD
Sipeed_ST7789 lcd(320, 240, spi_);

extern "C" int bamain( void );

extern "C" void rvos_print_text( const char * pc )
{
    int y = lcd.getCursorY();
    if ( ( y + 8 ) >= lcd.height() )
    {
        lcd.fillScreen( COLOR_BLACK );
        lcd.setCursor( 0, 0 ); 
    }

    lcd.print( pc );
} //rvos_print_text

void setup() 
{
    lcd.begin( 15000000, COLOR_BLACK ); // frequency and fill with red

    bamain();
}

void loop() 
{
    while ( true );
}
