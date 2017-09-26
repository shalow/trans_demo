#include <asm/types.h>          /* for videodev2.h */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <linux/videodev2.h>
#include <dirent.h>
#include "video_capture.h"
#include "mjpegencode.h"
#define CLEAR(x) memset (&(x), 0, sizeof (x))

typedef unsigned char uint8_t;

FILE *yuv_fp;

unsigned int n_buffers = 0;

void errno_exit(const char *s)
    {
	     fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	     //exit(EXIT_FAILURE);
    }


int xioctl(int fd, int request, void *arg)
   {
	   int r = 0;
	   do {
	  	   r = ioctl(fd, request, arg);
	      } while (-1 == r && EINTR == errno);
     return r;
}


void open_camera(struct camera *cam)
 {
	  struct stat st;
	if (-1 == stat(cam->device_name, &st)) 
		{
		    fprintf(stderr, "Cannot identify '%s': %d, %s\n", cam->device_name,
				errno, strerror(errno));
		    exit(EXIT_FAILURE);
	  }
	if (!S_ISCHR(st.st_mode))
	 {
		  fprintf(stderr, "%s is no device\n", cam->device_name);
	  	exit(EXIT_FAILURE);
	 }
	cam->fd = open(cam->device_name, O_RDWR, 0); //  | O_NONBLOCK
	if (-1 == cam->fd)
	 {
		  fprintf(stderr, "Cannot open '%s': %d, %s\n", cam->device_name, errno,
	  	strerror(errno));
	  	exit(EXIT_FAILURE);
	 }
}


void init_camera(struct camera *cam) 
{
	struct v4l2_capability *cap = &(cam->v4l2_cap);
	struct v4l2_cropcap *cropcap = &(cam->v4l2_cropcap);
	struct v4l2_crop *crop = &(cam->crop);
	struct v4l2_format *fmt = &(cam->v4l2_fmt);
	unsigned int min;

	if (-1 == xioctl(cam->fd, VIDIOC_QUERYCAP, cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", cam->device_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap->capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", cam->device_name);
		exit(EXIT_FAILURE);
	}

	if (!(cap->capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n",
				cam->device_name);
		exit(EXIT_FAILURE);
	}
       /*	
	printf("the camera driver is %s\n", cap->driver);
	printf("the camera card is %s\n", cap->card);
	printf("the camera bus info is %s\n", cap->bus_info);
	printf("the version is %d\n", cap->version); */

	/* Select video input, video standard and tune here. */
	CLEAR(*cropcap);

	cropcap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	crop->c.width = cam->width;
	crop->c.height = cam->height;
	crop->c.left = 0;
	crop->c.top = 0;
	crop->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	CLEAR(*fmt);

	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt->fmt.pix.width = cam->width;
	fmt->fmt.pix.height = cam->height;
	fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; //yuv422
	
	fmt->fmt.pix.field = V4L2_FIELD_INTERLACED; //interlaced scanning

	if (-1 == xioctl(cam->fd, VIDIOC_S_FMT, fmt))
		errno_exit("VIDIOC_S_FMT");

	/* Note VIDIOC_S_FMT may change width and height. */

        struct v4l2_streamparm *setfps;
        setfps = (struct v4l2_streamparm *)calloc(1,sizeof(struct v4l2_streamparm));
        memset(setfps,0,sizeof(struct v4l2_streamparm));
        setfps -> type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        setfps -> parm.capture.timeperframe.numerator = 1;
        setfps -> parm.capture.timeperframe.denominator = 10;
        if(ioctl(cam->fd,VIDIOC_S_PARM,setfps)<0)
           {
             printf("fail to set fps\n");
             exit(EXIT_FAILURE);
           }
        printf("numerator=%d,denominator=%d\n",setfps->parm.capture.timeperframe.numerator,setfps->parm.capture.timeperframe.denominator);


	/* Buggy driver paranoia. */
	  min = fmt->fmt.pix.width * 2;  /*YUYV pixel occupy 2 bypes*/
	if (fmt->fmt.pix.bytesperline < min)
		fmt->fmt.pix.bytesperline = min;
	min = fmt->fmt.pix.bytesperline * fmt->fmt.pix.height;
	if (fmt->fmt.pix.sizeimage < min)
		fmt->fmt.pix.sizeimage = min;
        /*allocate memory，mapped memory*/
	init_mmap(cam);
}


void init_mmap(struct camera *cam) 
{
	struct v4l2_requestbuffers req;
	CLEAR(req);
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	//allocate memory
	if (-1 == xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
					"memory mapping\n", cam->device_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n", cam->device_name);
		exit(EXIT_FAILURE);
	}

	cam->buffers = calloc(req.count, sizeof(*(cam->buffers)));

	if (!cam->buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;
		//turn VIDIOC_REQBUFS data buffer into physical address
		if (-1 == xioctl(cam->fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		cam->buffers[n_buffers].length = buf.length;
		cam->buffers[n_buffers].start = mmap(NULL /* start anywhere */,
				buf.length, PROT_READ | PROT_WRITE /* required */,
				MAP_SHARED /* recommended */, cam->fd, buf.m.offset);

		if (MAP_FAILED == cam->buffers[n_buffers].start)
			errno_exit("mmap");
	}
}


void start_capturing(struct camera *cam) 
{
	unsigned int i;
	enum v4l2_buf_type type;
	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(cam->fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(cam->fd, VIDIOC_STREAMON, &type))
		errno_exit("VIDIOC_STREAMON");
}


void init_file() {
	 printf("video init success\n");       
}


int read_and_encode_frame(struct camera *cam) 
{
	struct v4l2_buffer buf;
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	//this operator below will change buf.index and (0 <= buf.index <= 3)
	if (-1 == xioctl(cam->fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			return 0;
		case EIO:
		default:
			errno_exit("VIDIOC_DQBUF err1");
		}
	}
  
	encode_jpeg_420(cam->buffers[buf.index].start,cam->width,cam->height);

       if (-1 == xioctl(cam->fd, VIDIOC_QBUF, &buf))
		     errno_exit("VIDIOC_QBUF err2");

	return 1;
}


void stop_capturing(struct camera *cam) 
{
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(cam->fd, VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");
}


void uninit_camera(struct camera *cam) 
{
	unsigned int i;
	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap(cam->buffers[i].start, cam->buffers[i].length))
			errno_exit("munmap");
	free(cam->buffers);
}


void close_camera(struct camera *cam)
{
	  if (-1 == close(cam->fd))
		errno_exit("close");
	  cam->fd = -1;
}



void close_file() {
          printf("video close success\n");
}



void v4l2_init(struct camera *cam) 
{
	open_camera(cam);
	init_camera(cam);
	start_capturing(cam);
	init_file();
}


void v4l2_close(struct camera *cam) 
{
	stop_capturing(cam);
	uninit_camera(cam);
	close_camera(cam);
//	free(cam);
	close_file();
}

