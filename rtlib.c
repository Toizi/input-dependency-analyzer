#include <stdio.h>
#include "rtlib.h"
/*void logop(int i) {
	printf("computed: %i\n", i);
}*/
//long hash =0;
void hashMe(int i) {
	//printf("adding hash %i\n", i);
	hash +=i;
}
//a dummy callsite to compensate the reference to slicing call-site (see my issue openned at dg repository on Github)
/*void dcs(int i){
	printf("%d",i);
}*/
//void dbghashMe(int i, std::string valueName){
//	printf("adding hash %s %i\n",valueName, i);
//        hash +=i;
//} 
void logHash(int hash) {
	//dcs(hash);
	printf("final hash: %d\n", hash);
}
