#include "linklayer.h"
#include <string.h>

#define NCONTROL 5

int main(int argc, char** argv){

    /*
    compile program: gcc -o test test_llopen_llclose.c linklayer.c
    run program in receiver: ./test /dev/ttyS11 rx
    run program in transmitter: ./test /dev/ttyS10 tx
    */
    
    int i;
    int data_size = 20;
    unsigned char data[data_size];

    if(argc != 3) printf("run program: ./test /dev/ttySxx tx|rx\n");
    
    // linklayer parameters
    struct linkLayer ll;
    sprintf(ll.serialPort, "%s", argv[1]);
    ll.role = -1;
    if(strcmp(argv[2], "tx")==0) ll.role = TRANSMITTER;
    else if(strcmp(argv[2], "rx")==0) ll.role = RECEIVER;
    if(ll.role == -1){

        printf("run program: ./test /dev/ttySxx tx|rx\n");
        return -1;
    }
    ll.baudRate = 9600;
    ll.numTries = 3;
    ll.timeOut = 3;

    if(llopen(ll) == -1){

        printf("error: llopen\n");
        return -1;
    }

    if(ll.role == TRANSMITTER){

        // gerar dados
        for(i = 0 ; i < data_size ; i++){

            data[i] = 'a';
        }
        data[0] = ESC;

        llwrite(data, data_size);
        
        data[0] = FLAG;

        llwrite(data, data_size);
    }
    else if(ll.role == RECEIVER){

        llread(data);
        llread(data);
    }

    printf("connection opened\n");

    if(llclose(ll, 0) == -1){

        printf("error: llclose\n");
        return -1;
    }

    printf("connection closed\n");

    return 0;
}