#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>
#include <mw_base/base.h>

typedef struct _mw_v4l2_buffer{
    void *start;
    uint32_t length;
} mw_v4l2_buffer_t;

//用户控制项ID
typedef struct _mw_v4l2_control_id {
uint32_t brightness_id;    					//亮度
uint32_t contrast_id;  						//对比度
uint32_t saturation_id;    					//饱和度
uint32_t hue_id;   							//色调
uint32_t white_balance_temperature_auto_id; //白平衡色温（自动）
uint32_t white_balance_temperature_id; 		//白平衡色温
uint32_t gamma_id; 							//伽马
uint32_t power_line_frequency_id;  			//电力线频率
uint32_t sharpness_id; 						//锐度，清晰度
uint32_t backlight_compensation_id;    		//背光补偿
//扩展摄像头项ID
uint32_t exposure_auto_id;  				//自动曝光
uint32_t exposure_absolute_id;
} mw_v4l2_control_id_t;

#define V4L2_CTRL_TEST
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#define MW_REQBUF_COUNT 4


void start_video_stream(int fd);
void end_video_stream(int fd);
void mw_clear_v4l2_buffers_munmap(int fd, mw_v4l2_buffer_t *buffers,unsigned char * rgb24);
int mw_get_current_resolution_width(int fd);
int mw_get_current_resolution_height(int fd);
uint32_t mw_get_current_v4l2_pixel_format(int fd);
void mw_get_v4l2_format(int fd, struct v4l2_format *format);
void mw_get_v4l2_resolutions(int fd);

#ifdef V4L2_CTRL_TEST
static int get_user_choice(){
    int index;
    scanf("%d",&index);
    return index;
}
#endif
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

static int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
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

#ifdef V4L2_CTRL_TEST
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
        if(mw_test_device_exist(dev_name) == 0){
            fd = open(dev_name, O_RDWR | O_NONBLOCK);
            if(fd < 0){
                printf("open %s failed!\n",dev_name);
            }
        }else{
            printf("%s is not exist!\n",dev_name);

        }
        return fd;
}

mw_v4l2_buffer_t *mw_get_v4l2_buffers_mmap(int fd)
{
	mw_v4l2_buffer_t *mmap_buffers = NULL;
	struct v4l2_buffer buffer;
	struct v4l2_requestbuffers reqbuf;

	//申请帧缓存区
	memset (&reqbuf, 0, sizeof (reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = MW_REQBUF_COUNT;

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
	for(i = 0; i < MW_REQBUF_COUNT; i++){
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;
		if (ioctl(fd,VIDIOC_QBUF,&buffer)==-1){
			printf("111111111\n");
			perror("VIDIOC_QBUF failed");
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
    if (ioctl(fd,VIDIOC_STREAMON,&type) == -1) {
        perror("VIDIOC_STREAMON failed");
    }
}

void end_video_stream(int fd)
{
    //关闭视频流
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd,VIDIOC_STREAMOFF,&type) == -1) {
        perror("VIDIOC_STREAMOFF failed");
    }
}

void mw_clear_v4l2_buffers_munmap(int fd, mw_v4l2_buffer_t *buffers, unsigned char *rgb24)
{
    //手动释放分配的内存
	uint32_t current_pixel_format = -1;
    int i;
    current_pixel_format = mw_get_current_v4l2_pixel_format(fd);
    for (i = 0; i < MW_REQBUF_COUNT; i++)
        munmap (buffers[i].start, buffers[i].length);
    if(current_pixel_format == V4L2_PIX_FMT_YUYV){
    	free(rgb24);
    	rgb24 = NULL;
    }
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
		}
		else {
			if(errno == EINVAL)
				continue;
			perror("VIDIOC_QUERYCTRL");
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
		fprintf(stderr, "ioctl querycap error\n");
		return -1;
	}
	else {
		printf("Capability Informations:\n");
		printf("\t%-15s = %s\n", "cap.driver", cap->driver);
		printf("\t%-15s = %s\n", "cap.card", cap->card);
		printf("\t%-15s = %s\n", "cap.bus_info", cap->bus_info);
		printf("\t%-15s = 0x%x\n", "cap.version", cap->version);
		printf("\t%-15s = 0x%x\n", "cap.capabilities", cap->capabilities);
		printf("\t%-15s = 0x%x\n", "cap.device_caps", cap->device_caps);
	}
	return 0;
}

void mw_get_v4l2_format(int fd, struct v4l2_format *format)
{
	char fmtstr[8];

	memset(fmtstr, 0, 8);
	memset(format, 0, sizeof(format));
	format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if( ioctl(fd, VIDIOC_G_FMT, format) <0)
		fprintf(stderr, "get format failed\n");

	memcpy(fmtstr, &format->fmt.pix.pixelformat, 4);
	printf("当前video参数信息:\n");
	printf("\t%-15s= %s\n", "pixelformat", fmtstr);
	printf("\t%-15s= %d\n", "type", format->type);
	printf("\t%-15s= %d\n", "width", format->fmt.pix.width);
	printf("\t%-15s= %d\n", "height", format->fmt.pix.height);
	printf("\t%-15s= %d\n", "field", format->fmt.pix.field);
	printf("\t%-15s= %d\n", "bytesperline", format->fmt.pix.bytesperline);
	printf("\t%-15s= %d\n", "sizeimage", format->fmt.pix.sizeimage);
	printf("\t%-15s= %d\n", "colorspace", format->fmt.pix.colorspace);
}

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
	format.fmt.pix.field       = V4L2_FIELD_INTERLACED;

	ret = ioctl(fd, VIDIOC_S_FMT, &format);
	if (ret < 0) {
		fprintf(stderr, "ioctl VIDIOC_S_FMT error\n") ;
		return -1;
	}
	//mw_get_v4l2_format(fd,format);
	return 0;
}

