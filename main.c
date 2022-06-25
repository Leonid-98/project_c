#include "avr/io.h"
#include "avr/interrupt.h"
#include "stdlib.h"
#include "util/delay.h"
#include "seven_segment.h"
#include "uart_game_messages.h"

#define F_CPU 2000000

#define GAME_NUMBERS_SIZE 10
#define ASCII_ZERO 48

// State machine variables
#define STATE_GAME_START 1
#define STATE_WAITING_TO_START 2
#define STATE_SELECT_DIFF 3
#define STATE_WAITING_FOR_DIFF 4
#define STATE_SHOW_NUMBERS 5
#define STATE_WAITING_FOR_INPUT 6
#define STATE_GAME_RESULT 7

// Delay in ms between numbers that shown in 7SEG led
#define DIFFICULTY_EASY 2000
#define DIFFICULTY_NORMAL 1500
#define DIFFICULTY_HARD 750

uint8_t state = STATE_GAME_START;

uint16_t gameDelay = 0;

uint64_t rand_seed = 0;
uint8_t random_values[GAME_NUMBERS_SIZE];

uint8_t received_values[GAME_NUMBERS_SIZE];
uint8_t uartRxCounter = 0;

uint8_t getRand() {
	/*
	Returns random value < MAX_GAME_NUMBER (In order to fit on 7SEG led)
	*/
	uint8_t val = rand();
	if (val > MAX_SSEG_NUMBER) 
		while (val > MAX_SSEG_NUMBER) 
			val /= 10;
	return val;
}

void fillArrayRand(uint8_t *array, uint8_t size) {
	/* 
	Fills arrays with *unique* random values generated in getRand().
	*/
	for (uint8_t i = 0; i < size; i++) {
		uint8_t x = getRand();
		if (i > 0) {
			while (x == array[i-1]) {
				x = getRand();
			}
		}
		array[i] = x;
	}
}


ISR(TIMER0_OVF_vect) {
    /*
    7SEG fast timer. Shows numbers on 7SEG. Also, here update random number seed.
    */
    counter_flip_7SEG ^= 0xFF;
    if (counter_flip_7SEG) {
        PORTD = 1 << PD4;
        SSEG_shiftByteOut(numbers_7SEG[number_right_7SEG]);
    } else {
        PORTD = 0;
        SSEG_shiftByteOut(numbers_7SEG[number_left_7SEG]);
    }

    // Random seed update
    rand_seed++;
    srand(rand_seed + rand());
}


ISR(USART1_RX_vect) {
	uint8_t rxData = UDR1;
	
	if (state == STATE_WAITING_TO_START) {
		if (rxData == 1) {
			state = STATE_SELECT_DIFF;
		}
	}
	else if (state == STATE_WAITING_FOR_DIFF) {
		switch (rxData) {
			case 1:
				gameDelay = DIFFICULTY_EASY;
				break;
			case 2:
				gameDelay = DIFFICULTY_NORMAL;
				break;
			case 3:
				gameDelay = DIFFICULTY_HARD;
				break;
			default:
				gameDelay = DIFFICULTY_EASY;
		}
		state = STATE_SHOW_NUMBERS;
	}
	else if (state == STATE_WAITING_FOR_INPUT) {
		received_values[uartRxCounter] = rxData;
		uartRxCounter++;
		
		if (uartRxCounter >= GAME_NUMBERS_SIZE) {
			uartRxCounter = 0;
			state = STATE_GAME_RESULT;
		}
	}
	
}


int main(void) {
	// fast timer to show 7SEG numbers
	TCCR0A = 0;
	TCCR0B = (0 << CS02) | (1 << CS01) | (1 << CS00);
	TIMSK0 = (1 << TOIE0); // overflow interrupt en
	
	// Slow timer init. Game logic timer. (delay func)
	TCCR1A = 0;
	TCCR1B = (1<<CS12) | (0<<CS11) | (1<<CS10);
	
	// UART init
	UCSR1B = 1 << TXEN1 | 1 << RXEN1 | 1<<RXCIE1;
	UCSR1C = 1 << UCSZ11 | 1 << UCSZ10; // char size = 8 bit
	UBRR1 = 12; // 9600 baud rate

	// 7 segment init
	DDRE |= (1 << PE4) | (1 << PE3); // DS and SHCP
	PORTE = 0;
	DDRB = 1 << PB7;
	PORTB = 1 << PB7;
	DDRD = 1 << PD4;
	
	DDRA = 0xFF;
	
	sei();
	
	SSEG_turnOff();
	uint8_t isGameWon;
	uint16_t score = 0;
	while (1) {
		
		if (state == STATE_GAME_START) {
			isGameWon = 1;
			UARTsendString(MSG_START_GAME);
			state = STATE_WAITING_TO_START;
		}
		else if (state == STATE_SELECT_DIFF) {
			UARTsendString(MSG_SELECT_DIFFICULTY);
			state = STATE_WAITING_FOR_DIFF;
		}
		else if (state == STATE_SHOW_NUMBERS) {
			fillArrayRand(random_values, GAME_NUMBERS_SIZE);
			for (uint8_t i = 0; i < GAME_NUMBERS_SIZE; i++) {
				SSEG_showNumber(random_values[i]);
				// _delay_ms want constant as argument
				switch (gameDelay) {
					case DIFFICULTY_EASY:
						_delay_ms(DIFFICULTY_EASY);
						break;
					case DIFFICULTY_NORMAL:
						_delay_ms(DIFFICULTY_NORMAL);
						break;
					case DIFFICULTY_HARD:
						_delay_ms(DIFFICULTY_HARD);
						break;
				}
			}
			SSEG_turnOff();
			state = STATE_WAITING_FOR_INPUT;
		}
		else if (state == STATE_GAME_RESULT) {
			for (uint8_t i = 0; i < GAME_NUMBERS_SIZE; i++) {
				if (random_values[i] != received_values[i]) {
					isGameWon = 0;
				} 
			}
			if (isGameWon) {
				UARTsendString(MSG_WIN_GAME);
				score += 1;
			}
			else {
				UARTsendString(MSG_LOSE_GAME);
				score = 0;
			}
			UARTsendByte(score + ASCII_ZERO);
			UARTsendString(MSG_SPACES);
			state = STATE_GAME_START;
		}
	}
}