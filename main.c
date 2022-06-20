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

// 7SEG variables
uint8_t counter_flip_7SEG = 0; // counts, which 7SEG left to show. Left or right.
uint8_t number_right_7SEG = 0;
uint8_t number_left_7SEG = 0;
uint8_t numbers_7SEG[10] = {
    0b11111100,
    0b01100000,
    0b11011010,
    0b11110010,
    0b01100110,
    0b10110110,
    0b10111110,
    0b11100000,
    0b11111110,
    0b11110110
};

// UART Feedback messages
char UART_START_GAME[] = "Memory game: In order to start, send <S>  \0";
char UART_LOSE_GAME[] = "You lost. Play again?  \0";
char UART_WIN_GAME[] = "You won! +10 points. Total score:  \0";
char UART_SELECT_DIFFICULTY_GAME[] = "Select difficulty from 1 to 5, send <num>  \0";
char GAME_START_CHAR = 'S';

// Game logic variables
uint64_t rand_seed = 0;
uint8_t random_values[10];
uint8_t game_start_flag = 0;

void getRandNumbers(uint8_t array, uint8_t size){
	for (uint8_t i = 0; i < size; i++) {
		uint8_t x;
		while (x > MAX_GAME_NUMBER) {
			x = rand();
		}
		array[i] = x;
	}
}

void UARTsendByte(char data) {
    while (!(UCSR1A & 1 << UDRE1));
    UDR1 = data;
}

void UARTsendString(char *str) {
    char *ptr = str;
    while (*ptr != 0) {
        UARTsendByte(*ptr);
        ptr++;
    }
}

void UARTsendMSG(uint64_t decoded_data, uint8_t bytes_in_msg) {
    /*
    Function that sends received IR data via UART. Unpacks 64bit data into ${bytes_in_msg} separate bytes. 5 in this project.
    */
    for (uint8_t i = 0; i < bytes_in_msg; i++) {
        UARTsendByte(decoded_data >> (8 * i) & 0xFF);
    }
}

void shiftByteOut(uint8_t number) {
    /*  
    Function for TIMER0_OVF_vect, that helps show numbers on 7SEG led. 
    */
    for (uint8_t i = 8; i > 0; i--) {
        if (number & 1) {
            PORTE = 1 << PE4;
            PORTE = (1 << PE4) | (1 << PE3);
        } else {
            PORTE = 0 << PE4;
            PORTE = (0 << PE4) | (1 << PE3);
        }
        number >>= 1;
    }
    PORTB = 1 << PB7;
    PORTB = 0;
}

ISR(TIMER0_OVF_vect) {
    /*
    7SEG fast timer. Shows numbers on 7SEG. Also, here update random number seed.
    */
    counter_flip_7SEG ^= 0xFF;
    if (counter_flip_7SEG) {
        PORTD = 1 << PD4;
        shiftByteOut(numbers_7SEG[number_right_7SEG]);
    } else {
        PORTD = 0;
        shiftByteOut(numbers_7SEG[number_left_7SEG]);
    }
	
	// Random seed update
	rand_seed++;
	srand(rand_seed);
}

ISR(TIMER1_COMPA_vect) {
    /*
    7SEG slow timer. Counts numbers what to show. 
    TODO use this timer for game logic to show numbers.
    */
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

    // Slow timer init. Game logic timer
    TCCR1A = (1 << WGM10) | (0 << WGM11);
    TCCR1B = (0 << WGM12) | (1 << WGM13) | (1 << CS12) | (0 << CS11) | (1 << CS10);
    TIMSK1 = 1 << OCIE1A; // interrupt en
    OCR1A = 128;		  // blink delay

    // fast timer
    TCCR0A = 0;
    TCCR0B = (0 << CS02) | (1 << CS01) | (1 << CS00);
    TIMSK0 = (1 << TOIE0); // overflow interrupt en

    // 7 segment init
    DDRE |= (1 << PE4) | (1 << PE3); // DS and SHCP
    PORTE = 0;
    DDRB = 1 << PB7;
    PORTB = 1 << PB7;
    DDRD = 1 << PD4;

    // UART init
    UCSR1B = 1 << TXEN1 | 1 << RXEN1;
    UCSR1C = 1 << UCSZ11 | 1 << UCSZ10; // char size = 8 bit
    UBRR1 = 12; // 9600 baud rate

    // LED. Blinks when IR received
    DDRA = 0xFF;

    //sei();
	number_left_7SEG = 0;
	number_right_7SEG = 0;
    while (1) {
        // Game logic
		UARTsendString(UART_START_GAME);           // Game start message
		while ((UCSR1A & (1 << RXC1)) == 0);       // Wait for user to start
		uint8_t user_input = UDR1;
		if (user_input == 'S') {
			game_start_flag = 1;
		}
		if (game_start_flag) {
			getRandNumbers(random_values);
		}
		// TODO SHOW NUMBER ON LED
		while ((UCSR1A & (1 << RXC1)) == 0); // Wait for user
	
		/*// Random value generator example. Random seed generated in TIMER0_OVF_vect
        uint8_t x = rand();
        PORTA = x;
        rand_seed++;
        UARTsendByte(x);
		*/

    }
}