void mw_set_v4l2_ctrl(int fd, uint32_t id, int32_t value)
{
	struct v4l2_control v4l2ctrl;
	v4l2ctrl.id = id;
	v4l2ctrl.value = value;
	if(0 > ioctl(fd, VIDIOC_S_CTRL, &v4l2ctrl)) {
		printf("set v4l2 ctrl failed!\n");
	}
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
		printf("获取video控制信息-%d 失败\n",id);
}

#ifdef V4L2_CTRL_TEST
void mw_print_v4l2_control_config_ids(mw_v4l2_control_id_t *v4l2_control_id){
	printf("##############video 控制信息##############\n");
	printf("\t%s(%s) = %d\n","backlight_compensation_id", "背光补偿", v4l2_control_id->backlight_compensation_id);
	printf("\t%s(%s) = %d\n", "brightness_id","亮度",v4l2_control_id->brightness_id);
	printf("\t%s(%s) = %d\n","contrast_id" ,"对比度",v4l2_control_id->contrast_id);
	printf("\t%s(%s) = %d\n","exposure_absolute_id" ,"曝光绝对值", v4l2_control_id->exposure_absolute_id);
	printf("\t%s(%s) = %d\n","exposure_auto_id" ,"自动曝光", v4l2_control_id->exposure_auto_id);
	printf("\t%s(%s) = %d\n","gamma_id" ,"伽马", v4l2_control_id->gamma_id);
	printf("\t%s(%s) = %d\n","hue_id" ,"色调", v4l2_control_id->hue_id);
	printf("\t%s(%s) = %d\n","power_line_frequency_id" ,"电力线频率", v4l2_control_id->power_line_frequency_id);
	printf("\t%s(%s) = %d\n","saturation_id" ,"饱和度", v4l2_control_id->saturation_id);
	printf("\t%s(%s) = %d\n", "sharpness_id","锐度/清晰度", v4l2_control_id->sharpness_id);
	printf("\t%s(%s) = %d\n","white_balance_temperature_auto_id" ,"白平衡色温(自动)", v4l2_control_id->white_balance_temperature_auto_id);
	printf("\t%s(%s) = %d\n","white_balance_temperature_id" ,"白平衡色温", v4l2_control_id->white_balance_temperature_id);
	printf("#####################################\n");
}

