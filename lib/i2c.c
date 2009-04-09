/**
 * Copyright (C) 2008 Ubixum, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 **/

#include <stdio.h> // NOTE this needs deleted

#include <fx2regs.h>
#include <fx2macros.h>
#include <i2c.h>
#include <delay.h>


//#define DEBUG_I2C 1

#ifdef DEBUG_I2C
#define i2c_printf(...) printf(__VA_ARGS__)
#else
#define i2c_printf(...)
#endif


/**
 *
    1. Set START=1. If BERR=1, start timer*.
    2. Write the 7-bit peripheral address and the direction bit (0 for a write) to I2DAT.
    3. Wait for DONE=1 or for timer to expire*. If BERR=1, go to step 1.
    4. If ACK=0, go to step 9.
    5. Load I2DAT with a data byte.
    6. Wait for DONE=1*. If BERR=1, go to step 1.
    7. If ACK=0, go to step 9.
    8. Repeat steps 5-7 for each byte until all bytes have been transferred.
    9. Set STOP=1. Wait for STOP = 0 before initiating another transfer.
 **/
BOOL i2c_write ( BYTE addr, WORD len, BYTE *dat ) {
    
    WORD cur_byte;
    //BOOL wait=FALSE; // use timer if needed
    
    step9:
    // 9. Set STOP=1. Wait for STOP = 0 before initiating another transfer.
    while ( I2CS & bmSTOP ); // WAIT for STOP = 0 before another transfer
    
    // 1. Set START=1. If BERR=1, start timer*.
    step1:
        cur_byte=0;
        I2CS |= bmSTART;
        if ( I2CS & bmBERR ) {
            i2c_printf ( "Woops.. need to do the timer\n" );
            delay(10); // way too long
            goto step1;
            }
    
    // 2. Write the 7-bit peripheral address and the direction bit (0 for a write) to I2DAT.
        I2DAT = addr << 1;
        
    // 3. Wait for DONE=1 or for timer to expire*. If BERR=1, go to step 1.
        while ( !(I2CS & bmDONE) );
        if (I2CS&bmBERR) {
            i2c_printf ( "bmBERR, going to step 1\n" );
            goto step1;
        }
        
    // 4. If ACK=0, go to step 9.
    if ( !(I2CS & bmACK) ) {
        I2CS |= bmSTOP;
        i2c_printf ( "No ack after writing address.! Fail\n");
        goto step9;
 //       return FALSE; 
    }
    
    // 8. Repeat steps 5-7 for each byte until all bytes have been transferred.
    while ( cur_byte < len ) {
        // 5. Load I2DAT with a data byte.
        I2DAT = dat[cur_byte++];
        // 6. Wait for DONE=1*. If BERR=1, go to step 1.
        while (!(I2CS&bmDONE)); if ( I2CS&bmBERR ) {
         i2c_printf ( "bmBERR on byte %d. Going to step 1\n" , cur_byte-1 );
         goto step1;
        }
        // 7. If ACK=0, go to step 9.
        if ( !(I2CS & bmACK) ) {
            I2CS |= bmSTOP;
            i2c_printf ( "No Ack after byte %d. Fail\n", cur_byte-1 );
            return FALSE; 
        }
    }
    
    //real step 9
    I2CS |= bmSTOP;


    return TRUE;

}

/*
 trm 13.4.4
 
    1. Set START=1. If BERR = 1, start timer*.
    2. Write the 7-bit peripheral address and the direction bit (1 for a read) to I2DAT.
    3. Wait for DONE=1 or for timer to expire*. If BERR=1, go to step 1.
    4. If ACK=0, set STOP=1 and go to step 15.
    5. Read I2DAT to initiate the first burst of nine SCL pulses to clock in the first byte from the slave.
    Discard the value that was read from I2DAT.
    6. Wait for DONE=1. If BERR=1, go to step 1.
    7. Read the just-received byte of data from I2DAT. This read also initiates the next read transfer.
    8. Repeat steps 6 and 7 for each byte until ready to read the second-to-last byte.
    9. Wait for DONE=1. If BERR=1, go to step 1.
    10. Before reading the second-to-last I2DAT byte, set LASTRD=1.
    11. Read the second-to-last byte from I2DAT. With LASTRD=1, this initiates the final byte read on
    the bus.
    12. Wait for DONE=1. If BERR=1, go to step 1.
    13. Set STOP=1.
    14. Read the final byte from I2DAT immediately (the next instruction) after setting the STOP bit. By
    reading I2DAT while the "stop" condition is being generated, the just-received data byte will be
    retrieved without initiating an extra read transaction (nine more SCL pulses) on the I²Cbus.
    15. Wait for STOP = 0 before initiating another transfer
*/

