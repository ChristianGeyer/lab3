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
    unsigned char data[MAX_PAYLOAD_SIZE];
    int res;

    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS0", argv[1])!=0) &&
          (strcmp("/dev/ttyS1", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS0\n");
        exit(1);
    }

    // define connection parameters
    strcpy(connectionParameters.serialPort, argv[1]);
    connectionParameters.role = RECEIVER;
    connectionParameters.baudRate = BAUDRATE_DEFAULT;
    connectionParameters.numTries = MAX_RETRANSMISSIONS_DEFAULT;
    connectionParameters.timeOut = TIMEOUT_DEFAULT; // in seconds
    
    // open connection
    res = llopen(connectionParameters);
    if(res == -1){

        printf("error in llopen\n");
        return -1;
    }

    // read data
    res = llread(data);
    if(res == -1){

        printf("error in llwrite\n");
        return -1;
    }  
    printf("data: ");
    print_message(data, res);
    printf("\n"); 
    // read data again
    res = llread(data);
    if(res == -1){

        printf("error in llwrite\n");
        return -1;
    }
    printf("data: ");
    print_message(data, res);
    printf("\n");
    // close connection
    res = llclose(connectionParameters, 0);
    if(res == -1){

        printf("error in llclose\n");
        return -1;
    }

    return 0;
}
