#include "avr/io.h"

#include "avr/interrupt.h"

#include "stdlib.h"

#define GAME_NUMBERS_SIZE 10
#define MAX_GAME_NUMBER 99

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
uint8_t random_values[GAME_NUMBERS_SIZE];
uint8_t game_start_flag = 0;
uint8_t user_input;
uint16_t timer_delay;
uint8_t game_win_flag = 1;

void delay_1ms(){
	TIFR0 = 1<<OCF0B;
	TCNT0 = 0;
	OCR0B = 1;
	
	while (!(TIFR0 & (1 << OCF0B)));
	TIFR0 = (1 << OCF0B);
	
}

void delay_ms(uint16_t time){
    for (uint16_t i = 0; i < time; i++) {
        delay_1ms();
    }
}

void getRandNumbers(uint8_t array, uint8_t size) {
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

void UARTsendString(char * str) {
    char * ptr = str;
    while ( * ptr != 0) {
        UARTsendByte( * ptr);
        ptr++;
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

uint8_t str_to_number(char number) {
    /* 
    Works only with ASCII numbers represented as char
    */
    uint8_t ascii_zero = 48;
    return number - ascii_zero;
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

int main(void) {
    // Slow timer init. Game logic timer. (delay func)
    TCCR1A = 0;
	TCCR1B = (1<<CS12) | (0<<CS11) | (1<<CS10);

    // fast timer to show 7SEG numbers
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

    DDRA = 0xFF;

    number_left_7SEG = 0;
    number_right_7SEG = 0;

    sei();
    while (1) {
        // Game logic
        UARTsendString(UART_START_GAME); // Game start message
        while ((UCSR1A & (1 << RXC1)) == 0); // Wait for user to start
        user_input = UDR1;
        if (user_input == 'S') {
            game_start_flag = 1;
        }
        if (game_start_flag) {
            // Select difficulty == speed of the timer
            UARTsendString(UART_SELECT_DIFFICULTY_GAME);
            while ((UCSR1A & (1 << RXC1)) == 0);
            user_input = UDR1;
            switch (user_input) {
                case '1':
                    timer_delay = 500;
                    break;
                case '2':
                    timer_delay = 350;
                    break;
                case '3':
                    timer_delay = 250;
                    break;
                case '4':
                    timer_delay = 100;
                    break;
                case '5':
                    timer_delay = 50;
                    break;
                default:
                    timer_delay = 500;
            }

            // Show random numbers on the 7 seg
            getRandNumbers(random_values, GAME_NUMBERS_SIZE);
            for (uint8_t i = 0; i < GAME_NUMBERS_SIZE; i++) {
                number_left_7SEG = random_values[i] / 10;
                number_right_7SEG = random_values[i] - (random_values[i] / 10) * 10;
                delay_ms(timer_delay);
            }
            // Check if user inputs correct numbers
            for (uint8_t i = 0; i < GAME_NUMBERS_SIZE; i++) {
                while ((UCSR1A & (1 << RXC1)) == 0);
                user_input = UDR1;
                if (str_to_number(user_input) != random_values[i]) {
                    game_win_flag = 0;
                }
            }

            if (game_win_flag) {
                UARTsendString(UART_WIN_GAME)
            } else {
                UARTsendString(UART_LOSE_GAME)
            }    
            game_win_flag = 1;
        }
        //EPROM_Show_score

        /*// Random value generator example. Random seed generated in TIMER0_OVF_vect
        uint8_t x = rand();
        PORTA = x;
        rand_seed++;
        UARTsendByte(x);
		*/

    }
}