void mw_print_v4l2_control_config(int fd, struct v4l2_control *v4l2ctrl, mw_v4l2_control_id_t *v4l2_control_id)
{
	printf("Camera Control Config Informations:\n");
	printf("\t%-15s = %u\n", "背光补偿", mw_get_v4l2_ctrl(fd,v4l2_control_id->backlight_compensation_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "亮度", mw_get_v4l2_ctrl(fd,v4l2_control_id->brightness_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "对比度", mw_get_v4l2_ctrl(fd,v4l2_control_id->contrast_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "曝光绝对值", mw_get_v4l2_ctrl(fd,v4l2_control_id->exposure_absolute_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "自动曝光", mw_get_v4l2_ctrl(fd,v4l2_control_id->exposure_auto_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "伽马", mw_get_v4l2_ctrl(fd,v4l2_control_id->gamma_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "色调", mw_get_v4l2_ctrl(fd,v4l2_control_id->hue_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "电力线频率", mw_get_v4l2_ctrl(fd,v4l2_control_id->power_line_frequency_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "饱和度", mw_get_v4l2_ctrl(fd,v4l2_control_id->saturation_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "锐度/清晰度", mw_get_v4l2_ctrl(fd,v4l2_control_id->sharpness_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "白平衡色温(自动)", mw_get_v4l2_ctrl(fd,v4l2_control_id->white_balance_temperature_auto_id, v4l2ctrl));
	printf("\t%-15s = %u\n", "白平衡色温", mw_get_v4l2_ctrl(fd,v4l2_control_id->white_balance_temperature_id, v4l2ctrl));
}

void mw_change_v4l2_control_config(int fd, struct v4l2_control *v4l2ctrl, mw_v4l2_control_id_t *v4l2_control_id)
{
	uint32_t control_id = -1;
	uint32_t control_value = -1;
	struct v4l2_queryctrl  queryctrl;

	mw_print_v4l2_control_config_ids(v4l2_control_id);
	printf("请输入需要更改的配置ID：\n");
	control_id = get_user_choice();

	mw_v4l2_query_control(fd,control_id, &queryctrl);
	printf("default value:%d max value:%d  min:%d\n",queryctrl.default_value,queryctrl.maximum,queryctrl.minimum);
	printf("请输入需要更改的值:\n");
	control_value = get_user_choice();
	mw_set_v4l2_ctrl(fd,control_id,control_value);

	printf("结果:\n%d = %d\n",control_id, mw_get_v4l2_ctrl(fd,control_id, v4l2ctrl));
}

float mw_get_time_used(struct timeval start,struct timeval end)
{
	float time_used = 0;
	time_used = (end.tv_usec/1000 - start.tv_usec/1000);
	time_used = end.tv_sec - start.tv_sec + time_used/1000;
	return time_used;
}
#endif
#if 1
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
#endif

int mw_get_buffers_start(int fd, struct v4l2_buffer *buffer)
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
	if (ioctl(fd, VIDIOC_DQBUF, buffer) == -1) {
		return -1;
	}

	//用完后放回队列中
	if (ioctl(fd, VIDIOC_QBUF, buffer) < 0) {
		perror("GetFrame VIDIOC_QBUF Failed");
		return -1;
	}
	return 0;
}

int mw_get_frames_test(int fd, unsigned char * rgb24,const mw_v4l2_buffer_t *buffers)
{
	struct v4l2_buffer buffer;
	uint32_t current_pixel_format = -1;
	int width = 0;
	int height = 0;
	FILE *file_fd = NULL;


	current_pixel_format = mw_get_current_v4l2_pixel_format(fd);
	width = mw_get_current_resolution_width(fd);
	height = mw_get_current_resolution_height(fd);
	printf("wid:%d,height:%d\n",width,height);

	file_fd = fopen("frame.mp4", "a+");
	if (file_fd < 0)
		printf("create file failed\n");

	printf("开始采集\n");
	for(;;){
		mw_get_buffers_start(fd,&buffer);
		//根据图像格式进行相应测试
		switch (current_pixel_format) {
		case V4L2_PIX_FMT_YUYV:
			convert_yuv_to_rgb_buffer((unsigned char*)buffers[buffer.index].start, rgb24, width, height);
			msleep(1000/30);
			break;
		case V4L2_PIX_FMT_MJPEG:
			fwrite((unsigned char*)buffers[buffer.index].start, 1, buffers[buffer.index].length, file_fd);
			msleep(1000/30);
			break;
		default:
			break;
		}
	}
	fclose(file_fd);
	return 0;
}

#ifdef V4L2_CTRL_TEST
uint32_t choose_v4l2_pixel_format(int fd)
{
	int pixformat_index = 0;
	uint32_t current_pixel_format = -1;
	struct v4l2_fmtdesc fmtdesc;

	mw_get_v4l2_pixel_formats(fd);

	printf("请输入图像格式的序号:\n");
	pixformat_index = get_user_choice();

	memset(&fmtdesc,0,sizeof(fmtdesc));
	fmtdesc.index = pixformat_index;
	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
		printf("IO VIDIOC_ENUM_FMT failed.\n");
	current_pixel_format = fmtdesc.pixelformat;
	if(current_pixel_format == -1)
		current_pixel_format = mw_get_current_v4l2_pixel_format(fd);
	return current_pixel_format;
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
#endif

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

uint32_t mw_get_current_v4l2_pixel_format(int fd)
{
	uint32_t current_pixel_format = -1;
	struct v4l2_format format;

	memset(&format, 0, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( ioctl(fd, VIDIOC_G_FMT, &format) != -1) {
		current_pixel_format= format.fmt.pix.pixelformat;
	}
	else {
		current_pixel_format= -1;
	}
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

void mw_get_current_resoulation(int fd, struct v4l2_format *current_format)
{
	current_format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, current_format) != -1)
		printf("current resolution is:\n\t%dX%d\n", current_format->fmt.pix.width, current_format->fmt.pix.height);
}

#ifdef V4L2_CTRL_TEST
static void usage(FILE *fp, int argc, char **argv)
{
	fprintf(fp,
		"Usage: %s [options]\n\n"
		"Version 1.0\n"
		"Options:\n"
		"-c | --config			show current video's control config\n"
		"-C | --control			control video's config\n"
		"-d | --device name		open device,default is video0.eg:%s -d \"/dev/video0\"\n"
		"-s | --show			display current video's normal info\n"
		"-h | --help			print this message\n"
		"-R | --resolution		set resolution\n"
		"-P | --pixel-format	set pixel format\n"
		"",
		argv[0],argv[0]);
}
static const char short_options[] = "cCd:hPRs";
static const struct option long_options[] = {
	{ "config", no_argument, NULL, 'c' },
	{ "control", no_argument, NULL, 'C' },
	{ "device", required_argument, NULL, 'd' },
	{ "show",   no_argument,       NULL, 's' },
	{ "help",   no_argument,       NULL, 'h' },
	{ "pixel-format",  no_argument, NULL, 'P' },
	{ "resolution",  no_argument, NULL, 'R' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char **argv)
{
	int fd = 0;
	uint32_t ret = 0;
	int is_p_format = 0;
	int is_resolution = 0;
	int is_show_info = 0;
	int is_config = 0;
	int is_control = 0;

	int current_width = 0;
	int current_height = 0;
	uint32_t choose_pixel_format = -1;
	mw_v4l2_buffer_t *buffers = NULL;
	unsigned char *rgb24 = NULL;

	mw_v4l2_control_id_t v4l2_control_ids;
	//struct v4l2_requestbuffers reqbuf;
	struct v4l2_capability cap;
	struct v4l2_format current_format;
	struct v4l2_frmsizeenum frmsizeenum;
	struct v4l2_control v4l2ctrl;
	char * dev_name = "/dev/video0";

	//mw_v4l2_buffer_t *buffers;

	for(;;) {
		int idx,c;
		c = getopt_long(argc, argv, short_options, long_options, &idx);
		if(-1 == c)
			break;
		switch(c) {
		case 0:
			break;
		case 'c':
			is_config ++;
			break;
		case 'C':
			is_control ++;
			break;
		case 'd':
			dev_name = optarg;
			break;
		case 'h':
			usage(stdout, argc, argv);
			exit(EXIT_SUCCESS);
			break;
		case 's':
			is_show_info ++;
			break;
		case 'P':
			is_p_format ++;
			break;
		case 'R':
			is_resolution ++;
			break;
		default:
			break;
		}
	}
	fd = mw_open_device(dev_name);
	mw_get_v4l2_control_ids(fd,&v4l2_control_ids);

	//设置图像格式
	if (is_p_format) {
		choose_pixel_format = choose_v4l2_pixel_format(fd);
	}
	//设置分辨率
	if(is_resolution) {
		mw_get_current_resoulation(fd, &current_format);
		int resolution_index = 0;
		mw_get_v4l2_resolutions(fd);
		printf("请输入分辨率的序号:\n");
		resolution_index = get_user_choice();
		mw_get_v4l2_frmsizeenum(fd, resolution_index, &frmsizeenum);
		current_width = mw_get_v4l2_frmsizeenum_width(fd,&frmsizeenum);
		current_height = mw_get_v4l2_frmsizeenum_height(fd,&frmsizeenum);
	}
	if(is_resolution || is_p_format ) {
		mw_set_v4l2_format(fd,choose_pixel_format,current_width,current_height);
		is_p_format = 0;
		is_resolution = 0;
	}

	if(is_config) {
		mw_print_v4l2_control_config(fd, &v4l2ctrl, &v4l2_control_ids);
		is_config = 0;
	}

	if(is_control) {
		mw_change_v4l2_control_config(fd, &v4l2ctrl, &v4l2_control_ids);
		is_control = 0;
	}

	if(is_show_info) {
		mw_get_v4l2_names();
		//获取video的属性值
		mw_get_v4l2_capability(fd,&cap);
		//获取video的图像格式
		mw_get_v4l2_pixel_formats(fd);
		//获取video的分辨率
		mw_get_v4l2_resolutions(fd);
		//获取video当前的配置
		mw_get_v4l2_format(fd,&current_format);
		//video当前的控制配置参数
		mw_print_v4l2_control_config(fd, &v4l2ctrl, &v4l2_control_ids);
		is_show_info =0;
	}

	buffers = mw_get_v4l2_buffers_mmap(fd);
	rgb24 = mw_init_grb_24(fd);

	start_video_stream(fd);
	mw_get_frames_test(fd,rgb24, buffers);
	end_video_stream(fd);
	mw_clear_v4l2_buffers_munmap(fd,buffers,rgb24);

	close(fd);
	return 0;
}
#endif
