#include "ntp_time.h"
#include <stdio.h>
void Getresult(int resultBack){
    printf("+++ return is %d\r\n",resultBack);
}
int main(){
    int try_Max=10;
    ntp_get_time(Getresult,try_Max);
    while(1){
        printf("main\r\n");
        sleep(1);
    }
    return 0;
}