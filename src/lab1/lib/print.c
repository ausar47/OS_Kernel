#include "print.h"
#include "sbi.h"

void puts(char *s) {
    while (*s != '\0') {
        sbi_ecall(0x1, 0x0, *s, 0, 0, 0, 0, 0);	// *s is the character pointed to by s
	s++;	// move pointer s to the next character
    }
}

void puti(int x) {
    int digit = 1, tmp = x;
    while (tmp >= 10) {
        digit *= 10;
	tmp /= 10;
    }	// digit means how many digits x has got
    while (digit >= 1) {
	char c = x / digit + 48;	// begin from the first digit, we get it and convert it to ASCII code
        sbi_ecall(0x1, 0x0, c, 0, 0, 0, 0, 0);
	x %= digit;	// x erases the first digit
	digit /= 10;	// now that x has erased the first digit, its digit also decreases
    }
}
