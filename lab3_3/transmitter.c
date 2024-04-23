/*Non-Canonical Input Processing*/
#include "mylib.h"
//#include "linklayer.h"

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

int main(int argc, char** argv)
{
    int fd, c, res, ns;
    struct termios oldtio,newtio;
    char buf[255];
    int buf_size;
    char data[255];
    int data_size;
    char data2[255];
    int data2_size;

    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS0", argv[1])!=0) &&
          (strcmp("/dev/ttyS1", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS0\n");
        exit(1);
    }

    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
    */

    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd < 0){
        perror(argv[1]);
        exit(-1); 
    }

    if (tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    /* define new port settings */
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */

    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
    */


   /* flush (discard) data received but not read and written but not transmitted */
    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");




    // ##### TEST ESTABLISHMENT PHASE #####
    /*
    T -----SET-----> R
    T <-----UA------ R
    */
    printf("ESTABLISHMENT\n");
    // construct SET command
    buf_size = construct_control_frame(A1, SET, buf, 255);
    // send control frame
    res = write(fd, buf, buf_size);
    if(res != buf_size){

        printf("Error writing\n");
        return -1;
    }

    // print sent command
    printf("sent: ");
    print_message(buf, buf_size);
    printf("\n");

    // receive UA reply
    buf_size = read_control_frame(fd, buf, 255);
    if(buf_size == -1){

        printf("Error in read_control_frame\n");
        return -1;
    }

    // check if reply is UA from receiver
    if(buf[3] == (A1^UA)){

        printf("Received UA reply from receiver\n");
    }
    else{

        printf("Didn't receive UA reply from receiver\n");
    }

    // print received reply
    printf("received: ");
    print_message(buf, buf_size);
    printf("\n");


    // ##### TEST DATA TRANSFER PHASE #####
    /*
    T -----I0-----> R
    T <----RR1----- R
    */
    printf("DATA TRANSFER\n");

    ns = 0; // sequence number

    // data
    data[0] = FLAG;
    data[1] = 0x0a;
    data[2] = 0x0a;
    data[3] = FLAG;
    data[4] = ESC;
    data[5] = FLAG;

    data_size = 6;

    printf("data before byte stuffing: ");
    print_message(data, data_size);
    printf("\n");

    // byte stuffing
    data2_size = byte_stuffing(data, data_size, data2, 255);
    printf("data after byte stuffing: ");
    print_message(data2, data2_size);
    printf("\n");

    // construct information frame
    buf_size = construct_information_frame(buf, 255, data2, data2_size, ns);

    // send information frame
    res = write(fd, buf, buf_size);
    if(res != buf_size){

        printf("DATA TRANSFER: error sending information frame\n");
        return -1;
    }

    ns = (ns+1)%2; // update sequence number

    // print sent command
    printf("sent: ");
    print_message(buf, buf_size);
    printf("\n");

    // receive RR0/RR1 reply
    buf_size = read_control_frame(fd, buf, 255);
    if(buf_size == -1){

        printf("TRANSFER DATA: error reading RR1 reply\n");
        return -1;
    }

    // check if RR0/RR1 received
    c = (ns == 0 ? RR0 : RR1);
    if(buf[3] == (A1^c)){

        printf("Received RR%d reply\n", ns);
    }
    else{

        printf("error - Didn't receive RR%d reply\n", ns);
        return -1;
    }

    // print received reply
    printf("received: ");
    print_message(buf, buf_size);
    printf("\n");

    // TERMINATION
    /*
    T -----DISC-----> R
    T <----DISC------ R
    T -----UA-------> R
    */
    // send DISC command

    // construct DISC command
    buf_size = construct_control_frame(A1, DISC, buf, 255);

    // write DISC command
    res = write(fd, buf, buf_size);
    if(res != buf_size){

        printf("TERMINATION: error writing DISC command\n");
        return -1;
    }

    // print sent command
    printf("sent: ");
    print_message(buf, buf_size);
    printf("\n");

    // receive DISC command
    buf_size = read_control_frame(fd, buf, 255);
    if(buf_size != 5){

        printf("TERMINATION: error reading DISC command\n");
        return -1;        
    }

    // print received command
    printf("received: ");
    print_message(buf, buf_size);
    printf("\n");

    // send UA reply

    // construct UA reply
    buf_size = construct_control_frame(A2, UA, buf, 255);

    // write UA reply
    res = write(fd, buf, buf_size);
    if(res != buf_size){

        printf("TERMINATION: error sending UA reply\n");
        return -1;
    }

    // print sent reply
    printf("sent: ");
    print_message(buf, buf_size);
    printf("\n");

    // restore old attributes of serial port
    tcsetattr(fd,TCSANOW,&oldtio);
    // close serial port
    close(fd);

    return 0;
}