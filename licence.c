#include<stdio.h>
#include <time.h>
#include "licence.h"
int licenceCheck(){
	struct tm expiryTime ={0};
	int year = 2017-1900, month = 2-1, day = 3, hour = 15, min = 0, sec = 0;
	time_t now;
	time(&now);
	expiryTime.tm_year = year;
	expiryTime.tm_mon= month;
	expiryTime.tm_mday= day;
	expiryTime.tm_hour=hour;
	expiryTime.tm_min=min;
	expiryTime.tm_sec=sec;
	time_t time_exp = mktime(&expiryTime);
	double daydiff;
	daydiff = difftime(now,time_exp);
	daydiff /= 86400;
	return daydiff <= 0.0f;
}
int main()
{
	if (licenceCheck()) {
		printf("Wellcome,\nEnjoy using the trial version of this useful application!\n");
	} else {
		printf("Your trial version has expired, please purchase a licence!\n");
	}
	return 0;
}

