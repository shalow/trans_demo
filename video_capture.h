#ifndef _VIDEO_CAPTURE_H
#define _VIDEO_CAPTURE_H

#include <linux/videodev2.h>

struct buffer {
	void *start;
	size_t length;
};

struct camera {
	char *device_name;
	int fd;
	int width;
	int height;
	struct v4l2_capability v4l2_cap;
	struct v4l2_cropcap v4l2_cropcap;/*设置摄像头的捕捉能力，在捕捉上视频时应先先设置 
v4l2_cropcap 的 type 域，再通过 VIDIO_CROPCAP 操作命令获取设备捕捉能力的参数，
保存于 v4l2_cropcap 结构体中，包括 bounds（最大捕捉方框的左上角坐标和宽高），defrect*/
	struct v4l2_format v4l2_fmt;
	struct v4l2_crop crop;/*设置窗口取景参数 VIDIOC_G_CROP 和 VIDIOC_S_CROP*/
	struct buffer *buffers;
};

void errno_exit(const char *s);
int xioctl(int fd, int request, void *arg);
void open_camera(struct camera *cam);
void close_camera(struct camera *cam);
int read_and_encode_frame(struct camera *cam);
void start_capturing(struct camera *cam);
void stop_capturing(struct camera *cam);
void init_camera(struct camera *cam);
void uninit_camera(struct camera *cam);
void init_mmap(struct camera *cam);
void init_file();
void close_file();
void v4l2_init(struct camera *cam);
void v4l2_close(struct camera *cam);

void wifi_set_thread(void);
void local_send_thread(void);
void data_thread();
void heart_bt_thread(void);
void capture_encode_thread();

#endif

