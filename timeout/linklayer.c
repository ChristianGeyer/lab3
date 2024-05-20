#include "linklayer.h"

void timeout_retransmission(){

    if(fd1 < 0){

        printf("unexpected call of timeout_retransmissions\n");
        return;
    }

    if(flag == 0) return; // switch off retransmission with flag

    // write contents of globalbuf to serial connection
    int res = write(fd1, globalbuf, globalbuf_size);
    if(res != globalbuf_size){

        printf("error: timeout_retransmission, write\n");
        exit(-1);
    }

    globalcount++; // retransmission count
    if(globalcount >= max_retransmissions){

        printf("error: max_retransmissions\n");
        exit(-1);
    }
    alarm(timeout); // set new alarm
}

int llopen(linkLayer connectionParameters){

    int i;
    int res;
    int retransmissions;
    unsigned char buf[255];

    // open serial connection
    fd1 = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if(fd1 < 0){

        printf("error: open\n");
        return -1;
    }

    // set attributes of connection (copied from noncanonical.c)

    if ( tcgetattr(fd1,&oldtio1) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio1, sizeof(newtio1));
    newtio1.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
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

    max_retransmissions = connectionParameters.numTries;
    timeout = connectionParameters.timeOut;

    printf("New termios structure set\n");

    /*
    ESTABLISHMENT PHASE:
    
    T -----SET-----> R
    T <-----UA------ R
    */
    if(connectionParameters.role == TRANSMITTER){

        // construct SET command
        res = construct_control_frame(A1, SET, buf, 255);
        if(res != 5){

            printf("error: construct_control_frame\n");
            return -1; // error
        }

        // print SET command
        printf("send (SET command):\n");
        printf("buf_size = %d\n", res);
        print_message(buf, res);
        printf("\n");

        // send SET command
        res = write(fd1, buf, 5);
        if(res != 5){

            printf("error: write\n");
            return -1; // error
        }
        
        // copy contents of buf to globalbuf
        for(i = 0 ; i < res ; i++){

            globalbuf[i] = buf[i];
        }
        globalbuf_size = res;
        globalcount = 0; // start counting retransmissions on timeout
        flag = 1; // switch on retransmissions
        alarm(timeout);  // set alarm
        // read UA reply
        res = read_control_frame(buf, 255);
        flag = 0; // switch off retransmissions
        globalcount = 0;
        globalbuf_size = 0;
        if(res != 5){ // check control frame

            printf("error: read_control_frame");
            return -1; // error
        }

        // print UA reply
        printf("received (UA reply): ");
        printf("buf_size = %d\n", res);
        print_message(buf, res);   
        printf("\n");

        // check if reply is UA from receiver
        if(buf[3] != (A1^UA)){

            printf("error: not UA reply\n");
            return -1; // error - did not receive UA reply
        }
    }

    else if(connectionParameters.role == RECEIVER){

        // receive SET command
        res = read_control_frame(buf, 255);
        if(res != 5){

            printf("error: read_control_frame");
            return -1; // error
        }

        // print SET command
        printf("received (SET command):\n");
        printf("buf_size = %d\n", res);
        print_message(buf, res); //received
        printf("\n");

        // check if SET command from transmitter
        if(buf[3] != (A1^SET)){

            printf("error: not SET command\n");
            return -1; // error - did not receive SET command
        }

        // construct UA reply
        res = construct_control_frame(A1, UA, buf, 255);
        if(res != 5){

            printf("error: contruct_control_frame\n");
            return -1; // error
        }

        // print UA reply
        printf("send (UA reply):\n");
        printf("buf_size = %d\n", res);
        print_message(buf, res);
        printf("\n");

        // send UA reply
        res = write(fd1, buf, 5);
        if(res != 5){

            printf("error: write\n");
            return -1;
        }
    }

    return 1;
}

