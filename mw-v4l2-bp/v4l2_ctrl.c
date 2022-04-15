#include <mw_base/time.h>
#include "mw_media/v4l2_ctrl.h"

static int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
	unsigned int pixel32 = 0;
	unsigned char *pixel = (unsigned char *)&pixel32;
	int r, g, b;
	r = y + (1.370705 * (v-128));
	g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
	b = y + (1.732446 * (u-128));
	if(r > 255) r = 255;
	if(g > 255) g = 255;
	if(b > 255) b = 255;
	if(r < 0) r = 0;
	if(g < 0) g = 0;
	if(b < 0) b = 0;
	pixel[0] = r ;
	pixel[1] = g ;
	pixel[2] = b ;
	return pixel32;
}

int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
	unsigned int in, out = 0;
	unsigned int pixel_16;
	unsigned char pixel_24[3];
	unsigned int pixel32;
	int y0, u, y1, v;

	for(in = 0; in < width * height * 2; in += 4)
	{
		pixel_16 =
			yuv[in + 3] << 24 |
			yuv[in + 2] << 16 |
			yuv[in + 1] <<  8 |
			yuv[in + 0];
		y0 = (pixel_16 & 0x000000ff);
		u  = (pixel_16 & 0x0000ff00) >>  8;
		y1 = (pixel_16 & 0x00ff0000) >> 16;
		v  = (pixel_16 & 0xff000000) >> 24;
		pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];
		pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];
	}
	return 0;
}

/**
 *open devices
 */
int mw_test_device_exist(const char *devName)
{
	struct stat st;
	if (-1 == stat(devName, &st))
		return -1;

	if (!S_ISCHR (st.st_mode))
		return -1;

	return 0;
}

//获取摄像头名称
char *mw_get_v4l2_name(const char * dev_name,char *camera_name)
{
	struct v4l2_capability cap;

	int fd = open(dev_name, O_RDWR);
	if(ioctl(fd, VIDIOC_QUERYCAP, &cap) != -1)
	{
		strcpy(camera_name, (char *)cap.card);
	}
	close(fd);

	return camera_name;
}

#if 0
void mw_get_v4l2_names()
{
    char dev_name[15] = "";
    static char camName[32] = "";
    int index;
    printf("当前挂载设备名称：\n");
    for(index = 0; ; index++)
    {
        sprintf(dev_name, "%s%d", "/dev/video", index);
        if(mw_test_device_exist(dev_name) == 0){
            printf("\t%d. %s(%s)\n", index, dev_name, mw_get_v4l2_name(dev_name, camName));
            memset(dev_name, 0, sizeof(dev_name));
        }else{
            break;
        }
    }
}
#endif

int mw_open_device(const char* dev_name)
{
	int fd = -1;
	if(mw_test_device_exist(dev_name) == 0) {
		fd = open(dev_name, O_RDWR | O_NONBLOCK);
		if(fd < 0)
			printf("open %s failed!\n",dev_name);
		else
			printf("%s is not exist!\n",dev_name);
	}
	return fd;
}

