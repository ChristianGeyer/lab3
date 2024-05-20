#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

int flag=1, conta=1;
time_t tstart, delta;

void atende()                   // atende alarme
{
    printf("alarme # %d\n", conta);
    flag=1;
    conta++;
    delta = time(NULL) - tstart;
    tstart = time(NULL);
    printf("delta = %ld\n", delta);
    alarm(3);
}


int main()
{
    (void) signal(SIGALRM, atende);  // instala rotina que atende interrupcao

    tstart = time(NULL);
    alarm(3);
    sleep(1);
    alarm(3);
    conta = 1;
    while (conta < 4);
    printf("Vou terminar.\n");

    return 0;
}
