/*******************************************************
This program was created by the
CodeWizardAVR V3.14 Advanced
Automatic Program Generator
© Copyright 1998-2014 Pavel Haiduc, HP InfoTech s.r.l.
http://www.hpinfotech.com

Project : 
Version : 
Date    : 10/3/2025
Author  : 
Company : 
Comments: 


Chip type               : ATmega32
Program type            : Application
AVR Core Clock frequency: 8.000000 MHz
Memory model            : Small
External RAM size       : 0
Data Stack size         : 512
*******************************************************/

#include <mega32.h>

#include <delay.h>

// Declare your global variables here
unsigned int adc_values[3];  // Temp storage for reads
// Voltage Reference: AVCC pin
#define ADC_VREF_TYPE ((0<<REFS1) | (1<<REFS0) | (0<<ADLAR))

// Read the AD conversion result
unsigned int read_adc(unsigned char adc_input)
{
    ADMUX=adc_input | ADC_VREF_TYPE;
    // Delay needed for the stabilization of the ADC input voltage
    delay_us(10);
    // Start the AD conversion
    ADCSRA|=(1<<ADSC);
    // Wait for the AD conversion to complete
    while ((ADCSRA & (1<<ADIF))==0);
    ADCSRA|=(1<<ADIF);
    return ADCW;
}

unsigned int average_adc(unsigned char adc_input, unsigned char samples)
{
    unsigned long sum = 0;  // 32-bit to avoid overflow (8 * 1023 = ~8k)
    unsigned char i;        // Declare loop var at block start (C89 requirement)
    for (i = 0; i < samples; i++) {  // No init/decl in for
        sum += read_adc(adc_input);  // Read and add
        delay_us(50);  // Short settle time between reads (adjust if needed)
    }
    return (unsigned int)(sum / samples);  // Divide and return 10-bit avg
}

// TWI functions
#include <twi.h>

// TWI Slave receive buffer
#define TWI_RX_BUFFER_SIZE 1
unsigned char twi_rx_buffer[TWI_RX_BUFFER_SIZE];

// TWI Slave transmit buffer
#define TWI_TX_BUFFER_SIZE 6
unsigned char twi_tx_buffer[TWI_TX_BUFFER_SIZE];

// TWI Slave receive handler
// This handler is called everytime a byte
// is received by the TWI slave
bool twi_rx_handler(bool rx_complete)
{
if (twi_result==TWI_RES_OK)
   {
   unsigned char cmd = twi_rx_buffer[0];
   
   if (rx_complete) {
       // Process command  
       // Command protocol:
       // 0x01-0x04: Toggle commands (manual mode)
       // 0x10-0x17: Explicit ON/OFF (automatic control)
       switch(cmd) {
           // ===== TOGGLE COMMANDS (Manual Mode) =====
            case 0x01:  // Toggle Pump
                PORTD ^= (1<<PORTD2);
                break;
            case 0x02:  // Toggle Humidifier 
                PORTD ^= (1<<PORTD3);
                break;
            case 0x03:  // Toggle Fan 
                PORTD ^= (1<<PORTD4);
                break;
            case 0x04:  // Toggle Grow Light 1 
                PORTD ^= (1<<PORTD5);
                break;
           
            // ===== EXPLICIT PUMP CONTROL (Auto Mode) =====
            case 0x10:  // Pump OFF
                PORTD &= ~(1<<PORTD2);
                break;
            case 0x11:  // Pump ON
                PORTD |= (1<<PORTD2);
                break;

            // ===== EXPLICIT HUMIDIFIER CONTROL (Auto Mode) =====
            case 0x12:  // Humidifier OFF (WAS FAN)
                PORTD &= ~(1<<PORTD3);
                break;
            case 0x13:  // Humidifier ON (WAS FAN)
                PORTD |= (1<<PORTD3);
                break;

            // ===== EXPLICIT FAN CONTROL (Auto Mode) =====
            case 0x14:  // Fan OFF 
                PORTD &= ~(1<<PORTD4);
                break;
            case 0x15:  // Fan ON 
                PORTD |= (1<<PORTD4);
                break;

            // ===== EXPLICIT LIGHT 1 CONTROL (Auto Mode) =====
            case 0x16:  // Light 1 OFF 
                PORTD &= ~(1<<PORTD5);
                break;
            case 0x17:  // Light 1 ON 
                PORTD |= (1<<PORTD5);
                break;
           
           default:
               // Unknown command - ignore
               break;
       }
   }
   }
else
   {
   // Receive error
   return false;
   }

if (rx_complete) return false;

return (twi_rx_index<sizeof(twi_rx_buffer));
}

