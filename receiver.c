#include "avr/io.h"

#include "avr/interrupt.h"

#include "stdlib.h"


#define BUFFER_SIZE 128
#define DIFF_BETWEEN_PULSES 3
#define BYTES_IN_MSG 5
#define MAX_GAME_NUMBER 99

// IR receiver variables
uint8_t read_data_flag = 0;
uint16_t read_data_counter = 0;
uint8_t buffer1[BUFFER_SIZE];
uint8_t buffer2[BUFFER_SIZE];
uint8_t buffer3[BUFFER_SIZE];
uint8_t buffer4[BUFFER_SIZE];
uint8_t ir_data;
char serial_debug_data;

void UARTsendMSG(uint64_t decoded_data, uint8_t bytes_in_msg) {
    /*
    Function that sends received IR data via UART. Unpacks 64bit data into ${bytes_in_msg} separate bytes. 5 in this project.
    */
    for (uint8_t i = 0; i < bytes_in_msg; i++) {
        UARTsendByte(decoded_data >> (8 * i) & 0xFF);
    }
}

ISR(TIMER3_COMPA_vect) {
    /*
    IR receiver timer. Decodes IR signal using NEC protocol at frequency ~200 kHz
    */

    if (PINE & 1 << PE5) {
        ir_data = 0x00;
    } else {
        ir_data = 0x01;
        read_data_flag = 1;
    }

    PORTA = ir_data;
    if (read_data_flag == 1) {
        // Fill all buffers with serial data
        if (read_data_counter < BUFFER_SIZE) {
            buffer1[read_data_counter] = ir_data;
            read_data_counter++;
        } else if (read_data_counter < BUFFER_SIZE * 2) {
            buffer2[read_data_counter - BUFFER_SIZE] = ir_data;
            read_data_counter++;
        } else if (read_data_counter < BUFFER_SIZE * 3) {
            buffer3[read_data_counter - BUFFER_SIZE * 2] = ir_data;
            read_data_counter++;
        } else if (read_data_counter < BUFFER_SIZE * 4) {
            buffer4[read_data_counter - BUFFER_SIZE * 3] = ir_data;
            read_data_counter++;
        }
        // Decode data when all buffers is full
        else {
            uint16_t ones_counter = 0;
            uint16_t zeroes_counter = 0;
            uint8_t prev_val = 1;
            uint8_t current_val;
            uint64_t decoded_value = 0;

            for (uint16_t buffer_counter = 0; buffer_counter < BUFFER_SIZE * 4; buffer_counter++) {
                // Select which buffer to read
                if (buffer_counter < BUFFER_SIZE) {
                    current_val = buffer1[buffer_counter];
                } else if (buffer_counter < BUFFER_SIZE * 2) {
                    current_val = buffer2[buffer_counter - BUFFER_SIZE];
                } else if (buffer_counter < BUFFER_SIZE * 3) {
                    current_val = buffer3[buffer_counter - BUFFER_SIZE * 2];
                } else if (buffer_counter < BUFFER_SIZE * 4) {
                    current_val = buffer4[buffer_counter - BUFFER_SIZE * 3];
                }

                // Decode data from buffer
                if ((prev_val == 0) && (current_val == 1)) {
                    // If low and high pulse duration is different more, than tolerance (DIFF_BETWEEN_PULSES), decode logical 1
                    if ((ones_counter > zeroes_counter) && (ones_counter - zeroes_counter > DIFF_BETWEEN_PULSES)) {
                        decoded_value |= 1;
                    }
                    decoded_value <<= 1;
                    zeroes_counter = 0;
                    ones_counter = 1;
                } else if (current_val == 0) {
                    zeroes_counter++;
                } else if (current_val == 1) {
                    ones_counter++;
                }
                prev_val = current_val;
            }

            UARTsendMSG(decoded_value, BYTES_IN_MSG);
            read_data_counter = 0;
            read_data_flag = 0;
        }
    }
}

int main(void) {
    // Timer init (PWM, phase and frequency Correct), no prescaler
    TCCR3A = 1 << WGM30 | 0 << WGM31;
    TCCR3B = 0 << WGM32 | 1 << WGM33 | 0 << CS32 | 0 << CS31 | 1 << CS30;
    TIMSK3 = 1 << OCIE3A;
    OCR3A = 10;

    // IR receiver
    DDRE &= ~(1 << PE5);

    // LED. Blinks when IR received
    DDRA = 0xFF;

    sei();
    while (1);
}