uint32_t mw_get_current_v4l2_pixel_format(int fd)
{
	uint32_t current_pixel_format = -1;
	struct v4l2_format format;

	memset(&format, 0, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( ioctl(fd, VIDIOC_G_FMT, &format) != -1)
		current_pixel_format= format.fmt.pix.pixelformat;
	else
		current_pixel_format= -1;

	return current_pixel_format;
}

int mw_get_current_resolution_width(int fd)
{
	struct v4l2_format format;
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
		return -1;
	return format.fmt.pix.width;
}

int mw_get_current_resolution_height(int fd)
{
	struct v4l2_format format;
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
		return -1;
	return format.fmt.pix.height;
}

//初始化mmap，申请队列用于接收视频帧
mw_v4l2_buffer_t *mw_init_v4l2_buffers_mmap(int fd, int reqbuf_count)
{
	mw_v4l2_buffer_t *mmap_buffers = NULL;
	struct v4l2_buffer buffer;
	struct v4l2_requestbuffers reqbuf;

	//申请帧缓存区
	memset (&reqbuf, 0, sizeof (reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = reqbuf_count;

	//初始化内存映射或者用户指针IO
	if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
		if (errno == EINVAL)
			printf ("Video capturing or mmap-streaming is not supported\n");
		else
			perror ("VIDIOC_REQBUFS");
		return NULL;
	}

	//分配缓存区
	mmap_buffers = calloc (reqbuf.count, sizeof (*mmap_buffers));
	if(mmap_buffers == NULL)
		perror("buffers is NULL");
	else
		assert (mmap_buffers != NULL);

	//mmap内存映射
	int i;
	for (i = 0; i < (int)reqbuf.count; i++) {
		memset (&buffer, 0, sizeof (buffer));
		buffer.type = reqbuf.type;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buffer)) {
			perror ("VIDIOC_QUERYBUF");
			return NULL;
		}

		mmap_buffers[i].length = buffer.length;
		mmap_buffers[i].start = mmap (NULL, buffer.length,
			 PROT_READ | PROT_WRITE,
			 MAP_SHARED,
			 fd, buffer.m.offset);

		if (MAP_FAILED == mmap_buffers[i].start) {
			perror ("mmap");
			return NULL;
		}
	}

	//将缓存帧放到队列中等待视频流到来
	for(i = 0; i < reqbuf.count; i++){
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;
		if (ioctl(fd,VIDIOC_QBUF,&buffer)==-1) {
			printf("VIDIOC_QBUF-query buff failed,buffer.index=%d\n",i);
			return NULL;
		}
	}

	return mmap_buffers;
}

unsigned char *mw_init_grb_24(int fd)
{
	unsigned char *rgb24 = NULL;
	uint32_t current_pixel_format = -1;
	int width,height;

	current_pixel_format = mw_get_current_v4l2_pixel_format(fd);
	width = mw_get_current_resolution_width(fd);
	height = mw_get_current_resolution_height(fd);

	if(current_pixel_format == V4L2_PIX_FMT_YUYV){
		rgb24 = (unsigned char*)malloc(width*height*3*sizeof(char));
		assert(rgb24 != NULL);
	}
	return rgb24;
}

void start_video_stream(int fd)
{
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd,VIDIOC_STREAMON,&type) == -1)
		printf("VIDIOC_STREAMON-start video stream failed!\n");
}

void end_video_stream(int fd)
{
	//关闭视频流
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd,VIDIOC_STREAMOFF,&type) == -1)
		printf("VIDIOC_STREAMOFF-end video stream failed!\n");
}

//释放rgb24申请的空间
void mw_free_v4l2_rgb_24(int fd, unsigned char *rgb24)
{
	if( mw_get_current_v4l2_pixel_format(fd) == V4L2_PIX_FMT_YUYV ){
		free(rgb24);
		rgb24 = NULL;
	}
}

void mw_clear_v4l2_buffers_munmap(int fd, int reqbuf_count, mw_v4l2_buffer_t *buffers)
{
	//手动释放分配的内存
	uint32_t current_pixel_format = -1;
	int i;
	current_pixel_format = mw_get_current_v4l2_pixel_format(fd);
	for (i = 0; i < reqbuf_count; i++)
		munmap (buffers[i].start, buffers[i].length);
}

