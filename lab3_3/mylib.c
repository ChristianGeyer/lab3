#include "mylib.h"

int byte_unstuffing(const char* const in, const int in_size, char* const out, const int out_size){

    int i = 0;
    int j = 0;
    while(i < in_size){
        if(i < in_size-1 && esc_rcv(in[i]) && in[i+1] == (FLAG^0x20)){

            out[j] = FLAG;
            i += 2;
        }
        else if(i < in_size-1 && esc_rcv(in[i]) && in[i+1] == (ESC^0x20)){

            out[j] = ESC;
            i += 2;
        }
        else{

            out[j] = in[i];
            i++;
        }
        j++;
        if(j >= out_size){

            printf("overflow\n");
            return -1;
        }
    }

    return j;
}

int byte_stuffing(const char* const in, const int in_size, char* const out, const int out_size){

    int j = 0;
    for(int i = 0 ; i < in_size ; i++){

        if(flag_rcv(in[i]) || esc_rcv(in[i])){

            out[j] = ESC;
            out[j+1] = in[i]^0x20;
            j += 2;
        }
        else{

            out[j] = in[i];
            j++;
        }

        if(j >= out_size){

            printf("overflow\n");
            return -1;
        }
    }

    return j;
}

int read_information_frame(const int fd, char* const buf, const int buf_size){


    /*
    state:
    0 - buf = ()                                    next = (FLAG)
    1 - buf = (FLAG)                                next = (A)
    2 - buf = (FLAG|A)                              next = (C)
    3 - buf = (FLAG|A|C)                            next = (BCC)
    4 - buf = (FLAG|A|C|BCC)                        next = (D_1 ... D_N|BCC (FLAG)    
    5 - buf = (FLAG|A|C|BCC|D_1|...|D_N|BCC2|FLAG)  
    */
    int state = 0;
    int res;
    int i = 0;
    char BCC_aux;
    
    while(state!=5){
    
        printf("state=%d i=%d\n", state, i);
        // read next byte
        res = read(fd, (void*)(buf+state+i), 1);
        if(res == -1){

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
                if(!flag_rcv(buf[state+i])){
                
                  state = 4;
                  i++;  
                }
                else{
                    state = 5;
                }
                break;
        }
    }
    
    
    BCC_aux = buf[4];
    for(int j = 5 ; j < 4+i-1 ; j++){
    
        BCC_aux = BCC_aux^buf[j];
    }
    
    if(BCC_aux!=buf[4+i-1]){
    
        printf("Error in BCC2\n");
        return -1;
    }
    return 5+i;
}

int read_control_frame(const int fd, char* const buf, const int buf_size){

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

        printf("state=%d\n", state);
        // read next byte
        res = read(fd, (void*)(buf+state), 1);
        if(res == -1){

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

    return 5;
}

int construct_control_frame(const char A, const char C, char* const buf, const int buf_size){

    if(buf_size < 5 || buf == NULL) return -1;
    buf[0] = FLAG;
    buf[1] = A;
    buf[2] = C;
    buf[3] = A^C;
    buf[4] = FLAG;
    return 5;
}

int construct_information_frame(char* const buf, const int buf_size, const char* const data, const int data_size , const int ns){

    if(buf == NULL || data == NULL || data_size + 6 > buf_size) return -1;

    int BCC = 0;

    buf[0] = FLAG;
    buf[1] = A1;
    buf[2] = (ns == 0 ? I0 : I1);
    buf[3] = buf[1]^buf[2];
    for(int i = 0 ; i < data_size ; i++){

        buf[4+i] = data[i];
        if(i == 0) BCC = data[i];
        else{

            BCC = BCC^data[i];
        }
    }
    buf[data_size+4] = BCC;
    buf[data_size+5] = FLAG;

    return data_size+6;
}

int print_message(const char* const buf, const int buf_size){

    if(buf == NULL) return -1;
    for(int i = 0 ; i < buf_size ; i++){

        printf("%x", buf[i]);
    }
    return 0;
}

int flag_rcv(char byte){

    return byte == FLAG;
}

int esc_rcv(char byte){

    return byte == ESC;
}

int A_rcv(char byte){

    return byte == A1 || byte == A2;
}

int C_rcv(int byte){

    return byte == SET || byte == DISC || byte == UA || byte == RR0 || byte == RR1 || byte == REJ0 || byte == REJ1 || byte == I0 || byte == I1;
}