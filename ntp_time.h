#ifndef __NTP_TIME_H__
#define __NTP_TIME_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

//授时服务器 端口默认 123
#define DEF_NTP_SERVER_IP "182.92.12.11"
#define DEF_NTP_PORT 123

//默认请求数据包填充
#define LI 0   //协议头中的元素
#define VN 3   //版本
#define MODE 3 //模式 : 客户端请求
#define STRATUM 0
#define POLL 4  //连续信息间的最大间隔
#define PREC -6 //本地时钟精度

//校验时间计算用到的宏

//ntp时间从年开始，本地时间从年开始，这是两者之间的差值
#define JAN_1970 0x83aa7e80 //3600s*24h*(365days*70years+17days)
//x*10^(-6)*2^32 微妙数转 NtpTime 结构的 fraction 部分
#define NTPFRAC(x) (4294 * (x) + ((1981 * (x)) >> 11))
//NTPFRAC的逆运算
#define USEC(x) (((x) >> 12) - 759 * ((((x) >> 10) + 32768) >> 16))

#define MKSEC(ntpt) ((ntpt).integer - JAN_1970)
#define MKUSEC(ntpt) (USEC((ntpt).fraction))
#define TTLUSEC(sec, usec) ((long long)(sec)*1000000 + (usec))
#define GETSEC(us) ((us) / 1000000)
#define GETUSEC(us) ((us) % 1000000)

#define DATA(i) ntohl(((unsigned int *)data)[i])

#define PDEBUG(fmt, args...) printf("[%s:%d]" fmt "\n", __func__, __LINE__, ##args)
typedef void(*ntpCallBack)(int);
//ntp时间戳结构
typedef struct
{
    unsigned int integer;
    unsigned int fraction;
} NtpTime;
typedef struct ntp_Timeset{
    ntpCallBack callBackfunction;
    int setTryTime;
}ntp_TimeSet;
int do_main(void *arg);
void ntp_get_time(ntpCallBack resultCallback,int in_tryTime);
#endif