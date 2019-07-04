#include "ntp_time.h"
static int errTimeCount = 0;

int send_packet(int fd)
{
  unsigned int data[12];
  int ret;
  struct timeval now;
  if (sizeof(data) != 48)
  {
    // PDEBUG("data 长度小于48!");
    return -1;
  }
  memset((char *)data, 0, sizeof(data));
  data[0] = htonl((LI << 30) | (VN << 27) | (MODE << 24) | (STRATUM << 16) |
                  (POLL << 8) | (PREC & 0xff));
  data[1] = htonl(1 << 16);
  data[2] = htonl(1 << 16);
  gettimeofday(&now, NULL);
  data[10] = htonl(now.tv_sec + JAN_1970);
  data[11] = htonl(NTPFRAC(now.tv_usec));
  ret = send(fd, data, 48, 0);
  return ret;
  PDEBUG("发送请求数据包到服务器, ret: %d\n", ret);
}

int get_server_time(int sock, struct timeval *newtime)
{
  int ret;
  unsigned int data[12];
  NtpTime oritime, rectime, tratime, destime;
  struct timeval offtime, dlytime;
  struct timeval now;
  bzero(data, sizeof(data));
  ret = recvfrom(sock, data, sizeof(data), 0, NULL, 0);
  if (ret == -1)
  {
    PDEBUG("读取返回数据失败\n");
    return -1;
  }
  else if (ret == 0)
  {
    PDEBUG("读取到时间长度: 0!\n");
    return 0;
  }
  // 1970逆转换到1900
  gettimeofday(&now, NULL);
  destime.integer = now.tv_sec + JAN_1970;
  destime.fraction = NTPFRAC(now.tv_usec);
  //字节序转换
  oritime.integer = DATA(6);
  oritime.fraction = DATA(7);
  rectime.integer = DATA(8);
  rectime.fraction = DATA(9);
  tratime.integer = DATA(10);
  tratime.fraction = DATA(11);
  long long orius, recus, traus, desus, offus, dlyus;
  orius = TTLUSEC(MKSEC(oritime), MKUSEC(oritime));
  recus = TTLUSEC(MKSEC(rectime), MKUSEC(rectime));
  traus = TTLUSEC(MKSEC(tratime), MKUSEC(tratime));
  desus = TTLUSEC(now.tv_sec, now.tv_usec);
  offus = ((recus - orius) + (traus - desus)) / 2;
  dlyus = (recus - orius) + (desus - traus);
  offtime.tv_sec = GETSEC(offus);
  offtime.tv_usec = GETUSEC(offus);
  dlytime.tv_sec = GETSEC(dlyus);
  dlytime.tv_usec = GETUSEC(dlyus);
  struct timeval new;
  //粗略校时
  // new.tv_sec = tratime.integer - JAN_1970;
  // new.tv_usec = USEC(tratime.fraction);
  //精确校时
  new.tv_sec = destime.integer - JAN_1970 + offtime.tv_sec;
  new.tv_usec = USEC(destime.fraction) + offtime.tv_usec;
  //提取现在好的时间
  *newtime = new;
  //printf("get server time success!\n");
  return 1;
}

int mod_localtime(struct timeval newtime)
{  //设置方式1
  //PDEBUG("设置时间!\n");
#if 1
  //只有 root 用户拥有修改时间的权限
  if (getuid() != 0 && geteuid() != 0)
  {
    // PDEBUG("不是 root 用户，无法进行时间校准，被迫终止 !\n");
    return -1;
  }
  if (settimeofday(&newtime, NULL) == -1)
  {
    PDEBUG("设置时间失败 !\n");
    sleep(2);
    return -1;
  }
  else
  {
    PDEBUG("设置时间成功 !\n");
    sleep(2);
  }
#endif
  //设置方式2(对于settimeofday函数不可用情况)
#if 0
	struct tm *tm_p;
    char time_buff[100];
    memset(time_buff, 0, 100);
	time_t time_sec = newtime.tv_sec + 28800;
    tm_p = gmtime(&time_sec);
    sprintf(time_buff, "date -s \"%d-%d-%d %d:%d:%d\"", tm_p->tm_year + 1900, tm_p->tm_mon + 1, tm_p->tm_mday, tm_p->tm_hour, tm_p->tm_min, tm_p->tm_sec);
    system(time_buff);
#endif
  return 1;
}

