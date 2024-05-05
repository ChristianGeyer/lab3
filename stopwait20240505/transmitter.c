#include "linklayer.h"

/*
typedef struct linkLayer{
    char serialPort[50];
    int role; //defines the role of the program: 0==Transmitter, 1=Receiver
    int baudRate;
    int numTries;
    int timeOut;
} linkLayer;
*/

int main(int argc, unsigned char** argv){

    linkLayer connectionParameters;
    int res;
    unsigned char data[MAX_PAYLOAD_SIZE];

    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS0", argv[1])!=0) &&
          (strcmp("/dev/ttyS1", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS0\n");
        exit(1);
    }

    // define connection parameters
    strcpy(connectionParameters.serialPort, argv[1]);
    connectionParameters.role = TRANSMITTER;
    connectionParameters.baudRate = BAUDRATE_DEFAULT;
    connectionParameters.numTries = MAX_RETRANSMISSIONS_DEFAULT;
    connectionParameters.timeOut = TIMEOUT_DEFAULT; // in seconds
    
    // open connection
    res = llopen(connectionParameters);
    if(res == -1){

        printf("error in llopen\n");
        return -1;
    }

    // construct data to send - "aaaaaaaaaa"
    for(int i = 0; i < 10 ; i++){

        data[i] = 0x0a;
    }
    // write data
    res = llwrite(data, 10);
    if(res == -1){

        printf("error in llwrite\n");
        return -1;
    }   
    // write data again
    res = llwrite(data, 10);
    if(res == -1){

        printf("error in llwrite\n");
        return -1;
    }
    // close connection
    res = llclose(connectionParameters, 0);
    if(res == -1){

        printf("error in llclose\n");
        return -1;
    }

    return 0;
}
