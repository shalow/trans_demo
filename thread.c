#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <malloc.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#include "json/json.h"
#include "address.h"
#include "sensor_hb.h"
#include "video_capture.h"

#define LED_IOC_MAGIC 'a'
#define LED_SET_ON        _IOW(LED_IOC_MAGIC, 1, int)
#define LED_SET_OFF        _IOW(LED_IOC_MAGIC, 0, int)

extern unsigned char qrdecode[100];
extern unsigned char ssid[30];
extern unsigned char pass[30];
extern unsigned char loc_ip[20];
extern int qr_len;
extern unsigned char wifi;              //wifi status mark
extern unsigned char local;             //local status mark
extern int stop;
extern pthread_t video_thread;
extern struct camera *cam;

pthread_t nat_thread;
unsigned char nat_ip[20];
unsigned int nat_port;
unsigned int nat_id;
int sockfd_sen;                          //sensor data tcp socket fd 
int sockfd_nat;                          //nat socket
unsigned int vd_msg_dest;
unsigned int vd_msg_source;             // send to server source and dest
unsigned int vd_local_dest;
unsigned int vd_local_source;           //send to local source and dest
unsigned int sen_msg_dest;
unsigned int sen_msg_source;
volatile unsigned char video_st = 0;     //video thread status mark 
volatile unsigned char server = 0;       //server status mark
unsigned char video_to_server = 0;
unsigned char video_to_local = 0;
unsigned char video_use = 0;


void message_decryption(TCP_HEAD *in,int len)      //function to descryption message
{        
           in -> message_mark   = ntohs(in -> message_mark);
           in -> length         = ntohs(in -> length);
           in -> message_source = ntohl(in -> message_source);
           in -> command_code   = ntohs(in -> command_code);
           in -> message_dest   = ntohl(in -> message_dest);
           in -> affair_mark    = ntohl(in -> affair_mark);
           in -> load_type      = in -> load_type;
                    
}

void get_json_object_member(struct json_object *new,char *field,unsigned char jx)   //jx 0:id 1:port
{
   struct json_object *object = json_object_object_get(new,field);
   enum json_type object_type = json_object_get_type(object);
   switch(object_type){
             case json_type_int: 
                  if(jx == 0)  
                    nat_id = json_object_get_int(object);
                  if(jx == 1)
                    nat_port = json_object_get_int(object); 
                  break;
             case json_type_string:          
                  sprintf("nat_ip","%s",json_object_get_string(object));
                  break;    
       }
   json_object_put(object);
}

void execute_command(TCP_HEAD *in,unsigned char *load,int len,unsigned char source)  //according command execute code
{                                                                // source 0  : server  1 :nat
                                                                                        
     if((in->command_code) == 30000)
        {
            printf("server command open led\n");
            int fd_on = open("/dev/led", 0);
            ioctl(fd_on,LED_SET_ON,0);
            close(fd_on);
        }

     if((in->command_code) == 30010)
        {
             printf("server command close led\n");
             int fd_off = open("/dev/led",0);
             ioctl(fd_off,LED_SET_OFF,0);
             close(fd_off);
        }          

     if((in->command_code) == 20110)
        {
          if(source == 0)      //command from server
             {
               vd_msg_dest   = in -> message_dest;
               vd_msg_source = in -> message_source;
               video_to_server = 1;
               printf("server ");
             }
          if(source == 1)     //command from nat
             {             
              vd_local_dest    = in->message_dest;
              vd_local_source  = in->message_source;
              video_to_local = 1;
              printf("local ");
             }
          
          if(video_st == 0)
           {
              printf("command open video\n");
              video_st = 1;
              v4l2_init(cam);
              if (0 != pthread_create(&video_thread, NULL, (void *) capture_encode_thread, NULL)) {
		         printf("thread create fail\n");
                    }
           }               
        }

     if((in->command_code) == 20120)
        {
          int vd_num = 0;                              //the video used time 
          if(video_to_server == 1)
                   vd_num++;
          if(video_to_local == 1)
                   vd_num++;
              
          if((video_st == 1) && (vd_num == 1))       //close vd when video is on and one device is used
           {     
              printf("command close video\n");
              video_st = 0;
              pthread_cancel(video_thread);
             //sleep(1);
              pthread_join(video_thread,NULL);
              v4l2_close(cam);
           }  

          if(source == 0)
               {
                 printf("clsoe video from server\n");
                 video_to_server = 0;
               }
          if(source == 1)
             {
                printf("clsoe video from local\n");
                video_to_local = 0;     
             }          
        }
   
     if((in->command_code) == 10100)
        {
              printf("\ntcp command send sensor data\n");
              sen_msg_dest   = in -> message_dest;
              sen_msg_source = in -> message_source;
              if(source == 0)             //command from server
                   send_tcp_data(0,1);
              if(source == 1)             //command from local
                   send_tcp_data(1,1);    //send by command
        }
/*
     if((in->command_code) == 150)            //Client NAT
       {
              TCP_HEAD nat_msg;
              nat_msg.message_mark    = htons(0xFFFF);
              nat_msg.length          = htons(15);
              nat_msg.message_source  = htonl(in->message_dest);
              nat_msg.command_code    = htons(151);
              nat_msg.message_dest    = htonl(in->message_source);
              nat_msg.affair_mark     = htonl(0);
              nat_msg.load_type       = 1;  
              send(sockfd_sen,&nat_msg,19,0);   //send nat message
              vd_local_dest    = in->message_dest;
              vd_local_source  = in->message_source;
         
              unsigned char json_buff[100];
              memset(json_buff,0,100);
              memcpy(json_buff,load,len-19);
              
              struct json_object *json_obj = json_tokener_parse(json_buff);
              get_json_object_member(json_obj,"id",0);
              get_json_object_member(json_obj,"address",2);
              get_json_object_member(json_obj,"port",1);
 
              if (0 != pthread_create(&nat_thread, NULL, (void *) nat_connect_thread, NULL)) {
		         printf("nat thread create fail\n");
                    }
       }  */  
      
}


