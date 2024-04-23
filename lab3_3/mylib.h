#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
define bytes for frames
*/
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

#define SENT 0
#define RECEIVED 1

struct termios oldtio;


/*
function:
perform byte unstuffing on an input vector of chars
(FLAG) <- (ESC,FLAG^0x20)
(ESC) <- (ESC,ESC0x20)
save result in output vector of chars

parameters:
[in] - input vector of chars
in_size - number of chars stored in [in]
[out] - ouput vector of chars
out_size - max capacity of [out]

return:
ret >= 0 - number of chars stored in [out] 
ret == -1 - error
*/
int byte_unstuffing(const char* const in, const int in_size, char* const out, const int out_size);

/*
function:
perform byte stuffing on an input vector of chars
(FLAG) -> (ESC,FLAG^0x20)
(ESC) -> (ESC,ESC0x20)
save result in output vector of chars

parameters:
[in] - input vector of chars
in_size - number of chars stored in [in]
[out] - ouput vector of chars
out_size - max capacity of [out]

return:
ret >= 0 - number of chars stored in [out] 
ret == -1 - error
*/
int byte_stuffing(const char* const in, const int in_size, char* const out, const int out_size);

/*
function:
read an information frame with the format FLAG|A|C|BCC1|D_1|...|D_N|BCC2|FLAG
save it in buffer

parameters:
fd - file descriptor (of serial port)
buf - pointer to start of a buffer
buf_size - allocated size of buffer

return:
ret>=0 - success
-1 - error
*/
int read_information_frame(const int fd, char* const buf, const int buf_size);

/*
function:
read a control frame with the format FLAG|A|C|BCC|FLAG
save it in buffer

parameters:
fd - file descriptor (of serial port)
buf - const pointer to start of a buffer
buf_size - allocated size of buffer

return:
0 - success
-1 - error
*/
int read_control_frame(const int fd, char* const buf, const int buf_size);

/*
function:
construct control frame with the format (FLAG,A,C,BCC,FLAG)
save it in buf

parameters:
A - Address field
C - Control field
[buf] - buffer
buf_size - max capacity of buffer

return:
ret == 5 - success, size of control frame
ret == -1 - error
*/
int construct_control_frame(const char A, const char C, char* const buf, const int buf_size);

/*
function:
construct information frame
save it in buffer

parameters:
[buf] - buffer
buf_size - max capacity of buffer
[data] - data after byte stuffing
data_size - number of elements in [data]
ns - sequence number
*/
int construct_information_frame(char* const buf, const int buf_size, const char* const data, const int data_size , const int ns);

/*
function:
print contents of buf

parameters:
buf - buffer
buf_size - number of elements in buffer

return:
0 - ok
-1 - error
*/
int print_message(const char* const buf, const int buf_size);
/*
return:
1 - byte == FLAG
0 - byte != FLAG 
*/
int flag_rcv(char byte);

/*
return:
1 - byte == ESC
0 - byte != ESC
*/
int esc_rcv(char byte);

/*
return:
1 - byte is A
0 - byte isn't A
*/
int A_rcv(char byte);

/*
return:
1 - byte is C
0 - byte isn't C
*/
int C_rcv(int byte);
