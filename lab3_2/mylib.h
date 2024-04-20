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
#define RR 0x01
#define REJ 0x05

/*
function:
read a control frame with the format FLAG|A|C|BCC|FLAG
save it in buffer

parameters:
fd - file descriptor
buf - pointer to start of a buffer
buf_size - allocated size of buffer

return:
0 - success
-1 - error
*/
int read_control_frame(const int fd, const char* buf, const int buf_size);

/*
return:
1 - byte == FLAG
0 - byte != FLAG 
*/
int flag_rcv(char byte);

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