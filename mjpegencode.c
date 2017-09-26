#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "jpeglib.h"
#include "mjpegencode.h"
#include "address.h"
#include "zbar.h"
#include "MagickWand/MagickWand.h"


#define  TRUE    1
#define  FALSE    0

extern unsigned char wifi;
extern unsigned char local;
extern unsigned char loc_ip[20];
extern unsigned char qrdecode[100];
extern int qr_len;
extern int sockfd_sen;
extern int sockfd_nat;
extern unsigned int vd_msg_dest;
extern unsigned int vd_msg_source;
extern unsigned char video_to_server;
extern unsigned char video_to_local;
extern unsigned int vd_local_dest;
extern unsigned int vd_local_source;

unsigned char mjpegbuffer[2*1024*1024];
unsigned char send_buff[2*1024*1024];
unsigned char image_name[50];
#define JPG_PC "/tmp/pic/image%d.jpg"
FILE *fp_jpg;


 //pYUV为422，yuv为420
int YUV422To420(unsigned char *pYUV, unsigned char*yuv, int lWidth, int lHeight)
{        
    int i,j;
    unsigned char*pY = yuv;
    unsigned char *pU = yuv + lWidth*lHeight;
    unsigned char *pV = pU + (lWidth*lHeight)/4;
 
    unsigned char *pYUVTemp = pYUV;
    unsigned char *pYUVTempNext = pYUV+lWidth*2;
         
    for(i=0; i<lHeight; i+=2)
    {
        for(j=0; j<lWidth; j+=2)
        {
            pY[j] = *pYUVTemp++;
            pY[j+lWidth] = *pYUVTempNext++;
                         
            pU[j/2] =(*(pYUVTemp) + *(pYUVTempNext))/2;
            pYUVTemp++;
            pYUVTempNext++;
                         
            pY[j+1] = *pYUVTemp++;
            pY[j+1+lWidth] = *pYUVTempNext++;
                         
            pV[j/2] =(*(pYUVTemp) + *(pYUVTempNext))/2;
            pYUVTemp++;
            pYUVTempNext++;
        }
        pYUVTemp+=lWidth*2;
        pYUVTempNext+=lWidth*2;
        pY+=lWidth*2;
        pU+=lWidth/2;
        pV+=lWidth/2;
    }
    return 1;
} 


static int put_jpeg_yuv420p_memory(unsigned char **dest_image,unsigned char *input_image, int width, int height)  
{  
    int i, j, jpeg_image_size;  
  
    JSAMPROW y[16],cb[16],cr[16]; // y[2][5] = color sample of row 2 and pixel column 5; (one plane)  
    JSAMPARRAY data[3]; // t[0][2][5] = color sample 0 of row 2 and column 5  
  
    struct jpeg_compress_struct cinfo;  
    struct jpeg_error_mgr jerr;  
  
    data[0] = y;  
    data[1] = cb;  
    data[2] = cr;  
  
    cinfo.err = jpeg_std_error(&jerr);  // errors get written to stderr   
      
    jpeg_create_compress(&cinfo);  
    cinfo.image_width = width;  
    cinfo.image_height = height;  
    cinfo.input_components = 3;  
    jpeg_set_defaults (&cinfo);  
  
    jpeg_set_colorspace(&cinfo, JCS_YCbCr);  
  
    cinfo.raw_data_in = TRUE;                  // supply downsampled data  
    cinfo.do_fancy_downsampling = FALSE;       // fix segfaulst with v7  
    cinfo.comp_info[0].h_samp_factor = 2;  
    cinfo.comp_info[0].v_samp_factor = 2;  
    cinfo.comp_info[1].h_samp_factor = 1;  
    cinfo.comp_info[1].v_samp_factor = 1;  
    cinfo.comp_info[2].h_samp_factor = 1;  
    cinfo.comp_info[2].v_samp_factor = 1;  
  
    jpeg_set_quality(&cinfo, 80, 1);  
    cinfo.dct_method = JDCT_FASTEST;  
    unsigned long  buffersize;
    jpeg_mem_dest(&cinfo, dest_image, &buffersize );    // data written to mem  
      
    jpeg_start_compress (&cinfo, TRUE);  
  
    for (j = 0; j < height; j += 16) {  
        for (i = 0; i < 16; i++) {  
            y[i] = input_image + width * (i + j);  
            if (i%2 == 0) {  
                cb[i/2] = input_image + width * height + width / 2 * ((i + j) / 2);  
                cr[i/2] = input_image + width * height + width * height / 4 + width / 2 * ((i + j) / 2);  
            }  
        }  
        jpeg_write_raw_data(&cinfo, data, 16);  
    }  
  
    jpeg_finish_compress(&cinfo);  
    jpeg_destroy_compress(&cinfo);  
    return buffersize;  
}


