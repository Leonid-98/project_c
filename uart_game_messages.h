#ifndef UART_GAME_MESSAGES_H_
#define UART_GAME_MESSAGES_H_

char MSG_START_GAME[] = "Memory game: In order to start, send <1>  \0";
char MSG_LOSE_GAME[] = "You lost. Total score: \0";
char MSG_WIN_GAME[] = "You won! +10 points. Total score: \0";
char MSG_SELECT_DIFFICULTY[] = "Select difficulty from 1 to 3, send <num>  \0";
char MSG_SPACES[] = "         \0";
char GAME_START_CHAR = 'S';

void UARTsendByte(char data) {
	while (!(UCSR1A & 1 << UDRE1));
	UDR1 = data;
}

void UARTsendString(char *str) {
	char *ptr = str;
	while ( *ptr != 0) {
		UARTsendByte( * ptr);
		ptr++;
	}
}
#endif /* UART_GAME_MESSAGES_H_ */