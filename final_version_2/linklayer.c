#include "linklayer.h"

// writes contents of globalbuf to serial connection
void timeout_retransmission();
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
// print format:
// size = <size>
// 0x## 0x## ... 0x## 0x## (first and last five bytes)
void print_message_2(const unsigned char* const buf, const int buf_size);
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

void timeout_retransmission(){

    if(fd1 < 0){

        printf("unexpected call of timeout_retransmissions\n");
        return;
    }

    if(flag == 0) return; // switch off retransmission with flag

    timeout_count++;

    // print contents of global buf
    printf("globalbuf (%d/%d):\n", globalcount, max_retransmissions);
    print_message_2(globalbuf, globalbuf_size);

    // write contents of globalbuf to serial connection
    int res = write(fd1, globalbuf, globalbuf_size);
    if(res != globalbuf_size){

        printf("error: write in timeout_retransmission\n");
        exit(-1);
    }

    if(globalcount >= max_retransmissions){

        printf("error: max_retransmissions\n");
        exit(-1);
    }
    globalcount++; // retransmission count
    alarm(timeout); // set new alarm
}

int llopen(linkLayer connectionParameters){

    int i;
    int res;
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

    // define function to call when alarm activated
    (void) signal(SIGALRM, timeout_retransmission);

    /*
    ESTABLISHMENT PHASE:
    
    T -----SET-----> R
    T <-----UA------ R
    */

    timeout_count = 0;
    I0_count = 0;
    I1_count = 0;
    REJ0_count = 0;;
    REJ1_count = 0;
    RR0_count = 0;
    RR1_count = 0;

    if(connectionParameters.role == TRANSMITTER){

        // construct SET command
        res = construct_control_frame(A1, SET, buf, 255);
        if(res != 5){

            printf("error: construct_control_frame\n");
            return -1; // error
        }

        // print SET command
        printf("send (SET command):\n");
        print_message_2(buf, res);

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
        
        globalcount = 1; // start counting retransmissions on timeout
        flag = 1; // switch on retransmissions
        alarm(timeout);  // set alarm
        
        // read UA reply
        res = read_control_frame(buf, 255); 
        flag = 0; // switch off retransmissions
        if(res != 5){ // check control frame

            printf("error: read_control_frame");
            return -1; // error
        }

        // print UA reply
        printf("received (UA reply): ");
        print_message_2(buf, res);

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
        print_message_2(buf, res);

        // check SET command
        if(buf[3] != (A1^SET)){

            printf("error: not SET command\n");
            return -1;
        }

        // construct UA reply
        res = construct_control_frame(A1, UA, buf, 255);
        if(res != 5){

            printf("error: contruct_control_frame\n");
            return -1; // error
        }

        // print UA reply
        printf("send (UA reply):\n");
        print_message_2(buf, res);

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

    // check parameters and global variables
    if(fd1 < 0 || buf == NULL || bufSize > MAX_PAYLOAD_SIZE) return -1;

    static int ns = 0; // sequence number
    unsigned char data[2*MAX_PAYLOAD_SIZE];
    int data_size;
    unsigned char frame[2*MAX_PAYLOAD_SIZE+6];
    int frame_size;
    int ret;
    unsigned char c, c2, c3, c4;

    // print data
    printf("data:\n");
    print_message_2(buf, bufSize);

    // byte stuffing
    data_size = byte_stuffing(data, 2*MAX_PAYLOAD_SIZE, buf, bufSize);
    if(data_size == -1){

        printf("error: byte_stuffing\n");
        return -1;
    }

    // print data after byte stuffing
    printf("data after byte stuffing:\n");
    print_message_2(data, data_size);

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

    globalcount = 1;
    while(globalcount <= max_retransmissions){

        // construct information frame
        frame_size = construct_information_frame(frame, 2*MAX_PAYLOAD_SIZE+6, data, data_size, ns);
        ns = (ns+1)%2; // update sequence number
        if(frame_size == -1){

            printf("error: construct_information_frame\n");
            return -1;
        }

        // print information frame
        printf("send (information frame):\n");
        print_message_2(frame, frame_size);

        // send information frame
        ret = write(fd1, frame, frame_size);
        if(ret != frame_size){

            printf("error: write\n");
            return -1;
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
            return -1;
        }

        // c is expected RR reply
        // c2 is expected REJ reply
        if(ns){

            c = RR1;
            c2 = REJ1;
        }
        else{

            c = RR0;
            c2 = REJ0;
        }

        switch(frame[2]){

            case RR0:
                RR0_count++;
                break;
            case RR1:
                RR1_count++;
                break;
            case REJ0:
                REJ0_count++;
                break;
            case REJ1:
                REJ1_count++;
                break;
        }

        if(frame[3] == (A1^c)){ // data arrived correctly (RR)

            // print RR reply
            printf("received (RR%d reply):\n", ns);
            print_message_2(frame, frame_size);
            break; // stop retransmissions
        }
        else if(frame[3] == (A1^c2)){ // data arrived with errors (REJ)

            // print REJ reply
            printf("received (REJ%d reply):\n", ns);
            print_message_2(frame, frame_size);
            ns = (ns+1)%2; // update sequence number
            // continue retransmissions
        }
        else{ // wrong BCC, only happens if errors occur

            printf("error: BCC\n");
            return -1;
        }

        globalcount++; // update retransmission count
    }
    return ret; // number of sent bytes in the frame (bigger than original size of data)
}

int llread(unsigned char* packet){

    // check parameters and global variables
    if(fd1 < 0 || packet == NULL) return -1;

    static int ns = 0; // sequence number
    unsigned char frame[2*MAX_PAYLOAD_SIZE+6];
    int frame_size;
    int ret;
    unsigned char c, c2;
    int i;
    int retransmissions;

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

    retransmissions = 1;
    while(retransmissions <= max_retransmissions){

        printf("retransmissions = (%d/%d)\n", retransmissions, max_retransmissions);
        // read information frame
        frame_size = read_information_frame(frame, 2*MAX_PAYLOAD_SIZE+6);
        if(frame_size == -1){

            printf("error: read_information_frame\n");
            return -1; // error
        }

        // print information frame
        printf("received (information frame):\n");
        print_message_2(frame, frame_size);

        // check BCC1
        // c - correct sequence number
        // c2 - wrong sequence number
        if(ns){

            c = I1;
            c2 = I0;
        } 
        else{

            c = I0;
            c2 = I1;
        }
        ns = (ns+1)%2; // update sequence number

        switch(frame[2]){

            case I0:
                I0_count++;
                break;
            case I1:
                I1_count++;
                break;
        }

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
        // save BCC2 in c
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
            print_message_2(frame, frame_size);

            // send REJ reply
            frame_size = write(fd1, frame, 5);
            if(frame_size != 5){

                printf("error: write\n");
                return -1;
            }
        }
        else{ // BCC2 ok

            break; // stop retransmissions
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
    print_message_2(packet, ret);

    // construct RR0/RR1 reply
    if(ns) c = RR1;
    else c = RR0;
    frame_size = construct_control_frame(A1, c, frame, 2*MAX_PAYLOAD_SIZE+6);
    if(frame_size!=5){

        printf("error: construct_control_frame\n");
        return -1;
    }

    // print RR0/RR1 reply
    printf("send (RR%d reply):\n", ns);
    print_message_2(frame, frame_size);

    // send RR0/RR1 reply
    frame_size = write(fd1, frame, 5);
    if(frame_size != 5){

        printf("error: write\n");
        return -1; // error - write
    }

    return ret; // number of bytes read into packet (original data size)
}

int llclose(linkLayer connectionParameters, int showStatistics){

    if(fd1 < 0){

        printf("error: fd1\n");
        return -1;
    }

    int i;
    int frame_size;
    unsigned char frame[255];

    /*
    TERMINATION:
    T ------DISC-------> R
    T <-----DISC-------- R
    T ------UA---------> R
    */
    if(connectionParameters.role == TRANSMITTER){

        if(showStatistics){

            printf("\nSTATISTICS:\n");
            printf("timeout_count = %d\n", timeout_count);
            printf("RR0_count = %d\n", RR0_count);
            printf("RR1_count = %d\n", RR1_count);
            printf("REJ0_count = %d\n", REJ0_count);
            printf("REJ1_count = %d\n\n", REJ1_count);
            
        }

        // construct DISC command
        frame_size = construct_control_frame(A1, DISC, frame, 255);
        if(frame_size != 5){

            printf("error: construct_control_frame\n");
            return -1; // error - construct control frame
        }

        // print DISC command
        printf("send (DISC command):\n");
        print_message_2(frame, frame_size);
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
        globalcount = 1; // start counting retransmissions
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
        print_message_2(frame, frame_size);

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
        print_message_2(frame, frame_size);

        // send UA reply
        frame_size = write(fd1, frame, 5);
        if(frame_size != 5){

            printf("error: write\n");
            return -1;
        }
    }
    else if(connectionParameters.role == RECEIVER){

        if(showStatistics){

            printf("\nSTATISTICS:\n");
            printf("I0_count = %d\n", I0_count);
            printf("I1_count = %d\n", I1_count);
        }
        // read DISC command
        frame_size = read_control_frame(frame, 255);
        if(frame_size != 5){

            printf("error: read_control_frame\n");
            return -1;
        }

        // print DISC command
        printf("received (DISC command):\n");
        print_message_2(frame, frame_size);

        // check DISC command
        if(frame[3] != (A1^DISC)){

            printf("error: not DISC command\n");
            return -1;
        }

        // construct DISC command from receiver
        frame_size = construct_control_frame(A2, DISC, frame, 255);
        if(frame_size != 5){

            printf("error: construct_control_frame\n");
            return -1; // error - construct control frame
        }

        // print DISC command
        printf("send (DISC command):\n");
        print_message_2(frame, frame_size);

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
        globalcount = 1; // start counting retransmissions
        flag = 1; // switch on retransmissions on timeout
        alarm(timeout); // start timer
        // receive UA reply from transmitter
        frame_size = read_control_frame(frame, 255);
        flag = 0; // switch off retransmissions on timeout
        if(frame_size != 5){

            printf("error: read_control_frame\n");
            return -1; // error - read control frame
        }

        // print UA reply
        printf("received (UA reply):\n");
        print_message_2(frame, frame_size);

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

        //printf("state=%d\n", state);
        
        // read next byte
        res = read(fd1, (void*)(buf+state), 1);
        if(res != 1) return -1;

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
    // printf("state=%d\n", state);

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
    
        //printf("state=%d i=%d\n", state, i);
        
        // read next byte
        res = read(fd1, (void*)(buf+state+i), 1);
        if(res != 1) return -1;
        
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

void print_message_2(const unsigned char* const buf, const int buf_size){

    printf("size = %d\n", buf_size);
    if(buf_size > 10){

        print_message(buf, 5);
        printf("...");
        print_message(buf+buf_size-5, 5);
    }
    else print_message(buf, buf_size);
    printf("\n");
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