void encode_jpeg_420(unsigned char *start,int w,int h)
{
    int i;
    unsigned long outsize = w*h*3;
    unsigned char *outbuf = (unsigned char*)malloc(sizeof(char)*outsize);
    unsigned char *frame_buffer = (unsigned char*)malloc(sizeof(char)*outsize);
    YUV422To420(start,frame_buffer,w,h);
    outsize = put_jpeg_yuv420p_memory(&outbuf,frame_buffer,w,h);
    unsigned char *ptcur = outbuf;

    if(outsize> 0 ){
       
        for(i = 0;i < outsize;++i){
            if((outbuf[i] == 0xFF) && (outbuf[i+1]) == 0xC4)
                break;
        }

        if(i == outsize){
            printf("huffman tables don't exist!\n");
            goto out;
        }
        ptcur = outbuf;  
        int imagesize  = outsize;

        if(imagesize <= 0)
                goto out;
 
        memcpy(mjpegbuffer,ptcur,imagesize);
   
    if(wifi == 0)
     { 
       static int numb=0;
        sprintf(image_name,JPG_PC,numb++);
        if((fp_jpg = fopen(image_name,"w"))==NULL)
           {
                printf("fail to open image_name\n");
                exit(EXIT_FAILURE);
           }     
        fwrite(mjpegbuffer,imagesize,1,fp_jpg);
        fclose(fp_jpg);
     }

        if(wifi == 1)
            {
              SendFrame(mjpegbuffer,imagesize,w,h);
             }
    
        if(wifi == 0)
             {
               int qr_rt;
               qr_rt = qr_decode();
               if(qr_rt == 0)
                    {
                      wifi = 1;
                      pthread_exit(NULL);
                    }
             }          
    }
out:
    free(outbuf);
    free(frame_buffer);
}



unsigned short SendFrame(unsigned char *jpeg_data, int len, int width, int height)
{
        
	UDP_HEAD udp_head;
        udp_head.message_mark = htons(0xFFFF);
        udp_head.command_code   = htons(140);    
        udp_head.affair_mark    = htonl(0);
        udp_head.load_type      = 1;
        udp_head.length         = htons(15+len);      	
	
        memcpy(send_buff+19, jpeg_data, len);

        if(video_to_local == 1)
         {
          
          udp_head.message_source  = htonl(vd_local_dest);
          udp_head.message_dest    = htonl(vd_local_source);

          memcpy(send_buff,&udp_head,19);
          send(sockfd_nat,send_buff,19+len,0);
         } 
 
      if(video_to_server == 1)
         {
         
          udp_head.message_source = htonl(vd_msg_dest);
          udp_head.message_dest   = htonl(vd_msg_source);
   
          memcpy(send_buff,&udp_head,19);
          send(sockfd_sen,send_buff,19+len,0);
         }
   
       
    return 0;
}


int qr_decode()
{
      zbar_processor_t *processor=NULL;
      MagickBooleanType status;
      int display = 0;
      MagickWandGenesis();  
  processor = zbar_processor_create(0);
  assert(processor);   
  if(zbar_processor_init(processor,NULL,display))
    {
      zbar_processor_error_spew(processor,0);
      return(1);
    }
  
  MagickWand *images = NewMagickWand();
  status = MagickReadImage(images,image_name);
  if(status == MagickFalse)
    {
      printf("mageic read image return status MagickFalse\n");
      exit(-1);
    }

   zbar_image_t *zimage = zbar_image_create();
  
  zbar_image_set_format(zimage,*(unsigned long*)"Y800");
  int width = MagickGetImageWidth(images);
  int height = MagickGetImageHeight(images);
  zbar_image_set_size(zimage,width,height);

  size_t bloblen = width *height;
  unsigned char *blob = (unsigned char *)malloc(bloblen);
  zbar_image_set_data(zimage,blob,bloblen,zbar_image_free_data);
  
  if(!MagickExportImagePixels(images,0,0,width,height,"I",CharPixel,blob))
         return(-1);

   zbar_process_image(processor,zimage);

   const zbar_symbol_t *sym = zbar_image_first_symbol(zimage);
   if(sym == NULL)
       {
          printf("no qr symbol is available\n");
          return -1;
       }
  
   zbar_symbol_type_t typ = zbar_symbol_get_type(sym);
   memset(qrdecode,0,100);
   qr_len = sprintf(qrdecode,"%s",zbar_symbol_get_data(sym));
   if(qr_len < 0)
       {
          printf("sprintf error\n");
          return -1;
       } 
   //printf("%s\n",zbar_symbol_get_data(sym));
   printf("get qr data:%s\n",qrdecode);

   zbar_image_destroy(zimage);  
   DestroyMagickWand(images);
   zbar_processor_destroy(processor);
   MagickWandTerminus();
   return 0;
}
/**********************************************************************/