int llwrite(unsigned char* buf, int bufSize){

    int i;

    if(fd1 < 0 || buf == NULL || bufSize > MAX_PAYLOAD_SIZE) return -1; // error - parameters

    static int ns = 0; // sequence number
    unsigned char data[2*MAX_PAYLOAD_SIZE];
    int data_size;
    unsigned char frame[2*MAX_PAYLOAD_SIZE+6];
    int frame_size;
    int ret;
    unsigned char c, c2;
    int retransmissions = 0;

    // print data
    printf("data:\n");
    printf("bufSize = %d\n", bufSize);
    if(bufSize > 10){

        print_message(buf, 5);
        printf("...");
        print_message(buf+bufSize-5, 5);
    }
    else print_message(buf, bufSize);
    printf("\n");

    // byte stuffing
    data_size = byte_stuffing(data, 2*MAX_PAYLOAD_SIZE, buf, bufSize);
    if(data_size == -1){

        printf("error: byte_stuffing\n");
        return -1; // error
    }

    // print data after byte stuffing
    printf("data after byte stuffing:\n");
    printf("data_size = %d\n", data_size);
    if(data_size > 10){

        print_message(data, 5);
        printf("...");
        print_message(data+data_size-5, 5);
    }
    else print_message(data, data_size);
    printf("\n");

    /*
    DATA TRANSFER PHASE:
    T -----I0/I1-----> R
    T <----RR1/RR0---- R
    or (if errors in data)
    T ------I0/I1-----> R
    T <-----REJ1/REJ0-- R
    T ------I0/I1-----> R
    T <-----REJ1/REJ0-- R
    ...
    T ------I0/I1-----> R
    T <-----RR1/RR0---- R
    */

    globalcount = 0;
    while(globalcount < max_retransmissions){

        printf("retransmissions = (%d/%d)\n", globalcount, max_retransmissions);

        // construct information frame
        frame_size = construct_information_frame(frame, 2*MAX_PAYLOAD_SIZE+6, data, data_size, ns);
        ns = (ns+1)%2; // update sequence number
        if(frame_size == -1){

            printf("error: construct_information_frame\n");
            return -1; // error
        }

        // print information frame
        printf("send (information frame):\n");
        printf("frame_size = %d\n", frame_size);
        if(frame_size > 10){

            print_message(frame, 5);
            printf("...");
            print_message(frame+frame_size-5, 5);
        }
        else print_message(frame, frame_size);
        printf("\n");

        // send information frame
        ret = write(fd1, frame, frame_size);
        if(ret != frame_size){

            printf("error: write\n");
            return -1; // error
        }

        // copy information frame contents to globalbuf
        for(i = 0 ; i < frame_size ; i++){

            globalbuf[i] = frame[i]; 
        }
        globalbuf_size = frame_size;
        flag = 1; // switch on retransmissions on timeout
        alarm(timeout); // set alarm
        // receive RR0/RR1 or REJ0/REJ1 reply
        frame_size = read_control_frame(frame, 2*MAX_PAYLOAD_SIZE+6);
        flag = 0; // switch off retransmissions on timeout
        if(frame_size != 5){

            printf("error: read_control_frame\n");
            return -1; // error
        }

        c = RR0;
        if(ns) c = RR1;
        c2 = REJ0;
        if(ns) c2 = REJ1;
        if(frame[3] == (A1^c2)){ // data arrived with errors (REJ)

            // print REJ reply
            printf("received (REJ%d reply):\n", ns);
            printf("frame_size = %d\n", frame_size);
            print_message(frame, frame_size);
            printf("\n");
            ns = (ns+1)%2; // update sequence number
        }
        else if(frame[3] == (A1^c)){ // data arrived correctly (RR)

            // print RR reply
            printf("received (RR%d reply):\n", ns);
            printf("frame_size = %d\n", frame_size);
            print_message(frame, frame_size);
            printf("\n");
            break; // stop retransmissions
        }
        else{ // wrong BCC

            printf("error: BCC\n");
            return -1;
        }

        globalcount++;
    }

    return ret; // number of sent bytes in the frame (bigger than original size of data)
}