//获取video的控制ID
mw_v4l2_control_id_t *mw_get_v4l2_control_ids(int fd,mw_v4l2_control_id_t *v4l2_control_id)
{
	struct v4l2_queryctrl  queryctrl;

	memset(&queryctrl,0,sizeof(queryctrl));
	int i = 0;
	for(i = V4L2_CID_BASE; i <= V4L2_CID_LASTP1; i++) {
		queryctrl.id = i;
		if(0 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
			if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;
			if(queryctrl.id == V4L2_CID_BRIGHTNESS)
				v4l2_control_id->brightness_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_CONTRAST)
				v4l2_control_id->contrast_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_SATURATION)
				v4l2_control_id->saturation_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_HUE)
				v4l2_control_id->hue_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_AUTO_WHITE_BALANCE)
				v4l2_control_id->white_balance_temperature_auto_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_WHITE_BALANCE_TEMPERATURE)
				v4l2_control_id->white_balance_temperature_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_GAMMA)
				v4l2_control_id->gamma_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_POWER_LINE_FREQUENCY)
				v4l2_control_id->power_line_frequency_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_SHARPNESS)
				v4l2_control_id->sharpness_id = queryctrl.id;
			if(queryctrl.id == V4L2_CID_BACKLIGHT_COMPENSATION)
				v4l2_control_id->backlight_compensation_id = queryctrl.id;
		}else {
			if(errno == EINVAL)
				continue;
			printf("VIDIOC_QUERYCTRL-query v4l2 control failed!\n");
			return v4l2_control_id;
		}
	}
	queryctrl.id = V4L2_CTRL_CLASS_CAMERA | V4L2_CTRL_FLAG_NEXT_CTRL;
	while (0 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (V4L2_CTRL_ID2CLASS (queryctrl.id) != V4L2_CTRL_CLASS_CAMERA)
			break;
		if(queryctrl.id == V4L2_CID_EXPOSURE_AUTO)
			v4l2_control_id->exposure_auto_id = queryctrl.id;
		if(queryctrl.id == V4L2_CID_EXPOSURE_ABSOLUTE)
			v4l2_control_id->exposure_absolute_id = queryctrl.id;
		queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	return v4l2_control_id;
}

int mw_get_v4l2_capability(int fd,struct v4l2_capability *cap)
{
	int ret = 0;
	memset(cap, 0, sizeof(cap));
	ret = ioctl(fd, VIDIOC_QUERYCAP, cap);
	if (ret < 0) {
		printf("get v4l2_capability failed\n");
		return -1;
	}
	return 0;
}

void mw_get_v4l2_format(int fd, struct v4l2_format *format)
{
	memset(format, 0, sizeof(format));
	format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( ioctl(fd, VIDIOC_G_FMT, format) <0)
		printf("get v4l2_format failed!\n");
}

#if 0
void mw_get_v4l2_pixel_formats(int fd)
{
	struct v4l2_fmtdesc fmtdesc;

	memset(&fmtdesc,0,sizeof(fmtdesc));
	fmtdesc.index = 0;
	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("当前设备支持的图像格式有:\n");
	while( ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1 ) {
		printf("\t%d. %s\n",fmtdesc.index,fmtdesc.description);
		fmtdesc.index++;
	}
}
#endif

