#ifndef LINKLAYER
#define LINKLAYER

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef struct linkLayer{
    char serialPort[50];
    int role; //defines the role of the program: 0==Transmitter, 1=Receiver
    int baudRate;
    int numTries;
    int timeOut;
} linkLayer;

//ROLE
#define NOT_DEFINED -1
#define TRANSMITTER 0
#define RECEIVER 1


//SIZE of maximum acceptable payload; maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

//CONNECTION deafault values
#define BAUDRATE_DEFAULT B38400
#define MAX_RETRANSMISSIONS_DEFAULT 3
#define TIMEOUT_DEFAULT 4
#define _POSIX_SOURCE 1 /* POSIX compliant source */

//MISC
#define FALSE 0
#define TRUE 1

// file descriptor
int fd;

// old configuration
struct termios oldtio;
struct termios newtio;


// define bytes for frames
#define FLAG 0x5c
#define A1 0x01
#define A2 0x03
#define SET 0x07
#define DISC 0x0a
#define UA 0x06
#define RR0 0x01
#define RR1 0x11
#define REJ0 0x05
#define REJ1 0x15
#define ESC 0x5d
#define I0 0x80
#define I1 0xc0


// Opens a connection using the "port" parameters defined in struct linkLayer
// returns "-1" on error and "1" on sucess
int llopen(linkLayer connectionParameters);
// Sends data in buf with size bufSize
// returns "number of written characters" on succes or "negative value" on failure/error
int llwrite(unsigned char* buf, int bufSize);
// Receive data in packet
// returns "number of read characters" on success or "negative value" on failure/error
int llread(unsigned char* packet);
// Closes previously opened connection; if showStatistics==TRUE, link layer should print statistics in the console on close
// returns "1" on success, "-1" on failure/error
int llclose(linkLayer connectionParameters, int showStatistics);
// performs byte unstuffing on in vector, saves in out vector
// returns "out_size" on success or "-1" on failure/error
int byte_unstuffing(unsigned char* const out, const int out_max_size, const unsigned char* const in, const int in_size);
// performs byte stuffing on in vector, saves in out vector
// returns "out_size" on succes or "-1" on failure/error
int byte_stuffing(unsigned char* const out, const int out_max_size, const unsigned char* const in, const int in_size);
// reads control frame to buf using a state machine
// returns "5" on success or "-1" on failure/error
int read_control_frame(unsigned char* const buf, const int buf_max_size);
// reads information frame to buf using a state machine
// returns "number of read characters" on success or "-1" on failure/error
int read_information_frame(unsigned char* const buf, const int buf_max_size);
// contructs control frame
// returns "5" on success or "-1" on failure/error
int construct_control_frame(const unsigned char A, const unsigned char C, unsigned char* const buf, const int buf_max_size);
// constructs information frame
// returns "frame length" on success or "-1" on failure/error
int construct_information_frame(unsigned char* const buf, const int buf_max_size, const unsigned char* const data, const int data_size , const int ns);
// print frame in hexadecimal format
// returns "1" on success and "-1" on failure//error
int print_message(const unsigned char* const buf, const int buf_size);
// returns "1" when byte is a FLAG
// return "0" otherwise
int flag_rcv(unsigned char byte);
// returns "1" when byte is an ESC
// return "0" otherwise
int esc_rcv(unsigned char byte);
// returns "1" when byte is an A-type byte
// return "0" otherwise
int A_rcv(unsigned char byte);
// returns "1" when byte is a C-type byte
// return "0" otherwise
int C_rcv(int byte);

#endif
