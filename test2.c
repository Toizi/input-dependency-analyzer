#include<stdio.h>

int hashp =0;
void addHash(int a){
	hashp+=a;
}

//dummy call site
void dcs(int hash){
	printf("%d", hash);
}
int main(){
	int a = 5;
	addHash(5);
	a++;
	addHash(6);
	dcs(hashp);
	return 0;
}
