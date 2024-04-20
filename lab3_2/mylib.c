#include "mylib.h"
int read_control_frame(const int fd, const char* buf, const int buf_size){

    /*
    state:
    0 - buf = ()                    next = (FLAG)
    1 - buf = (FLAG)                next = (A)
    2 - buf = (FLAG|A)              next = (C)
    3 - buf = (FLAG|A|C)            next = (BCC)
    4 - buf = (FLAG|A|C|BCC)        next = (FLAG)
    5 - buf = (FLAG|A|C|BCC|FLAG)   next = () 
    */
    int state = 0;
    int res;

    while(state!=5){

        // read next byte
        res = read(fd, buf+state, 1);
        if(res != -1){

            printf("Error in read_control_frame\n");
            printf("state = %d\n", state);
            return -1;
        }

        switch(state){

            case 0:
                if(flag_rcv(buf[state])){

                    state=1;
                }
                else{

                    state = 0;
                }
                break;
            case 1:
                if(A_rcv(buf[state])){

                    state = 2;
                }
                else if(flag_rcv(buf[state])){

                    state=1;
                }
                else{

                    state = 0;
                }
                break;
            case 2:
                if(C_rcv(buf[state])){

                    state = 3;
                }
                else if(flag_rcv(buf[state])){
                    state = 1;
                }
                else{

                    state = 0;
                }
                break;
            case 3:
                if((buf[1]^buf[2]) == buf[state]){

                    state = 4;
                }
                else if(flag_rcv(buf[state])){

                    state = 1;
                }
                else{

                    state = 0;
                }
                break;
            case 4:
                if(flag_rcv(buf[state])){

                    state = 5;
                }
                else{

                    state = 0;
                }
                break;
        }
    }

    return 0;
}

int flag_rcv(char byte){

    return byte == FLAG;
}

int A_rcv(char byte){

    return byte == A1 || byte == A2;
}

int C_rcv(int byte){

    return byte == SET || byte == DISC || byte == UA || byte == RR || byte == REJ;
}