void local_send_thread(void)
{
        int sockfd_local;
        struct sockaddr_in server_addr;
        struct sockaddr_in client_addr;
        int rcv_nat; 
        TCP_HEAD *nat_head;
        unsigned char nat_buff[100];      
        memset(nat_buff,0,100); 
 
        if((sockfd_local = socket(AF_INET,SOCK_STREAM,0)) == -1)
         {
             printf("local socket error\n");
         }
        memset(&server_addr,0,sizeof(struct sockaddr_in));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(local_port);

        if(bind(sockfd_local,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr)) == -1)
           {
                printf("local bind error\n");
           }
        if(listen(sockfd_local,5) == -1)
          {
                printf("local listen error\n");
          }

  int sin_size;
  while(1)
  {     
      sin_size = sizeof(struct sockaddr_in);
      printf("wait lcoal connect\n");
      if((sockfd_nat = accept(sockfd_local,(struct sockaddr *)(&client_addr),&sin_size)) == -1)
      {
         printf("accept error\n");
      }
      local = 1;                      //nat bit state
      printf("\nlocal direct connect success\n");

      
       for(;;)                          //circle analysis data
      {          
           rcv_nat = recv(sockfd_nat,nat_buff,100,0); 
   //          printf("local recv %s\n",nat_buff);

   /*     if(rcv_nat == 0)            
          {
             printf("nat recv error 0\n");
             break;
          }   */

        if(rcv_nat == -1)
          {
             printf("nat recv error -1\n");
             break;
          }
  
        if(rcv_nat == 19)             //receive a tcp head message
         {
           printf("\nrecive 19bytes data from local\n");
           nat_head = (TCP_HEAD *)nat_buff;
           message_decryption(nat_head,19);    //command descryption

           if((nat_head->message_mark) != 0xFFFF)
                  continue;           
    
           if((nat_head->command_code) == 1200)
               {
                   printf("\ncommand close nat tcp\n");
                   break;
               }  
          
          execute_command(nat_head,NULL,19,1);    //execute nat command
                 
        }

       
    }
     printf("disconnect local tcp\n");  
     close(sockfd_nat);
     local = 0;
  }

  close(sockfd_local);
}    



void wifi_set_thread(void)              //decode QR string to WIFI SSID and passwords
{
  for(;;)
   { 
      if(stop ==1){
         printf("-----need exit from wifi_set thread------\n");
         break;
          }

      if(wifi == 1){
              memset(ssid,0,30);
              memset(pass,0,30);
              int i_cyc,id_end,ps_sta,ps_end;
              for(i_cyc=0;i_cyc<qr_len;i_cyc++)
               {
                   if(qrdecode[i_cyc] == ';')
                         {
                            id_end = i_cyc;
                            break;
                         }
               }

              for(i_cyc=id_end+1;i_cyc<qr_len;i_cyc++)
               {
                    if(qrdecode[i_cyc] == ';')
                        {      
                           ps_sta = i_cyc;
                           break;
                        }
               }

              for(i_cyc=ps_sta+1;i_cyc<qr_len;i_cyc++)
               {
                    if(qrdecode[i_cyc] == ';')
                         {
                           ps_end = i_cyc;
                           break;
                         }
               }
              memcpy(ssid,qrdecode+7,id_end-7);
              memcpy(pass,qrdecode+ps_sta+3,ps_end-ps_sta-3);
              break;
          }
   }
}



