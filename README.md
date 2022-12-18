# platformio
source ~/.platformio/penv/bin/activate
platformio project init --ide eclipse
platformio run

platformio run --target upload
platformio run --target uploadfs 

[telegram]
Step 1. Enter @Botfather in the search tab and choose this bot.

##wemos
Pin | Function    | ESP-8266 Pin | used
TX  | TXD         | TXD          |
RX  | RXD         | RXD          |
A0  | ADC, 3.3V   | A0           | 
D0  | IO          | GPIO16       |
D1  | IO, SCL     | GPIO5        | 
D2  | IO, SDA     | GPIO4        | 
D3  | IO, 10k P-up| GPIO0        | 
D4  | IO, 10k P-up,LED|   GPIO2  | 
D5  | IO, SCK     | GPIO14       | 
D6  | IO, MISO    | GPIO12       | 
D7  | IO, MOSI    | GPIO13       | 
D8  | IO, 10k P-down, SS|  GPIO15|
G   | Ground      | GND          |
5V  | 5V          | -            |
3V3 | 3.3V        | 3.3V         |
RST | Reset       | RST          |




