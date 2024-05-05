#include "linklayer.h"

int llopen(linkLayer connectionParameters){

    int res;
    unsigned char buf[255];

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY );
    if (fd < 0){
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    if (tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 1;

    newtio.c_cc[VTIME] = 0; // dont use timer between characters
    newtio.c_cc[VMIN] = 1; // return read as soon as there is 1 byte

    /*
    ESTABLISHMENT PHASE:
    
    T -----SET-----> R
    T <-----UA------ R
    */
    if(connectionParameters.role == TRANSMITTER){

        // construct SET command
        res = construct_control_frame(A1, SET, buf, 255);
        if(res != 5){

            return -1; // error
        }

        // send SET command
        res = write(fd, buf, 5);
        if(res != 5){

            return -1; // error
        }
        printf("sent: ");
        print_message(buf, 5); //print sent message
        printf("\n");

        // read UA reply
        res = read_control_frame(buf, 255);
        if(res != 5){

            return -1; // error
        }
        printf("received: ");
        print_message(buf, 5);   
        printf("\n");

        // check if reply is UA from receiver
        if(buf[3] != (A1^UA)){

            return -1; // error - did not receive UA reply
        }
    }

    else if(connectionParameters.role == RECEIVER){

        // receive SET command
        res = read_control_frame(buf, 255);
        if(res != 5){

            return -1; // error
        }
        printf("received: ");
        print_message(buf, 5); //received
        printf("\n");

        // check if command is SET from transmitter
        if(buf[3] != (A1^SET)){

            return -1; // error - did not receive SET command
        }

        // construct UA reply
        res = construct_control_frame(A1, UA, buf, 255);
        if(res != 5){

            return -1; // error
        }

        // send UA reply
        res = write(fd, buf, 5);
        if(res != 5){

            printf("Error writing\n");
            return -1;
        }
        printf("sent: ");
        print_message(buf, 5);
        printf("\n");
    }

    return 1;
}

int llwrite(unsigned char* buf, int bufSize){

    if(fd < 0 || buf == NULL || bufSize > MAX_PAYLOAD_SIZE) return -1; // error - parameters

    static int ns = 0; // sequence number
    unsigned char data[2*MAX_PAYLOAD_SIZE];
    int data_size;
    unsigned char frame[2*MAX_PAYLOAD_SIZE+6];
    int frame_size;
    int ret;
    unsigned char c;

    // byte stuffing
    data_size = byte_stuffing(data, 2*MAX_PAYLOAD_SIZE, buf, bufSize);
    if(data_size == -1){

        return -1; // error
    }

    // construct information frame
    frame_size = construct_information_frame(frame, 2*MAX_PAYLOAD_SIZE+6, data, data_size, ns);
    ns = (ns+1)%2; // update sequence number
    if(frame_size == -1){

        return -1; // error
    }

    // send information frame
    ret = write(fd, frame, frame_size);
    if(ret != frame_size){

        return -1; // error
    }
    printf("sent: ");
    print_message(frame, frame_size);
    printf("\n");

    // receive RR0/RR1 reply
    frame_size = read_control_frame(frame, 2*MAX_PAYLOAD_SIZE+6);
    if(frame_size != 5){

        return -1; // error
    }
    printf("received: ");
    print_message(frame, frame_size);
    printf("\n");

    // check if RR0/RR1 received
    c = RR0;
    if(ns) c = RR1;
    if(frame[3] != (A1^c)){

        return -1; // error
    }

    return ret;
}

int llread(unsigned char* packet){

    if(fd < 0 || packet == NULL) return -1; // error - parameters

    static int ns = 0; // sequence number
    unsigned char frame[2*MAX_PAYLOAD_SIZE+6];
    int frame_size;
    int ret;
    unsigned char c;
    int i;

    // read information frame
    frame_size = read_information_frame(frame, 2*MAX_PAYLOAD_SIZE+6);
    if(frame_size == -1){

        return -1; // error
    }
    printf("received: ");
    print_message(frame, frame_size);
    printf("\n");

    // check BCC1
    c = I0;
    if(ns) c = I1;
    ns = (ns+1)%2; // update sequence number
    if(frame[3] != (A1^c)){

        return -1; // error - BCC1
    }

    // check BCC2
    for(i = 4 ; i < frame_size-2 ; i++){

        if(i == 4) c = frame[i];
        else{

            c = (c^frame[i]);
        }
    }
    if(frame[frame_size-2] != c){

        return -1; // error - BCC2
    }

    // byte unstuffing
    ret = byte_unstuffing(packet, MAX_PAYLOAD_SIZE, frame+4, frame_size-6);
    if(ret == -1){

        return -1; // error - byte unstuffing
    }

    // construct RR0/RR1 reply
    c = RR0;
    if(ns) c = RR1;
    frame_size = construct_control_frame(A1, c, frame, 2*MAX_PAYLOAD_SIZE+6);
    if(frame_size!=5){

        return -1; // error - construct control frame
    }

    // send RR0/RR1 reply
    frame_size = write(fd, frame, 5);
    if(frame_size != 5){

        return -1; // error - write
    }
    printf("sent: ");
    print_message(frame, 5);
    printf("\n");

    return ret; // number of bytes in read into packet
}

int llclose(linkLayer connectionParameters, int showStatistics){

    if(fd < 0) return -1;

    unsigned char frame[255];
    int frame_size;
    /*
    TERMINATION:
    T ------DISC-------> R
    T <-----DISC-------- R
    T ------UA---------> R
    */
    if(connectionParameters.role == TRANSMITTER){

        // construct DISC command
        frame_size = construct_control_frame(A1, DISC, frame, 255);
        if(frame_size != 5){

            return -1; // error - construct control frame
        }

        // send DISC command
        frame_size = write(fd, frame, 5);
        if(frame_size != 5){

            return -1; // error - write
        }
        printf("sent: ");
        print_message(frame, 5);
        printf("\n");

        // receive DISC command from receiver
        frame_size = read_control_frame(frame, 255);
        if(frame_size != 5){

            return -1; // error - read control frame
        }
        printf("received: ");
        print_message(frame, 5);
        printf("\n");
        // check if DISC command from receiver arrived
        if(frame[3] != (A2^DISC)){

            return -1; // error - did not receive DISC command from receiver
        }

        // construct UA reply
        frame_size = construct_control_frame(A2, UA, frame, 255);
        if(frame_size != 5){

            return -1; // error - construct control frame
        }
        // send UA reply
        frame_size = write(fd, frame, 5);
        if(frame_size != 5){

            return -1; // error - write
        }
        printf("sent: ");
        print_message(frame, 5);
        printf("\n");
    }
    else if(connectionParameters.role == RECEIVER){

        // read DISC command
        frame_size = read_control_frame(frame, 255);
        if(frame_size != 5){

            return -1; // error - read control frame
        }
        printf("received: ");
        print_message(frame, 5);
        printf("\n");
        // check DISC command
        if(frame[3] != (A1^DISC)){

            return -1; // error - did not receive DISC command from transmitter
        }

        // construct DISC command from receiver
        frame_size = construct_control_frame(A2, DISC, frame, 255);
        if(frame_size != 5){

            return -1; // error - construct control frame
        }
        // send DISC command
        frame_size = write(fd, frame, 5);
        if(frame_size != 5){

            return -1; // error - write
        }
        printf("sent: ");
        print_message(frame, 5);
        printf("\n");

        // receive UA reply from transmitter
        frame_size = read_control_frame(frame, 255);
        if(frame_size != 5){

            return -1; // error - read control frame
        }
        printf("received: ");
        print_message(frame, 5);
        printf("\n");
        // check UA reply from transmitter
        if(frame[3] != (A2^UA)){

            return -1; // error - did not receive UA reply from transmitter
        }
    }

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 1;
}

int byte_unstuffing(unsigned char* const out, const int out_max_size, const unsigned char* const in, const int in_size){

    if(out == NULL || in == NULL) return -1;
    int i = 0;
    int j = 0;
    while(i < in_size){

        if(j >= out_max_size){

            return -1; // overflow
        }

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
    }

    return j;
}

int byte_stuffing(unsigned char* const out, const int out_max_size, const unsigned char* const in, const int in_size){

    if(in == NULL || out == NULL) return -1;

    int i, j;
    j = 0;
    for(i = 0 ; i < in_size ; i++){

        if(flag_rcv(in[i]) || esc_rcv(in[i])){

            if(j+1 >= out_max_size) return -1; // overflow
            out[j] = ESC;
            out[j+1] = in[i]^0x20;
            j += 2;
        }
        else{

            if(j >= out_max_size) return -1; // overflow
            out[j] = in[i];
            j++;
        }
    }

    return j;
}

int read_control_frame(unsigned char* const buf, const int buf_max_size){

    if(fd < 0) return -1; // invalid file descriptor
    /*
    state:
    0 - buf = ()                    next = (FLAG)
    1 - buf = (FLAG)                next = (A)
    2 - buf = (FLAG,A)              next = (C)
    3 - buf = (FLAG,A,C)            next = (BCC)
    4 - buf = (FLAG,A,C,BCC)        next = (FLAG)
    5 - buf = (FLAG,A,C,BCC,FLAG)   next = () 
    */
    int state = 0;
    int res;

    while(state!=5){

        printf("state=%d\n", state);
            
        // read next byte
        res = read(fd, (void*)(buf+state), 1);
        if(res != 1){

            return -1; // error
        }

        switch(state){

            case 0:
                if(flag_rcv(buf[state])){

                    state = 1;
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

                    state = 1;
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
    printf("state=%d\n", state);

    return 5;
}

int read_information_frame(unsigned char* const buf, const int buf_max_size){

    if(fd < 0) return -1; // invalid fd
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
    int i = 0;
    int res;
    unsigned char BCC_aux;
    
    while(state!=5){
    
        printf("state=%d i=%d\n", state, i);
        
        // read next byte
        res = read(fd, (void*)(buf+state+i), 1);
        if(res != 1){

            return -1; // erro
        }
        
        switch(state){
        
            case 0:
                if(flag_rcv(buf[state])){

                    state = 1;
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

                    state = 1;
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

int construct_control_frame(const unsigned char A, const unsigned char C, unsigned char* const buf, const int buf_max_size){

    if(buf_max_size < 5 || buf == NULL || !rsc_A(A) || !rsc_C(C)) return -1;
    buf[0] = FLAG;
    buf[1] = A;
    buf[2] = C;
    buf[3] = A^C;
    buf[4] = FLAG;
    return 5;
}

int construct_information_frame(unsigned char* const buf, const int buf_max_size, const unsigned char* const data, const int data_size , const int ns){

    if(buf == NULL || data == NULL || data_size + 6 > buf_max_size || ns>1 || ns<0) return -1; // error

    int BCC = 0;

    buf[0] = FLAG;
    buf[1] = A1;
    buf[2] = I0;
    if(ns) buf[2] = I1;
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

int print_message(const unsigned char* const buf, const int buf_size){

    if(buf == NULL) return -1;
    for(int i = 0 ; i < buf_size ; i++){

        printf("0x%02x ", buf[i]);
    }
    return 1;
}

int flag_rcv(unsigned char byte){

    return byte == FLAG;
}

int esc_rcv(unsigned char byte){

    return byte == ESC;
}

int A_rcv(unsigned char byte){

    return byte == A1 || byte == A2;
}

int C_rcv(int byte){

    return byte == SET || byte == DISC || byte == UA || byte == RR0 || byte == RR1 || byte == REJ0 || byte == REJ1 || byte == I0 || byte == I1;
}