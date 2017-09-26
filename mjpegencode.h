#ifndef __JPEGENCODE_H__
#define __JPEGENCODE_H__

#include <stdint.h>

unsigned short SendFrame(unsigned char *jpeg_data, int len, int width, int height);
                         
void encode_jpeg_420(unsigned char *start,int w,int h);

static int put_jpeg_yuv420p_memory(unsigned char **dest_image,unsigned char *input_image, int width, int height) ;

int YUV422To420(unsigned char *pYUV, unsigned char*yuv, int lWidth, int lHeight);

typedef struct {
	  unsigned char version:2;
	  unsigned char p:1;
	  unsigned char x:1;		
    unsigned char cc:4;
    
    
    unsigned char m:1;
    unsigned char pt:7;
    
    unsigned short seq;
    uint32_t ts;
    uint32_t ssrc;
}rtp_hdr_t;

#define RTP_HDR_SZ 12
 /* The following definition is from RFC1890 */
 #define RTP_PT_JPEG             26

 struct jpeghdr {
    unsigned int tspec:8;   /* type-specific field */
    unsigned int off:24;    /* fragment byte offset */
    unsigned char type;            /* id of jpeg decoder params */
    unsigned char q; /* quantization factor (or table id) */
    unsigned char width;           /* frame width in 8 pixel blocks */
    unsigned char height;          /* frame height in 8 pixel blocks */

};
struct jpeghdr_rst{
    unsigned short dri;
    unsigned int f:1;
    unsigned int l:1;
    unsigned int count:14;
 };
struct jpeghdr_qtable {
    unsigned char  mbz;
    unsigned char  precision;
    unsigned short length;

};

#define RTP_JPEG_RESTART           0x40

#pragma pack(push)
#pragma pack(1)
typedef struct 
{
	unsigned short message_mark;
	unsigned short length;
	unsigned int message_source;
	unsigned short command_code;
        unsigned int message_dest;
        unsigned int affair_mark;
        unsigned char load_type;
}UDP_HEAD;
#pragma pack(pop)

void encode_jpeg_420(unsigned char *start,int w,int h);
void encode_jpeg();
int qr_decode();
#endif
