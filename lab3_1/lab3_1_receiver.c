/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

/*
define bytes for frames
*/
#define FLAG 0x5c
#define A1 0x01
#define A2 0x03
#define SET 0x07
#define DISC 0x0a
#define UA 0x06
#define RR 0x01
#define REJ 0x05

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];

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
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */

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

    // receive SET command
    res = read(fd, buf, 5);
    if(res != 5){

        printf("Error reading\n");
        return -1;
    }

    // check if command is SET from transmitter
    if(buf[3] == A1^SET){

        printf("Received SET command from transmitter\n");
    }
    else{

        printf("Didn't receive SET command from transmitter\n");
    }

    // print received command
    printf("received: ");
    for(int i = 0 ; i < 5 ; i++){

        printf("%x", buf[i]);
    }
    printf("\n");
    

    // construct UA reply
    buf[0] = FLAG;
    buf[1] = A1;
    buf[2] = UA;
    buf[3] = buf[1]^buf[2];
    buf[4] = FLAG;

    // send UA reply
    res = write(fd, buf, 5);
    if(res != 5){

        printf("Error writing\n");
        return -1;
    }

    // print sent reply
    printf("sent: ");
    for(int i = 0 ; i < 5 ; i++){

        printf("%x", buf[i]);
    }
    printf("\n");

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
