#pragma once

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
uint32_t brightness_id;    //亮度
uint32_t contrast_id;  //对比度
uint32_t saturation_id;    //饱和度
uint32_t hue_id;   //色调
uint32_t white_balance_temperature_auto_id; //白平衡色温（自动）
uint32_t white_balance_temperature_id; //白平衡色温
uint32_t gamma_id; //伽马
uint32_t power_line_frequency_id;  //电力线频率
uint32_t sharpness_id; //锐度，清晰度
uint32_t backlight_compensation_id;    //背光补偿
//扩展摄像头项ID
uint32_t exposure_auto_id;  //自动曝光
uint32_t exposure_absolute_id;
} mw_v4l2_control_id_t;

int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height);
int mw_test_device_exist(const char *devName);
char *mw_get_v4l2_name(const char * dev_name,char *camera_name);
int mw_open_device(const char* dev_name);
uint32_t mw_get_current_v4l2_pixel_format(int fd);
int mw_get_current_resolution_width(int fd);
int mw_get_current_resolution_height(int fd);
mw_v4l2_buffer_t *mw_init_v4l2_buffers_mmap(int fd, int reqbuf_count);
unsigned char *mw_init_grb_24(int fd);
void start_video_stream(int fd);
void end_video_stream(int fd);
void mw_free_v4l2_rgb_24(int fd, unsigned char *rgb24);
void mw_clear_v4l2_buffers_munmap(int fd, int reqbuf_count, mw_v4l2_buffer_t *buffers);
mw_v4l2_control_id_t *mw_get_v4l2_control_ids(int fd,mw_v4l2_control_id_t *v4l2_control_id);
int mw_get_v4l2_capability(int fd,struct v4l2_capability *cap);
void mw_get_v4l2_format(int fd, struct v4l2_format *format);
int mw_set_v4l2_format(int fd, uint32_t pixelformat, int width,int height);
void mw_set_v4l2_ctrl(int fd, uint32_t id, int32_t value);
int32_t mw_get_v4l2_ctrl(int fd, uint32_t id, struct v4l2_control *v4l2ctrl);
void mw_v4l2_query_control(int fd, uint32_t id, struct v4l2_queryctrl  *queryctrl);
int mw_get_v4l2_buffer(int fd, struct v4l2_buffer *buffer);
int mw_get_v4l2_frmsizeenum(int fd, int index, struct v4l2_frmsizeenum *frmsizeenum);
int mw_get_v4l2_frmsizeenum_width(int fd,struct v4l2_frmsizeenum *frmsizeenum);
int mw_get_v4l2_frmsizeenum_height(int fd,struct v4l2_frmsizeenum *frmsizeenum);
