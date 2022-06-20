#include "avr/io.h"

#include "avr/interrupt.h"

#include "stdlib.h"


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

int main(void) {
    // Slow timer init. Game logic timer
    TCCR1A = (1 << WGM10) | (0 << WGM11);
    TCCR1B = (0 << WGM12) | (1 << WGM13) | (1 << CS12) | (0 << CS11) | (1 << CS10);
    TIMSK1 = 1 << OCIE1A; // interrupt en
    OCR1A = 128; // blink delay

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

    //sei();
    number_left_7SEG = 0;
    number_right_7SEG = 0;
    while (1) {
        // Game logic
        UARTsendString(UART_START_GAME); // Game start message
        while ((UCSR1A & (1 << RXC1)) == 0); // Wait for user to start
        uint8_t user_input = UDR1;
        if (user_input == 'S') {
            game_start_flag = 1;
        }
        if (game_start_flag) {
            getRandNumbers(random_values);
            // pseudokood
            for number in random_values:
                show_on_7SEG(number)
            delay(timer1 used
                for delay, OCR1A m22rab delayt)

            for value in random_values:
                input = read_uart
            if input == value:
                score++ -> EPROM
            else :
                game_over
            UARTsendString(UART_LOSE_GAME);
            UARTsendString(UART_WIN_GAME);
        }
        EPROM_Show_score

        /*// Random value generator example. Random seed generated in TIMER0_OVF_vect
        uint8_t x = rand();
        PORTA = x;
        rand_seed++;
        UARTsendByte(x);
		*/

    }
}