int mw_set_v4l2_format(int fd, uint32_t pixelformat, int width,int height)
{
	int ret=0;
	struct v4l2_format format;

	if(width == 0 || height == 0) {
		width = mw_get_current_resolution_width(fd);
		height = mw_get_current_resolution_height(fd);
	}
	if(pixelformat == -1)
		pixelformat =  mw_get_current_v4l2_pixel_format(fd);
	memset(&format, 0, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat = pixelformat;
	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	format.fmt.pix.field = V4L2_FIELD_INTERLACED;

	ret = ioctl(fd, VIDIOC_S_FMT, &format);
	if (ret < 0) {
		printf("ioctl VIDIOC_S_FMT error\n") ;
		return -1;
	}
	return 0;
}

void mw_set_v4l2_ctrl(int fd, uint32_t id, int32_t value)
{
	struct v4l2_control v4l2ctrl;
	v4l2ctrl.id = id;
	v4l2ctrl.value = value;
	if(0 > ioctl(fd, VIDIOC_S_CTRL, &v4l2ctrl))
		printf("set v4l2 ctrl failed!\n");
}

int32_t mw_get_v4l2_ctrl(int fd, uint32_t id, struct v4l2_control *v4l2ctrl)
{
	v4l2ctrl->id = id;
	if(0 > ioctl(fd, VIDIOC_G_CTRL, v4l2ctrl)) {
		printf("get v4l2 ctrl failed!\n");
		return -1;
	}
	return v4l2ctrl->value;
}

void mw_v4l2_query_control(int fd, uint32_t id, struct v4l2_queryctrl  *queryctrl)
{
	memset(queryctrl,0,sizeof(queryctrl));
	queryctrl->id = id;
	if(0 >ioctl(fd, VIDIOC_QUERYCTRL, queryctrl))
		printf("get v4l2 query control-%d failed\n",id);
}
#if 0
float mw_get_time_used(struct timeval start,struct timeval end)
{
	float time_used = 0;
	time_used = (end.tv_usec/1000 - start.tv_usec/1000);
	time_used = end.tv_sec - start.tv_sec + time_used/1000;
	return time_used;
}
#endif
void msleep(int msec)
{
	struct timeval start;
	struct timeval end;
	float time_used = 0;
	gettimeofday(&start,NULL);
	while(time_used < msec){
		gettimeofday(&end,NULL);
		time_used = (end.tv_usec/1000 - start.tv_usec/1000);
		time_used = (end.tv_sec - start.tv_sec)*1000 + time_used;
	}
}

int mw_get_v4l2_buffer(int fd, struct v4l2_buffer *buffer)
{
	fd_set fds;
	struct timeval tv;
	int ret;
	int i = 0;

	FD_ZERO (&fds);
	FD_SET (fd, &fds);
	/* Timeout. */
	tv.tv_sec = 7;
	tv.tv_usec = 0;
	ret = select (fd + 1, &fds, NULL, NULL, &tv);
	if (1 > ret)
		return -1;

	memset(buffer, 0, sizeof(buffer));
	buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer->memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_DQBUF, buffer) == -1)
		return -1;

	//用完后放回队列中
	if (ioctl(fd, VIDIOC_QBUF, buffer) < 0) {
		perror("GetFrame VIDIOC_QBUF Failed");
		return -1;
	}
	return 0;
}

int mw_get_v4l2_frmsizeenum(int fd, int index, struct v4l2_frmsizeenum *frmsizeenum)
{
	uint32_t current_pixel_format = mw_get_current_v4l2_pixel_format(fd);
	frmsizeenum->pixel_format = current_pixel_format;
	frmsizeenum->index = index;

	return ioctl(fd, VIDIOC_ENUM_FRAMESIZES, frmsizeenum) ;
}

int mw_get_v4l2_frmsizeenum_width(int fd,struct v4l2_frmsizeenum *frmsizeenum)
{
	return frmsizeenum->discrete.width;
}

int mw_get_v4l2_frmsizeenum_height(int fd,struct v4l2_frmsizeenum *frmsizeenum)
{
	return frmsizeenum->discrete.height;
}

#if 0
//设备分辨率相关,打印所有分辨率
void mw_get_v4l2_resolutions(int fd)
{
	int i = 0;
	int width=0;
	int height=0;
	struct v4l2_frmsizeenum frmsizeenum;

	printf("当前分辨率：\n" );
	for(i = 0; ; i++) {

		if(mw_get_v4l2_frmsizeenum(fd, i, &frmsizeenum) != -1) {
			width = mw_get_v4l2_frmsizeenum_width(fd, &frmsizeenum);
			height = mw_get_v4l2_frmsizeenum_height(fd, &frmsizeenum);
			printf("\t%d. %dX%d\n", i, width, height);
		}else {
			break;
		}
	}
}

void mw_get_current_resoulation(int fd, struct v4l2_format *current_format)
{
	current_format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, current_format) != -1)
		printf("current resolution is:\n\t%dX%d\n", current_format->fmt.pix.width, current_format->fmt.pix.height);
}
#endif