// TWI Slave transmission handler
// This handler is called for the first time when the
// transmission from the TWI slave to the master
// is about to begin, returning the number of bytes
// that need to be transmitted
// The second time the handler is called when the
// transmission has finished
// In this case it must return 0
unsigned char twi_tx_handler(bool tx_complete)
{
if (tx_complete==false)
   {
   // Transmission from slave to master is about to start
   // Return the number of bytes to transmit
   return sizeof(twi_tx_buffer);
   }

// Transmission from slave to master has finished
// Place code here to eventually process data from
// the twi_rx_buffer, if it wasn't yet processed
// in the twi_rx_handler

// No more bytes to send in this transaction
return 0;
}

void main(void)
{
// Declare your local variables here

// Input/Output Ports initialization
// Port A initialization
// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In 
DDRA=(0<<DDA7) | (0<<DDA6) | (0<<DDA5) | (0<<DDA4) | (0<<DDA3) | (0<<DDA2) | (0<<DDA1) | (0<<DDA0);
// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T 
PORTA=(0<<PORTA7) | (0<<PORTA6) | (0<<PORTA5) | (0<<PORTA4) | (0<<PORTA3) | (0<<PORTA2) | (0<<PORTA1) | (0<<PORTA0);

// Port B initialization
// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In 
DDRB=(0<<DDB7) | (0<<DDB6) | (0<<DDB5) | (0<<DDB4) | (0<<DDB3) | (0<<DDB2) | (0<<DDB1) | (0<<DDB0);
// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T 
PORTB=(0<<PORTB7) | (0<<PORTB6) | (0<<PORTB5) | (0<<PORTB4) | (0<<PORTB3) | (0<<PORTB2) | (0<<PORTB1) | (0<<PORTB0);

// Port C initialization
// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In 
DDRC=(0<<DDC7) | (0<<DDC6) | (0<<DDC5) | (0<<DDC4) | (0<<DDC3) | (0<<DDC2) | (0<<DDC1) | (0<<DDC0);
// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T 
PORTC=(0<<PORTC7) | (0<<PORTC6) | (0<<PORTC5) | (0<<PORTC4) | (0<<PORTC3) | (0<<PORTC2) | (0<<PORTC1) | (0<<PORTC0);

// Port D initialization
// Function: Bit7=In Bit6=In Bit5=Out Bit4=Out Bit3=Out Bit2=Out Bit1=In Bit0=In 
DDRD=(0<<DDD7) | (0<<DDD6) | (1<<DDD5) | (1<<DDD4) | (1<<DDD3) | (1<<DDD2) | (0<<DDD1) | (0<<DDD0);
// State: Bit7=T Bit6=T Bit5=0 Bit4=0 Bit3=0 Bit2=0 Bit1=T Bit0=T 
PORTD=(0<<PORTD7) | (0<<PORTD6) | (0<<PORTD5) | (0<<PORTD4) | (0<<PORTD3) | (0<<PORTD2) | (0<<PORTD1) | (0<<PORTD0);

// ADC initialization
// ADC Clock frequency: 1000.000 kHz
// ADC Voltage Reference: AVCC pin
// ADC Auto Trigger Source: Free Running
ADMUX=ADC_VREF_TYPE;
ADCSRA=(1<<ADEN) | (0<<ADSC) | (1<<ADATE) | (0<<ADIF) | (0<<ADIE) | (0<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);
SFIOR=(0<<ADTS2) | (0<<ADTS1) | (0<<ADTS0);

// TWI initialization
// Mode: TWI Slave
// Match Any Slave Address: Off
// I2C Bus Slave Address: 0x08
twi_slave_init(false,0x08,twi_rx_buffer,sizeof(twi_rx_buffer),twi_tx_buffer,twi_rx_handler,twi_tx_handler);

// Global enable interrupts
#asm("sei")

while (1) {
        adc_values[0] = average_adc(0, 100);  // PA0/ADC0, average 8 samples
        adc_values[1] = average_adc(1, 100);  // PA1/ADC1
        adc_values[2] = average_adc(2, 100);  // PA2/ADC2

        
        // Pack into 6-byte TX buffer: high/low per ADC (big-endian)
        twi_tx_buffer[0] = (adc_values[0] >> 8) & 0xFF;  // ADC0 high
        twi_tx_buffer[1] = adc_values[0] & 0xFF;         // ADC0 low
        twi_tx_buffer[2] = (adc_values[1] >> 8) & 0xFF;  // ADC1 high
        twi_tx_buffer[3] = adc_values[1] & 0xFF;         // ADC1 low
        twi_tx_buffer[4] = (adc_values[2] >> 8) & 0xFF;  // ADC2 high
        twi_tx_buffer[5] = adc_values[2] & 0xFF;         // ADC2 low

      }
}
