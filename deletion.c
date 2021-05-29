#include <stdio.h>

#define reg 0b01010101010

int main(void) {
	int new_reg = reg | 0b00000001;

	printf("%u",new_reg);

	return 0;
}
