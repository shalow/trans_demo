#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <malloc.h>
#include <linux/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "sensor_hb.h"
#include "address.h"
#include "json/json.h"

extern unsigned char local;
extern unsigned char loc_ip[20];
extern unsigned int sen_msg_dest;
extern unsigned int sen_msg_source;
extern int sockfd_nat;
int fd_i2c;
int fd_tty;
int fd_dht11;

extern int sockfd_sen; //extern sensor data socket fd

int open_port(int fd_tty,int comport)
{
    char *dev[] = {"/dev/ttyS1","/dev/ttyS2"};  
    if(comport == 1)
      {
          fd_tty = open("/dev/ttyS1",O_RDWR|O_NOCTTY);
          if(fd_tty == -1)
              {
                  perror("can not open serial port1\n");
                  return -1;
             }
      }
 
    if(comport == 2)
      {
          fd_tty = open("/dev/ttyS2",O_RDWR|O_NOCTTY|O_NDELAY);
          if(fd_tty == -1)
             {
                 perror("can not open serial port2\n");
                 return -1;
             }
      }
   
    if(fcntl(fd_tty,F_SETFL,0)<0)
        printf("fcntl failed!\n");
    else 
        fcntl(fd_tty,F_SETFL,0);

    if(isatty(STDIN_FILENO) == 0)
        printf("standard input is not a terminal device\n");
    return fd_tty;
}

int set_opt(int fd_tty,int nSpeed,int nBits,char nEvent,int nStop)
{
      struct termios newtio,oldtio;    
      if(tcgetattr(fd_tty,&oldtio) != 0)
        {
            perror("set up serial");
            return -1;
        }

      bzero(&newtio,sizeof(newtio));
      newtio.c_cflag |= CLOCAL | CREAD;
      newtio.c_cflag &= ~CSIZE;

      switch(nBits)
      {
          case 8:
                 newtio.c_cflag |=CS8;
                 break;
     }

     switch(nEvent)
     {
         case 'N':
                 newtio.c_cflag &= ~PARENB;
                 break;
     }
  
     switch(nSpeed)
    {
        case 9600:
                cfsetispeed(&newtio,B9600);
                cfsetospeed(&newtio,B9600);
                break;
    }
 
     if(nStop == 1)
          newtio.c_cflag &= ~CSTOPB;

     newtio.c_cc[VTIME] = 0;
     newtio.c_cc[VMIN] = 0;
     tcflush(fd_tty,TCIFLUSH);

     if((tcsetattr(fd_tty,TCSANOW,&newtio))!=0)
   {
       perror("com set error\n");
       return -1;
   }
     return 0;
}



int send_tcp_data(unsigned char arg,unsigned char by)  //arg :0  send to server
{                                                      //arg :1  send to local
   TCP_HEAD  *tcp_head;
                                                       //by :0  send in time
   unsigned char ctl,buf;                              //by :1  send by command
   int tcp_head_len;

   tcp_head_len = sizeof(TCP_HEAD);
   unsigned char send_buff[150];
   unsigned char buff_tty[10];
   unsigned char result_dht11[6];
   memset(send_buff,0,150);

   unsigned int smog,pm25,pm10,humi,temp;
    
   fd_dht11 = open("/dev/dht11",0);
   if(fd_dht11<0)
  {
     perror("open device dht11 failed\n");
     return -1;
  }

  fd_i2c = open("/dev/i2c-0",O_RDWR);
  if(!fd_i2c)
   {
      printf("error on opening the i2c device file\n");
      return -1;
   }

   ioctl(fd_i2c,I2C_SLAVE,0x48);
   ioctl(fd_i2c,I2C_TIMEOUT,1);
   ioctl(fd_i2c,I2C_RETRIES,1);   

   if((fd_tty=open_port(fd_tty,1))<0)
   {
       perror("open port error\n");
       close(fd_i2c);
       close(fd_dht11);
       return -1;
   }
   int i_tty;
   if((i_tty=set_opt(fd_tty,9600,8,'N',1))<0)
   {
       perror("set opt error\n");
       close(fd_tty);
       close(fd_i2c);
       close(fd_dht11);
       return -1;
    }
   
        tcp_head = (TCP_HEAD *)send_buff;

        struct json_object *infor_object = NULL;
        infor_object = json_object_new_object();
        if(NULL == infor_object)
          {
            printf("new json object create failed\n");
            return -1;
          }
       
        read(fd_dht11,&result_dht11,sizeof(result_dht11));
        if(result_dht11[4] != result_dht11[5])
           {
             printf("dht11 data check wrong!\n");
             close(fd_dht11);
             close(fd_tty);  
             close(fd_i2c);
             return -1;
           }
    humi = result_dht11[0];
    temp = result_dht11[2];
    printf("\nhumi = %d  and  temp = %d\n",result_dht11[0],result_dht11[2]);
   
   int i_i2c;       
   for(i_i2c=0;i_i2c<3;i_i2c++)
      {
        ctl=0x40+i_i2c;     
        write(fd_i2c,&ctl,1);    
        read(fd_i2c,&buf,1);
        if(i_i2c==1)
            smog=(990*buf*3.3/255-99)/3.9+10;        
      }
      printf("smog: %dppm\n",smog);  
   
      int nread_tty;
      bzero(buff_tty,10);
     do
       nread_tty = read(fd_tty,buff_tty,10);
     while(nread_tty==0);
    
        pm25 = (buff_tty[3]*256+buff_tty[2])/10;
        pm10 = (buff_tty[5]*256+buff_tty[4])/10;
        printf("PM2.5=%dug/m3  PM10=%dug/m3\n",pm25,pm10);

        int json_len;
        unsigned char json_buff[100];
        memset(json_buff,0,100);
        json_object_object_add(infor_object,"temperature",json_object_new_int(temp));
        json_object_object_add(infor_object,"humidity",json_object_new_int(humi));
        json_object_object_add(infor_object,"smog",json_object_new_int(smog));
        json_object_object_add(infor_object,"pm25",json_object_new_int(pm25));
        json_object_object_add(infor_object,"pm10",json_object_new_int(pm10)); 
        const unsigned char *json_str = json_object_to_json_string(infor_object);
        strcpy(json_buff,json_str);
        json_len = strlen(json_buff);
        memcpy(send_buff+tcp_head_len,json_buff,json_len);
    

        tcp_head -> message_mark = htons(0xffff);
      
        tcp_head -> message_source = htonl(1);
        tcp_head -> command_code = htons(10100);

        tcp_head -> message_dest = htonl(0);
        tcp_head -> affair_mark = htonl(0);
        tcp_head -> load_type = 1;  
        tcp_head -> length = htons(15+json_len);

        if(by == 0)           //send to server in time
           {
             tcp_head -> message_source = htonl(1);
             tcp_head -> message_dest = htonl(0);
           }

        if(by == 1)          //send by command
          {
             tcp_head -> message_source = htonl(sen_msg_dest);
             tcp_head -> message_dest = htonl(sen_msg_source);
          }

        int package_length = tcp_head_len + json_len;
        int j;
         if(arg == 0)
              j = send(sockfd_sen,&send_buff,package_length,0);
         if(arg == 1)
              j = send(sockfd_nat,&send_buff,package_length,0);
        printf("send %d bytes\n",j); 
   
    close(fd_dht11);
    close(fd_tty);
    close(fd_i2c);
    return 0;
}
   
