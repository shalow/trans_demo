#ifndef SENSOR_HB_H
#define SENSOR_HB_H
#include "json/json.h"
int open_port(int fd_tty,int comport);
int set_opt(int fd_tty,int nSpeed,int nBits,char nEvent,int nStop);
int send_tcp_data(unsigned char arg,unsigned char by);



#pragma pack(push)
#pragma pack(1)
typedef struct 
{
   unsigned short message_mark;
   unsigned short length;
   unsigned int   message_source;
   unsigned short command_code;
   unsigned int message_dest;
   unsigned int affair_mark;
   unsigned char load_type;
}TCP_HEAD;
#pragma pack(pop)

void message_decryption(TCP_HEAD *in,int len);
void execute_command(TCP_HEAD *in,unsigned char *load,int len,unsigned char source);
void get_json_object_member(struct json_object *new,char *field,unsigned char jx);
#endif
