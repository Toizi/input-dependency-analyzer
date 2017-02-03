#include <stdio.h>
#include <string.h>

void reverse_string(char *str)
{
    /* skip null */
    if (str == 0)
    {
        return;
    }

    /* skip empty string */
    if (*str == 0)
    {
        return;
    }

    /* get range */
    char *start = str;
    char *end = start + strlen(str) - 1; /* -1 for \0 */
    char temp;

    /* reverse */
    while (end > start)
    {
        /* swap */
        temp = *start;
        *start = *end;
        *end = temp;

        /* move */
        ++start;
        --end;
    }
}


int main(void)
{
    char s1[] = "Reverse me!";
    char s2[] = "abc";
    char s3[] = "ab";
    char s4[] = "a";
    char s5[] = "";

    reverse_string(0);

    reverse_string(s1);
    reverse_string(s2);
    reverse_string(s3);
    reverse_string(s4);
    reverse_string(s5);

    printf("%s\n", s1);
    printf("%s\n", s2);
    printf("%s\n", s3);
    printf("%s\n", s4);
    printf("%s\n", s5);

    return 0;
}
