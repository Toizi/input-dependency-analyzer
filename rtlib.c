#include <stdio.h>
#include "rtlib.h"
#include "inputchallenge.h"
//#include "hashcomputation.h"


void logHash(int hash) {
	//dcs(hash);
	printf("computed proof: %d\n", hash);
}
void hashMe(int i) {
        hash +=i;
}

