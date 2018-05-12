#include <stdio.h>

void iLoveSexN(int n);

int main(int argc, char** arv) {

	iLoveSexN(10);
	return 0;
}

void iLoveSexN(int n) {
	int i = 0;
	while(i < n) {
		puts("I love to have sex");
		i++;
	}
}