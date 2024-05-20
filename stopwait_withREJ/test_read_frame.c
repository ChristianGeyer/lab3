#include "linklayer.h"
#include <string.h>

#define NCONTROL 5

int main(int argc, char** argv){

    /*
    compile program: gcc -o test test_read_frame.c linklayer.c
    run program in receiver: ./test /dev/ttyS10 rx
    run program in transmitter: ./test /dev/ttyS11 tx
    */

    int i;
    int res;
    int buf_size;
    unsigned char buf[NCONTROL];
    int testdata_size = 10;
    unsigned char testdata[testdata_size];
    int buf2_size;
    unsigned char buf2[2*testdata_size];
    int buf3_size;
    unsigned char buf3[2*testdata_size+6];

    unsigned char A;
    unsigned char C;
    
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

    if(ll.role == TRANSMITTER){

        // open serial connection
        fd1 = open(ll.serialPort, O_RDWR | O_NOCTTY);
        if(fd1 < 0){

            printf("error: open\n");
            return -1;
        }

        // set attributes of connection (copied from writenoncanonical.c)

        if ( tcgetattr(fd1,&oldtio1) == -1) { /* save current port settings */
            perror("tcgetattr");
            exit(-1);
        }

        bzero(&newtio1, sizeof(newtio1));
        newtio1.c_cflag = ll.baudRate | CS8 | CLOCAL | CREAD;
        newtio1.c_iflag = IGNPAR;
        newtio1.c_oflag = 0;

        /* set input mode (non-canonical, no echo,...) */
        newtio1.c_lflag = 0;

        newtio1.c_cc[VTIME]    = 0;   /* inter-character timer unused */
        newtio1.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */



        /*
        VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
        leitura do(s) próximo(s) caracter(es)
        */


        tcflush(fd1, TCIOFLUSH);

        if (tcsetattr(fd1,TCSANOW,&newtio1) == -1) {
            perror("tcsetattr");
            exit(-1);
        }

        printf("New termios structure set\n"); 

        // construct control frame
        A = A1;
        C = SET;
        buf_size = construct_control_frame(A, C, buf, NCONTROL);
        if(buf_size == -1){

            printf("error: construct_control_frame\n");
            return -1;
        }

        // print control frame
        printf("control frame:\n");
        printf("buf_size = %d\n", buf_size);
        print_message(buf, buf_size);
        printf("\n");

        // send control frame
        res = write(fd1, buf, buf_size);
        if(res != buf_size){

            printf("error: write\n");
            printf("res = %d , buf_size = %d\n", res, buf_size);
            return -1;
        }

        // gerar dados de teste
        for(i = 0 ; i < testdata_size ; i++){

            testdata[i] = 'a'+i;
        }

        // print dos dados
        printf("testdata:\n");
        printf("testdata_size = %d", testdata_size);
        print_message(testdata, testdata_size);
        printf("\n");

        // stuffing
        buf2_size = byte_stuffing(buf2, 2*testdata_size, testdata, testdata_size);
        if(buf2_size == -1){

            printf("error: byte_stuffing\n");
            return -1;
        }

        // print depois de stuffing
        printf("stuffing:\n");
        printf("buf2_size = %d\n", buf2_size);
        print_message(buf2, buf2_size);
        printf("\n");

        // construct information frame
        buf3_size = construct_information_frame(buf3, 2*testdata_size+6, buf2, buf2_size, 0);
        if(buf3_size == -1){

            printf("error: construct_information_frame\n");
            return -1;
        }

        // write information frame
        res = write(fd1, buf3, buf3_size);
        if(res != buf3_size){

            printf("error: write\n");
            printf("res = %d , buf3_size = %d\n", res, buf3_size);
            return -1;
        }

        // recover old attributes of the connection (copied from writenoncanonical.c)
        if ( tcsetattr(fd1,TCSANOW,&oldtio1) == -1) {
            perror("tcsetattr");
            exit(-1);
        }
        
        // close connection
        close(fd1);

    }
    else if(ll.role == RECEIVER){

        // open serial connection
        fd1 = open(ll.serialPort, O_RDWR | O_NOCTTY);
        if(fd1 < 0){

            printf("error: open\n");
            return -1;
        }

        // set attributes of connection (copied from noncanonical.c)

        if (tcgetattr(fd1,&oldtio1) == -1) { /* save current port settings */
            perror("tcgetattr");
            exit(-1);
        }

        bzero(&newtio1, sizeof(newtio1));
        newtio1.c_cflag = ll.baudRate | CS8 | CLOCAL | CREAD;
        newtio1.c_iflag = IGNPAR;
        newtio1.c_oflag = 0;

        /* set input mode (non-canonical, no echo,...) */
        newtio1.c_lflag = 0;

        newtio1.c_cc[VTIME]    = 0;   /* inter-character timer unused */
        newtio1.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */

        /*
        VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
        leitura do(s) próximo(s) caracter(es)
        */


        tcflush(fd1, TCIOFLUSH);

        if (tcsetattr(fd1,TCSANOW,&newtio1) == -1) {
            perror("tcsetattr");
            exit(-1);
        }

        printf("New termios structure set\n");

        // read control frame
        buf_size = read_control_frame(buf, NCONTROL);
        if(buf_size == -1){

            printf("error: read_control_frame\n");
            return -1;
        }

        // print received frame
        printf("control frame:\n");
        printf("buf_size = %d\n", buf_size);
        print_message(buf, buf_size);
        printf("\n");

        // read information frame
        buf3_size = read_information_frame(buf3, 2*testdata_size+6);
        if(buf3_size == -1){

            printf("error: read_information_frame\n");
            return -1;
        }

        // print information frame
        printf("information frame:\n");
        printf("buf3_size = %d\n", buf3_size);
        print_message(buf3, buf3_size);
        printf("\n");

        // print data before unstuffing
        printf("data before unstuffing:\n");
        printf("size = %d\n", buf3_size-6);
        print_message(buf3+4, buf3_size-6);
        printf("\n");

        // byte unstuffing
        buf2_size = byte_unstuffing(buf2, testdata_size, buf3+4, buf3_size-6);
        if(buf2_size == -1){

            printf("error: byte_unstuffing\n");
            return -1;
        }

        // print data after unstuffing
        printf("data after unstuffing\n");
        printf("buf2_size = %d\n", buf2_size);
        print_message(buf2, buf2_size);
        printf("\n");

        // recover old attributes of the connection (copied from noncanonical.c)
        if ( tcsetattr(fd1,TCSANOW,&oldtio1) == -1) {
            perror("tcsetattr");
            exit(-1);
        }
        
        // close connection
        close(fd1);
    }

    return 0;
}