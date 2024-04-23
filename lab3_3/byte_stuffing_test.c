#include "mylib.h"

int main(){

    char buf1[255];
    char buf2[255];
    int buf1_size, buf2_size;

    buf1[0] = FLAG;
    buf1[1] = ESC;
    buf1[2] = 0x0a;
    buf1[3] = 0x0a;
    buf1[4] = FLAG;

    buf1_size = 5;

    printf("before byte stuffing: ");
    for(int i = 0 ; i < buf1_size ; i++){

        printf("%x", buf1[i]);
    }
    printf("\n");

    buf2_size = byte_stuffing(buf1, buf1_size, buf2, 255);
    printf("after byte stuffing: ");
    for(int i = 0 ; i < buf2_size ; i++){

        printf("%x", buf2[i]);
    }
    printf("\n");

    buf1_size = byte_unstuffing(buf2, buf2_size, buf1, 255);

    printf("after byte unstuffing: ");
    for(int i = 0 ; i < buf1_size ; i++){

        printf("%x", buf1[i]);
    }
    printf("\n");
    return 0;
}