/*
  * timer should be at least as long as longest start-stop interval on the bus
  serial clock for i2c bus runs at 100khz by default and can run at 400khz for devices that support it
  start-stop interval is about 9 serial clock cycles
  400KHZ bit 0=100khz, 1=400khz
  
  how many cycles at XTAL cycles/second = 9 cycles at 400k (or 100k) cycles/second
  
  timeout = n i2c cycles / I2C cycles/sec = timeout seconds
          timeout seconds * XTAL cycles/sec = XTAL cycles
          9 / 400 (or 100) * (XTAL)
  
*/
BOOL i2c_read( BYTE addr, WORD len, BYTE* buf) {
    
    
    BYTE tmp;
    WORD cur_byte;
    //WORD timeout_cycles = (WORD)(9.0 * XTAL / I2CFREQ );
    
    /*printf ( "I2C: %04x%04x\nXTAL: %04x%04x\ntimeout cycles: %d\n",
            (WORD)(I2CFREQ>>16), (WORD)(I2CFREQ&0xffff),
            (WORD)(XTAL>>16),
            (WORD)(XTAL&0xffff),
            timeout_cycles ); */
    
    
    // step 15 1st.
    while ( I2CS & bmSTOP);
        
    
    // 1. Set START=1. If BERR = 1, start timer*.
    start:
        cur_byte=0;        
        I2CS |= bmSTART;
        if ( I2CS & bmBERR ) {            
            i2c_printf ( "Woops, step1 BERR, need to do timeout\n");
            delay(10); // NOTE way too long
            goto start;
        }
        
    // 2. Write the 7-bit peripheral address and the direction bit (1 for a read) to I2DAT.
        I2DAT = (addr << 1) | 1; // last 1 for read
    
    // 3. Wait for DONE=1 or for timer to expire*. If BERR=1, go to step 1.
                
        while ( !(I2CS & bmDONE) );        
        if ( I2CS & bmBERR )
            goto start;
            
    // 4. If ACK=0, set STOP=1 and go to step 15.
        if (!(I2CS&bmACK) ) {
            I2CS |= bmSTOP;
            return FALSE; 
        }
        
    // with only one byte to read, this needs set here.
    // (In this case, the tmp read is the 2nd to last read)
    if ( len==1 ) I2CS |= bmLASTRD; 
        
    // 5. Read I2DAT to initiate the first burst of nine SCL pulses to clock in the first byte from the slave.
    //    Discard the value that was read from I2DAT.
        tmp = I2DAT; // discard read
    
    while (len>cur_byte+1) { // reserve last byte read for after the loop
        
        // 6. Wait for DONE=1. If BERR=1, go to step 1.
        // 9. Wait for DONE=1. If BERR=1, go to step 1.
        while (!(I2CS&bmDONE)); if ( I2CS&bmBERR ) goto start;

        // 10. Before reading the second-to-last I2DAT byte, set LASTRD=1.
        if (len==cur_byte+2) // 2nd to last byte
            I2CS |= bmLASTRD;
        
        // 7. Read the just-received byte of data from I2DAT. This read also initiates the next read transfer.
        // 11. Read the second-to-last byte from I2DAT. With LASTRD=1, this initiates the final byte read on
        //     the bus.
            buf[cur_byte++] = I2DAT;
                        
        // 8. Repeat steps 6 and 7 for each byte until ready to read the second-to-last byte.
    }
    
    //12. Wait for DONE=1. If BERR=1, go to step 1.
        while (!(I2CS&bmDONE)); if ( I2CS&bmBERR ) goto start;
    // 13. Set STOP=1.
        I2CS |= bmSTOP;
    // 14. Read the final byte from I2DAT immediately (the next instruction) after setting the STOP bit. By
    // reading I2DAT while the "stop" condition is being generated, the just-received data byte will be
    // retrieved without initiating an extra read transaction (nine more SCL pulses) on the I²Cbus.
        buf[cur_byte] = I2DAT; // use instead of buffer addressing so next instruction reads I2DAT


    return TRUE;
}



BOOL eeprom_write(BYTE prom_addr, WORD addr, WORD length, BYTE* buf) {
    BYTE addr_len=0;
    // 1st bytes of buffer are address and next byte is value
    BYTE data_buffer[3];
    WORD cur_byte=0;
    
    while ( cur_byte<length ) {
        addr_len=0;
        if (EEPROM_TWO_BYTE)
            data_buffer[addr_len++] = MSB(addr);
        data_buffer[addr_len++] = LSB(addr);
        data_buffer[addr_len++] = buf[cur_byte++];

        i2c_printf ( "%02x " , data_buffer[addr_len-1] );
        
        if ( ! i2c_write ( prom_addr, addr_len, data_buffer ) ) return FALSE;
        ++addr; // next byte goes to next address
    }

    return TRUE;
    
}


BOOL eeprom_read (BYTE prom_addr, WORD addr, WORD length, BYTE *buf)
{

    BYTE eeprom_addr[2];
    BYTE addr_len=0;
    if (EEPROM_TWO_BYTE) 
        eeprom_addr[addr_len++] = MSB(addr);
    
    eeprom_addr[addr_len++] = LSB(addr);

    // write the address we want to read to the prom
    //printf ("Starting Addr Write with addr len %d\n", addr_len);
    if ( !i2c_write( prom_addr, addr_len, eeprom_addr ) ) return FALSE;
    //printf ( "Starting read\n" );
    if ( !i2c_read ( prom_addr, length, buf ) ) return FALSE;

    return TRUE;
    
}