void data_thread(){

          struct sockaddr_in sen_addr;     
          TCP_HEAD login_head;
          TCP_HEAD *rcv_head; 
          unsigned char *load;
          int rcv;
          unsigned char recv_buff[100]; 
 while(1)                                 // reconnect tcp server when tcp close
   { 
      server = 0;                         // set server mark 0 in cycle    
      memset(&sen_addr,0,sizeof(struct sockaddr_in));
      sen_addr.sin_family = AF_INET;
      sen_addr.sin_port = htons(tcp_port);
      sen_addr.sin_addr.s_addr = inet_addr(ip_addr);

     
      printf("try to connect server:\n");
      while(1)
       {
        struct timeval tim;                
        tim.tv_sec = 1;  
        tim.tv_usec = 0;
        select(0,NULL,NULL,NULL,&tim);
        if((sockfd_sen = socket(AF_INET,SOCK_STREAM,0)) ==-1)
         {
            printf("sockfd_sen socket error\n");
            continue;                 //ignore this one 
         }
        if(connect(sockfd_sen,(struct sockaddr *)(&sen_addr),sizeof(struct sockaddr)) == 0)
             break;                   //success
        close(sockfd_sen);            //when connect wrong
        
       }
      printf("\nserver tcp connect success\n");
              
      login_head.message_mark    = htons(0xFFFF);    //server login
      login_head.length          = htons(15);
      login_head.message_source  = htonl(1);
      login_head.command_code    = htons(10000);
      login_head.message_dest    = htonl(0);
      login_head.affair_mark     = htonl(0);
      login_head.load_type       = 1;
      if(send(sockfd_sen,&login_head,19,0) != 19)
            {  
               printf("send login message fail\n");
               close(sockfd_sen);
               continue;
            } 
      server = 1;                     //set server mark 1 , server online    
      
     for(;;)                          //circle analysis data
      {          
           memset(recv_buff,0,100);
           rcv = recv(sockfd_sen,recv_buff,100,0); 
           printf("\nrecive %d bytes data from server\n",rcv);

        if(rcv == 0)               // when recv return abnormal
          {
             printf("server off\n");
             break;
          }

        if(rcv == -1)
          {
             printf("recv error\n");
             break;
          }
  
        if(rcv == 19)             //receive a tcp head message
         {
       //    printf("\nrecive 19bytes data from server\n");
           rcv_head = (TCP_HEAD *)recv_buff;
           message_decryption(rcv_head,19);    //command descryption

           if((rcv_head->message_mark) != 0xFFFF)
                  continue;           
    
           if((rcv_head->command_code) == 1200)
               {
                   printf("server command close tcp\n");
                   break;
               }  
          
          execute_command(rcv_head,NULL,19,0);    //execute server command
                 
        }
        else{                                //receive message with load
           //    printf("recive %d bytes data\n",rcv);
               rcv_head = (TCP_HEAD *)recv_buff;
               load = recv_buff + 19;
               message_decryption(rcv_head,rcv);
               if((rcv_head->message_mark) != 0xFFFF)
                  continue;
               execute_command(rcv_head,load,rcv,0);
            }

      } 
     close(sockfd_sen);
    
  }
 
}

void heart_bt_thread(void){

      int count=0;
      TCP_HEAD heart_msg;
      heart_msg.message_mark = htons(0xFFFF);
      heart_msg.length       = htons(15);
      heart_msg.command_code   = htons(110);
      heart_msg.affair_mark  = htonl(0);
      heart_msg.load_type    = 1;
  
      for(;;)
     {
         struct timeval tcp_bt;
         tcp_bt.tv_sec = 30;
         tcp_bt.tv_usec = 0;
         select(0,NULL,NULL,NULL,&tcp_bt);
         count++;

         if(server == 1)         //server online
             {
              heart_msg.message_source = htonl(1);
              heart_msg.message_dest =  htonl(0);
              send(sockfd_sen,&heart_msg,sizeof(heart_msg),0);
              if(count == 4)    // count 2mins to send sensor data
                {
                  send_tcp_data(0,0);   //send in time
                  count = 0;
                }
             }

         if(local == 1)          //local online
             {
              heart_msg.message_source = htonl(vd_local_dest);
              heart_msg.message_dest =  htonl(vd_local_source);
              send(sockfd_nat,&heart_msg,sizeof(heart_msg),0);
             }
     }

}