int llread(unsigned char* packet){

    if(fd1 < 0 || packet == NULL) return -1; // error - parameters

    static int ns = 0; // sequence number
    unsigned char frame[2*MAX_PAYLOAD_SIZE+6];
    int frame_size;
    int ret;
    unsigned char c, c2;
    int i;
    int retransmissions = 0;

    /*
    DATA TRANFER PHASE:
    T ------I0/I1-----> R
    T <-----RR1/RR0---- R
    or (if errors in data)
    T ------I0/I1-----> R
    T <-----REJ1/REJ0-- R
    T ------I0/I1-----> R
    T <-----REJ1/REJ0-- R
    ...
    T ------I0/I1-----> R
    T <-----RR1/RR0---- R
    */

    while(retransmissions < max_retransmissions){

        printf("retransmissions = (%d/%d)\n", retransmissions, max_retransmissions);
        // read information frame
        frame_size = read_information_frame(frame, 2*MAX_PAYLOAD_SIZE+6);
        if(frame_size == -1){

            printf("error: read_information_frame\n");
            return -1; // error
        }

        // print information frame
        printf("received (information frame):\n");
        printf("frame_size = %d\n", frame_size);
        if(frame_size > 10){

            print_message(frame, 5);
            printf("...");
            print_message(frame+frame_size-5, 5);
        }
        else print_message(frame, frame_size);
        printf("\n");

        // check BCC1
        c = I0;
        c2 = I1;
        if(ns){

            c = I1;
            c2 = I0;
        } 
        ns = (ns+1)%2; // update sequence number
        if(frame[3] == (A1^c2)){ // duplicate, discard data

            printf("received duplicate\n");
            continue;
        }
        else if(frame[3] != (A1^c)){ // error in BCC1

            printf("error: BCC1\n");
            return -1;
        }

        // new data arrived, frame[3] == (A1^c)

        // check BCC2
        for(i = 4 ; i < frame_size-2 ; i++){

            if(i == 4) c = frame[i];
            else{

                c = (c^frame[i]);
            }
        }
        if(frame[frame_size-2] != c){ // error in BCC2

            // construct REJ reply
            c = REJ0;
            if(ns) c = REJ1;
            ns = (ns+1)%2; // update sequence number
            frame_size = construct_control_frame(A1, c, frame, 2*MAX_PAYLOAD_SIZE+6);
            if(frame_size != 5){

                printf("error: construct_control_frame\n");
                return -1;
            }

            // print REJ reply
            printf("send (REJ reply):\n");
            printf("frame_size = %d\n", frame_size);
            print_message(frame, frame_size);
            printf("\n");

            // send REJ reply
            frame_size = write(fd1, frame, 5);
            if(frame_size != 5){

                printf("error: write\n");
                return -1;
            }
        }
        else{ // BCC2 ok

            break;
        }
        
        retransmissions++;
    }

    // check if data is correct
    if(retransmissions == max_retransmissions){ // errors in data

        printf("error: max_retransmissions\n");
        return -1;
    }

    // byte unstuffing
    ret = byte_unstuffing(packet, MAX_PAYLOAD_SIZE, frame+4, frame_size-6); // save original data in packet
    if(ret == -1){

        printf("error: byte_unstuffing\n");
        return -1; // error - byte unstuffing
    }

    // print data after unstuffing
    printf("received (data after unstuffing):\n");
    printf("data_size = %d\n", ret);
    if(ret > 10){

        print_message(packet, 5);
        printf("...");
        print_message(packet+ret-5, 5);
    }
    else print_message(packet, ret);
    printf("\n");

    // construct RR0/RR1 reply
    c = RR0;
    if(ns) c = RR1;
    frame_size = construct_control_frame(A1, c, frame, 2*MAX_PAYLOAD_SIZE+6);
    if(frame_size!=5){

        printf("error: construct_control_frame\n");
        return -1; // error - construct control frame
    }

    // print RR0/RR1 reply
    printf("send (RR%d reply):\n", ns);
    printf("frame_size = %d\n", frame_size);
    print_message(frame, frame_size);
    printf("\n");

    // send RR0/RR1 reply
    frame_size = write(fd1, frame, 5);
    if(frame_size != 5){

        printf("error: write\n");
        return -1; // error - write
    }

    return ret; // number of bytes read into packet (original data size)
}

