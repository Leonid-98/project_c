#ifndef SEVEN_SEGMENT_H_
#define SEVEN_SEGMENT_H_

#define SSEG_NOTHING 10 // index in numbers_7SEG, where all diodes is 0
#define MAX_SSEG_NUMBER 99

// 7SEG variables
uint8_t counter_flip_7SEG = 0; // counts, which 7SEG left to show. Left or right.
uint8_t number_right_7SEG = 0;
uint8_t number_left_7SEG = 0;
uint8_t numbers_7SEG[11] = {
	0b11111100,
	0b01100000,
	0b11011010,
	0b11110010,
	0b01100110,
	0b10110110,
	0b10111110,
	0b11100000,
	0b11111110,
	0b11110110,
	0b00000000
};

void SSEG_shiftByteOut(uint8_t number) {
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

void SSEG_showNumber(uint8_t number) {
	/*
	Shows number on 7SEG. Make sure number <= 99.
	*/
	number_left_7SEG = number / 10;
	number_right_7SEG = number - (number / 10) * 10;
}

void SSEG_turnOff() {
	number_left_7SEG = SSEG_NOTHING;
	number_right_7SEG = SSEG_NOTHING;
}

#endif /* SEVEN_SEGMENT_H_ */