int ntp_get_time_run(void *arg)
{
  ntp_TimeSet *in_setting = (ntp_TimeSet *)malloc(sizeof(struct ntp_Timeset));
  //in_setting = (ntp_TimeSet *)arg;
  ntp_TimeSet *Arg = (ntp_TimeSet *)arg;
  in_setting->setTryTime  = Arg->setTryTime;
  in_setting->callBackfunction  = Arg->callBackfunction;
  ntpCallBack resultCallback = in_setting->callBackfunction;
  int in_tryTime = in_setting->setTryTime;
  //printf("in_tryTime %d\r\n",in_tryTime); 
  int ret, sock;
  struct timeval newtime, timeout;  //<sys/time.h>
  int addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in addr_dst;  //服务器 socket
    // UDP数据报套接字
  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (-1 == sock)  // sock == -1 //sock = -1
  {
    // close(sock);
    PDEBUG("套接字创建失败，被迫终止 ! \n");
    resultCallback(-1);
  }
  memset(&addr_dst, 0, addr_len);
  addr_dst.sin_family = AF_INET;
  addr_dst.sin_port = htons(DEF_NTP_PORT);
  addr_dst.sin_addr.s_addr = inet_addr(DEF_NTP_SERVER_IP);
  if (-1 == connect(sock, (struct sockaddr *)&addr_dst, addr_len))
  {
    sleep(5);
    PDEBUG("连接服务器失败，被迫终止 !\n");
    resultCallback(-1);
  }
  send_packet(sock);
  int tryTime = 1;
  while (1)
  {
    // send_packet(sock);
    if (tryTime++ > in_tryTime)
    {
      resultCallback(0);
      break;
    }
    fd_set fds_read;
    FD_ZERO(&fds_read);
    FD_SET(sock, &fds_read);
    timeout.tv_sec = 30, timeout.tv_usec = 0;
    ret = select(sock + 1, &fds_read, NULL, NULL, &timeout);
    //printf("ret is %d\n", ret);
    if (ret == -1)
    {
      PDEBUG("select函数出错，被迫终止 !\n");
      continue;
    }
    if (ret == 0)
    {
      // PDEBUG("等待服务器响应超时，重发请求 !\n");
      //向服务器发送数据
      send_packet(sock);
    }
    if (FD_ISSET(sock, &fds_read))
    {
      if (get_server_time(sock, &newtime) < 1)
      {
        send_packet(sock);
        // exit(-1);
        printf("recv is overtime");
        continue;
      }
      if (mod_localtime(newtime) < 1)
      {
        send_packet(sock);
        continue;
      }
      //发送 ntp 包
      send_packet(sock);
    }
  }
  close(sock);
  free(in_setting);
  //printf("in_setting is free\n");
  return 0;
}

void ntp_get_time(ntpCallBack resultCallback,int in_tryTime)
{
  pthread_t main_tid;
  pthread_attr_t attr;
  int ret;
  ntp_TimeSet *ntpTimeTest = (ntp_TimeSet *)malloc(sizeof(struct ntp_Timeset));
  ntpTimeTest->setTryTime = in_tryTime;
  ntpTimeTest->callBackfunction = *resultCallback;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  ret = pthread_create(&main_tid, &attr, ntp_get_time_run, (void *)ntpTimeTest);
  if(0 == pthread_attr_destroy(&attr)){
    sleep(3);
    free(ntpTimeTest);
    //printf("ntpTimeTest is free\n");
    //printf("ntp_get_time_run is free\n");
  }
  if (ret)
  {
    perror("线进程创建失败");
    return -1;
  }
  return 0;
}
