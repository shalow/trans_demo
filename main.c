#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

#include "video_capture.h"
#include "address.h"

unsigned char qrdecode[100];
unsigned char ssid[30];
unsigned char pass[30];
unsigned char loc_ip[20];
int qr_len;
volatile unsigned char wifi=0;
volatile unsigned char local=0;

struct camera *cam;

pthread_t video_thread;
pthread_t data;
pthread_t wifi_set;
pthread_t local_send;
pthread_t heart_bt;


extern unsigned char video_st;
volatile int isigal = 0;
volatile int stop = 0;
void signal_handler(int sigm)
{
    if(isigal == 0){
        stop = 1;
        usleep(1000*400);
            if(video_st == 1)
              {
               pthread_cancel(video_thread);
               pthread_join(video_thread,NULL);
	       v4l2_close(cam);
               }
        isigal++;
        pthread_cancel(data); 
        pthread_cancel(local_send);
    }
}



void capture_encode_thread() {
          	
	for (;;) {

		if (stop == 1 ){
			printf("------need to exit from video thread------- \n");
			break;
		}
              
		fd_set fds;
		struct timeval tv;
		int r;
		FD_ZERO(&fds);
		FD_SET(cam->fd, &fds);

		/* Timeout. */
		tv.tv_sec = 3;
		tv.tv_usec = 0;
		r = select(cam->fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == r) {
			if (EINTR == errno)
				continue;
			errno_exit("select");
		}

		if (0 == r) {
			fprintf(stderr, "select timeout\n");
			exit(EXIT_FAILURE);
		}

		if (read_and_encode_frame(cam) != 1) {
			fprintf(stderr, "read_fram fail in thread\n");
			break;
		} 
	}       
}



int main(int argc,unsigned char **argv) {
        
    if(signal(SIGINT,signal_handler) == SIG_ERR){
        fprintf(stderr,"signal send error\n");
    }
  
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGPIPE);
    sigprocmask(SIG_BLOCK,&set,NULL);    // ignore SIGPIPE signal
    
    cam = (struct camera *) malloc(sizeof(struct camera));
	if (!cam) {
		printf("malloc camera failure!\n");
		exit(1);
	}
	cam->device_name = "/dev/video0";
	cam->buffers = NULL;
	cam->width = 320;
	cam->height = 240;

   
  if((*argv[1]) == 'n')                 // n: network is unreachable
  {
      wifi = 0;
      v4l2_init(cam);
 //QR decode
 if (0 != pthread_create(&video_thread, NULL, (void *) capture_encode_thread, NULL)) {
		fprintf(stderr, "thread create fail\n");
    }                                                
 //set wifi
 if(0 != pthread_create(&wifi_set,NULL,(void *)wifi_set_thread,NULL)){
                printf("thread wifi_set create failed\n");
                return -1;
    }
  pthread_join(video_thread,NULL);
  v4l2_close(cam);
  pthread_join(wifi_set,NULL);

  if(stop != 1)
    {
       pid_t pid;
       if((pid =fork()) < 0)
              printf("fork error\n");
       else if(pid == 0)
         {
             if(execl("/root/wifi_set.py","wifi_set.py",ssid,pass,(char *)0)<0)
                {
                    printf("execl error\n");  
                    exit(1);
                }        
         }    
    } 

 }

  cam->width = 640;
  cam->height = 480;    //change picture width and height
  wifi = 1;

  if(0 != pthread_create(&data,NULL,(void *)data_thread,NULL)){
                 fprintf(stderr,"thread sensor create failed\n"); 
                 return -1;
    }

  if(0 != pthread_create(&heart_bt,NULL,(void *)heart_bt_thread,NULL)){
                 printf("heart beat thread create error\n");
                 return -1;
    }


  if(0 != pthread_create(&local_send,NULL,(void *)local_send_thread,NULL)){
                printf("thread local_send create failed\n");
                return -1;
    }  
 
        pthread_join(data,NULL);
        printf("-----------end program------------\n");
        free(cam);
	

	return 0;
}
