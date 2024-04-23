/*Non-Canonical Input Processing*/
#include "mylib.h"

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

int main(int argc, char** argv)
{
    int fd, c, res, nr;
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
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
    */

    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd < 0) { perror(argv[1]); exit(-1); }

    if (tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 chars received */

    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
    */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");


    // ##### TEST ESTABLISHMENT PHASE ####
    /*
    T -----SET-----> R
    T <-----UA------ R
    */

    // receive SET command
    res = read_control_frame(fd, buf, 5);
    if(res == -1){

        printf("Error in read_control_frame\n");
        return -1;
    }

    // check if command is SET from transmitter
    if(buf[3] == (A1^SET)){

        printf("Received SET command from transmitter\n");
    }
    else{

        printf("Didn't receive SET command from transmitter\n");
    }

    // print received command
    printf("received: ");
    print_message(buf, res);
    printf("\n");
    

    // construct UA reply
    buf_size = construct_control_frame(A1, UA, buf, 255);
    // send UA reply
    res = write(fd, buf, buf_size);
    if(res != buf_size){

        printf("Error writing\n");
        return -1;
    }

    // print sent reply
    printf("sent: ");
    print_message(buf, buf_size);
    printf("\n");

    
    // ##### TEST DATA TRANSFER PHASE
    /*
    T -----I0-----> R
    T <----RR1----- R
    */

    // set sequence number to 0
    nr = 0;

    // read information frame
    buf_size = read_information_frame(fd, buf, 255);
    if(res == -1){

        printf("Error in read_information_frame\n");
        return -1;
    }

    // check information frame

    // check BCC1
    c = (nr == 0 ? I0 : I1);
    if(buf[3] == (A1^c)){
        
        printf("DATA TRANSFER: received I%d\n", nr);
    }
    else{

        printf("DATA TRANSFER: error - didn't receive I%d\n", nr);
        return -1;
    }

    // check BCC2
    res = 0;
    for(int i = 4 ; i < buf_size-2 ; i++){

        if(i == 4){

            res = buf[4];
        }
        else{

            res = (res^buf[i]);
        }
    }
    if(res == buf[buf_size-2]){

        printf("DATA TRANSFER: BCC2 is correct\n");
    }
    else{

        printf("DATA TRANSFER: error - BCC2\n");
        return -1;
    }

   
    // print received frame
    printf("received: ");
    print_message(buf, buf_size);
    printf("\n");

    // extract data
    data_size = byte_unstuffing(buf+4, buf_size-6, data, 255-4);
    // print data
    printf("data: ");
    print_message(data, data_size);
    printf("\n");

    // construct RR reply
    nr = (nr+1)%2; // update sequence number
    c = (nr==0?RR0:RR1); // choose correct RR byte
    buf_size = construct_control_frame(A1, nr==0?RR0:RR1, buf, 255);

    // send RR reply
    res = write(fd, buf, buf_size);
    if(res != buf_size){

        printf("Error writing\n");
        return -1;
    }

    // print sent reply
    printf("sent: ");
    print_message(buf, buf_size);
    printf("\n");

    // ##### TEST TERMINATION PHASE
    /*
    T -----DISC-----> R
    T <----DISC------ R
    T -----UA-------> R
    */
    
    // read DISC command
    buf_size = read(fd, buf, 5);
    if(buf_size != 5){

        printf("TERMINATION: error reading DISC command\n");
        return -1;
    }

    // check DISC command
    if(buf[3] == (A1^DISC)){

        printf("DISC command ok\n");
    }
    else{

        printf("error - didn't receive DISC command\n");
        return -1;
    }

    // print received command
    printf("received: ");
    print_message(buf, buf_size);
    printf("\n");

    // send DISC command
    // construct DISC command -> A = A2 (command from R to T)
    buf_size = construct_control_frame(A2, DISC, buf, 255);
    if(buf_size == -1){

        printf("TERMINATION: error constructing DISC command\n");
        return -1;
    }
    // send DISC command
    res = write(fd, buf, buf_size);
    if(res != buf_size){

        printf("TERMINATION: error sending DISC command\n");
        return -1;
    }

    // print sent command
    printf("sent: ");
    print_message(buf, buf_size);
    printf("\n");

    // read UA reply
    buf_size = read(fd, buf, 5);
    if(buf_size != 5){

        printf("TERMINATION: error reading DISC command\n");
        return -1;
    }

    // check UA reply
    if(buf[3] == (UA^A2)){

        printf("UA command ok\n");
    }
    else{

        printf("error - didn't receive UA command\n");
        return -1;
    }

    // print received command
    printf("received: ");
    print_message(buf, buf_size);
    printf("\n");


    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