int llclose(linkLayer connectionParameters, int showStatistics){

    int i;

    if(fd1 < 0){

        printf("error: fd1\n");
        return -1;
    }

    int frame_size;
    unsigned char frame[255];
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

            printf("error: construct_control_frame\n");
            return -1; // error - construct control frame
        }

        // print DISC command
        printf("send (DISC command):\n");
        printf("frame_size = %d\n", frame_size);
        print_message(frame, frame_size);
        printf("\n");

        // send DISC command
        frame_size = write(fd1, frame, 5);
        if(frame_size != 5){

            printf("error: write\n");
            return -1; // error - write
        }

        // copy DISC command contents to globalbuf
        for(i = 0 ; i < frame_size ; i++){

            globalbuf[i] = frame[i];
        }
        globalbuf_size = frame_size;
        globalcount = 0; // start counting retransmissions
        flag = 1; // switch on retransmissions on timeout
        alarm(timeout);
        // receive DISC command from receiver
        frame_size = read_control_frame(frame, 255);
        flag = 0; // switch off retransmissions on timeout
        if(frame_size != 5){

            printf("error: read_control_frame\n");
            return -1; // error - read control frame
        }

        // print DISC command
        printf("received (DISC command):\n");
        printf("frame_size = %d\n", frame_size);
        print_message(frame, frame_size);
        printf("\n");

        // check if DISC command from receiver arrived
        if(frame[3] != (A2^DISC)){ // check BCC

            printf("error: not DISC command\n");
            return -1; // error - did not receive DISC command from receiver
        }

        // construct UA reply
        frame_size = construct_control_frame(A2, UA, frame, 255);
        if(frame_size != 5){

            printf("error: contruct_control_frame");
            return -1; // error - construct control frame
        }

        // print UA reply
        printf("send (UA reply):\n");
        printf("frame_size = %d\n", frame_size);
        print_message(frame, frame_size);
        printf("\n");

        // send UA reply
        frame_size = write(fd1, frame, 5);
        if(frame_size != 5){

            printf("error: write\n");
            return -1; // error - write
        }
    }
    else if(connectionParameters.role == RECEIVER){

        // read DISC command
        frame_size = read_control_frame(frame, 255);
        if(frame_size != 5){

            printf("error: read_control_frame\n");
            return -1; // error - read control frame
        }

        // print DISC command
        printf("received (DISC command):\n");
        printf("frame_size = %d\n", frame_size);
        print_message(frame, frame_size);
        printf("\n");

        // check DISC command
        if(frame[3] != (A1^DISC)){

            printf("error: not DISC command\n");
            return -1; // error - did not receive DISC command from transmitter
        }

        // construct DISC command from receiver
        frame_size = construct_control_frame(A2, DISC, frame, 255);
        if(frame_size != 5){

            printf("error: construct_control_frame\n");
            return -1; // error - construct control frame
        }

        // print DISC command
        printf("send (DISC command):\n");
        printf("frame_size = %d\n", frame_size);
        print_message(frame, frame_size);
        printf("\n");

        // send DISC command
        frame_size = write(fd1, frame, 5);
        if(frame_size != 5){

            printf("error: write");
            return -1; // error - write
        }

        // copy contents of DISC command to globalbuf
        for(i = 0 ; i < frame_size ; i++){

            globalbuf[i] = frame[i];
        }
        globalbuf_size = frame_size;
        globalcount = 0; // start counting retransmissions
        flag = 1; // switch on retransmissions on timeout
        alarm(timeout);

        // receive UA reply from transmitter
        frame_size = read_control_frame(frame, 255);
        flag = 0; // switch off retransmissions on timeout
        if(frame_size != 5){

            printf("error: read_control_frame\n");
            return -1; // error - read control frame
        }

        // print UA reply
        printf("received (UA reply):\n");
        printf("frame_size = %d\n", frame_size);
        print_message(frame, frame_size);
        printf("\n");

        // check UA reply from transmitter
        if(frame[3] != (A2^UA)){ // check BCC

            printf("error: not UA reply");
            return -1; // error - did not receive UA reply from transmitter
        }
    }

    // recover old attributes of the connection (copied from noncanonical.c)
    if ( tcsetattr(fd1,TCSANOW,&oldtio1) == -1) {
        perror("tcsetattr");
        exit(-1);
    }
    
    // close connection
    close(fd1);

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
            out[j+1] = (in[i]^0x20);
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

    if(fd1 < 0) return -1; // invalid file descriptor
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
        res = read(fd1, (void*)(buf+state), 1);
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

    if(fd1 < 0) return -1; // invalid fd1
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
        res = read(fd1, (void*)(buf+state+i), 1);
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
                    i++;
                }
                break;
        }
    }
    
    // i has the number of bytes read after the first 4 bytes (including BCC and FLAG)
    // total number of bytes read: 4+i
    
    BCC_aux = buf[4];
    for(int j = 5 ; j < 4+i-2 ; j++){
    
        BCC_aux = (BCC_aux^buf[j]);
    }
    
    if(BCC_aux!=buf[4+i-2]){ // check BCC2
    
        printf("Error in BCC2\n");
        return -1;
    }
    return 4+i;
}

int construct_control_frame(const unsigned char A, const unsigned char C, unsigned char* const buf, const int buf_max_size){

    if(buf_max_size < 5 || buf == NULL || !A_rcv(A) || !C_rcv(C)) return -1;
    buf[0] = FLAG;
    buf[1] = A;
    buf[2] = C;
    buf[3] = (A^C);
    buf[4] = FLAG;
    return 5;
}

int construct_information_frame(unsigned char* const buf, const int buf_max_size, const unsigned char* const data, const int data_size , const int ns){

    if(buf == NULL || data == NULL || data_size + 6 > buf_max_size || ns>1 || ns<0) return -1; // error

    unsigned char BCC = 0;

    buf[0] = FLAG;
    buf[1] = A1;
    buf[2] = I0;
    if(ns) buf[2] = I1;
    buf[3] = (buf[1]^buf[2]);
    for(int i = 0 ; i < data_size ; i++){

        buf[4+i] = data[i];
        if(i == 0) BCC = data[i];
        else{

            BCC = (BCC^data[i]);
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