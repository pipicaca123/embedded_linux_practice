#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h> 
#include <poll.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include "show.h"

#define CAMERA_DEVICE "/dev/video0"
#define CAMERA_CAPTURE_WIDTH 1280
#define CAMERA_CAPTURE_HEIGHT 960
#define CAMERA_CAPTURE_PIX_FORMAT V4L2_PIX_FMT_YUYV  /* YUYV 4:2:2 */

#define CAMERA_BUF_REQ_NUM 10

struct CustomVideoDevice {
    int fd;
    int pixel_format;
    int width;
    int height;

    int video_buf_cnt;
    int video_buf_max_leng;
    int video_buf_cur_index;
    unsigned char *p_uv_video_buf[CAMERA_BUF_REQ_NUM];
};

struct VideoData {
    int per_pixel_bits;
    int lines_bytes;
    int total_bytes;
    int width;
    int height;
    unsigned char *pdata;
};

void device_init(struct CustomVideoDevice *vid_dev, 
                int fd,
                int pixel_format, int width, int height,
                int video_buf_cnt, int video_buf_max_leng, 
                int video_buf_cur_idx){
    vid_dev->fd = fd;
    vid_dev->width = width;
    vid_dev->height = height;
    vid_dev->video_buf_cnt = video_buf_cnt;

}
int get_v4l2_capability(int fd, struct v4l2_capability *t_v4l2cap){
    int err;
    memset(t_v4l2cap, 0, sizeof(struct v4l2_capability));
    err = ioctl(fd, VIDIOC_QUERYCAP, t_v4l2cap);
    if(err){
        printf("error opening device:%s\n", CAMERA_DEVICE);
        goto ERR_EXIT;
    }
    printf("driver=%s\ncard=%s\nbus_info=%s\ncapabilities=0x%x\n",
            (unsigned char*)t_v4l2cap->driver, 
            (unsigned char*)t_v4l2cap->card, 
            (unsigned char*)t_v4l2cap->bus_info,
            t_v4l2cap->capabilities);
    
    if(!(t_v4l2cap->capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        printf("%s isn't video capture device!\n",CAMERA_DEVICE);
        goto ERR_EXIT;
    }
    if(t_v4l2cap->capabilities & V4L2_CAP_STREAMING){
        printf("%s supports streaming I/O\n", CAMERA_DEVICE);
    }
    if(t_v4l2cap->capabilities & V4L2_CAP_READWRITE){
        printf("%s supports read/write syscall\n", CAMERA_DEVICE);
    }  
    return 0;

ERR_EXIT:
    return -1;
}
int get_support_format(int fd, struct v4l2_fmtdesc *t_fmt_desc, struct v4l2_frmsizeenum *t_frame_size){
    int err;
    memset(t_fmt_desc, 0, sizeof(struct v4l2_fmtdesc));
    t_fmt_desc->index = 0;
    t_fmt_desc->type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // for camera device

    memset(t_frame_size, 0, sizeof(struct v4l2_frmsizeenum));

    printf("\nSupported Format List:\n");
    while((err = ioctl(fd, VIDIOC_ENUM_FMT, t_fmt_desc)) == 0){
        printf("idx%d(%s), Pixel Format=0x%08x\n", t_fmt_desc->index, t_fmt_desc->description, t_fmt_desc->pixelformat);

        // printf("    Supported Resolution List:\n");
        t_frame_size->pixel_format = t_fmt_desc->pixelformat;
        while((err = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, t_frame_size)) == 0){
            // printf("    Resolution%d=%dx%d\n", t_frame_size->index, t_frame_size->discrete.width, t_frame_size->discrete.height);
            t_frame_size->index++;
        }
        t_fmt_desc->index++;
    }
    return 0;
}

int set_frame_format(int fd, struct v4l2_format *t_v4l2_format){/* smarter way, feed parameter from struct */
    int err;
    memset(t_v4l2_format, 0, sizeof(struct v4l2_format));
    t_v4l2_format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    t_v4l2_format->fmt.pix.pixelformat = CAMERA_CAPTURE_PIX_FORMAT;
    t_v4l2_format->fmt.pix.width = CAMERA_CAPTURE_WIDTH;
    t_v4l2_format->fmt.pix.height = CAMERA_CAPTURE_HEIGHT;
    t_v4l2_format->fmt.pix.field = V4L2_FIELD_ANY;

    err = ioctl(fd, VIDIOC_S_FMT, t_v4l2_format);
    if(err){
        printf("\nset camera capture format failed!\n");
        return -1;
    }
    printf("\nset camera capture format success!\n");
    return 0;
}
int buffer_request(int fd, struct v4l2_requestbuffers *t_reqbuf){
    memset(t_reqbuf, 0, sizeof(struct v4l2_requestbuffers));
    t_reqbuf->count = CAMERA_BUF_REQ_NUM;
    t_reqbuf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    t_reqbuf->memory = V4L2_MEMORY_MMAP;
    if(ioctl(fd, VIDIOC_REQBUFS, t_reqbuf)){
        printf("unable to allocate buffers.\n");
        return -1;
    }
    return 0;
}

int buffer_frame_mem_mapping(int fd, struct v4l2_buffer *t_video_buffer, 
                            struct CustomVideoDevice *custom_vid_dev){
    for(int i=0; i < custom_vid_dev->video_buf_cnt; i++){
        memset(t_video_buffer, 0, sizeof(struct v4l2_buffer));
        t_video_buffer->index = i;
        t_video_buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        t_video_buffer->memory = V4L2_MEMORY_MMAP;
        int err = ioctl(fd, VIDIOC_QUERYBUF, t_video_buffer);
        if(err){
            printf("unable to query buffer.\n");
            return -1;
        }
        /* '将内核空间的帧缓冲映射到用户空间，需要两个数据接收帧缓冲的长度和地址'
            the actual size will be twice of image size, 
            the reason is YUV4:2:2 takes 2 bytes a pixel.
        */
        custom_vid_dev->video_buf_max_leng = t_video_buffer->length;
        custom_vid_dev->p_uv_video_buf[i] = mmap(NULL/*allocated automatically*/, 
                                        t_video_buffer->length, 
                                        PROT_READ, MAP_SHARED, fd,
                                        t_video_buffer->m.offset);
        if(custom_vid_dev->p_uv_video_buf[i] == MAP_FAILED){
            printf("map buffer failed.\n");
            return -1;
        }
    }                    
    return 0;
}
int v4l2_start_camera(int fd){
    int vid_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int err = ioctl(fd, VIDIOC_STREAMON, &vid_type);
    if(err){
        printf("can't start capture video.\n");
        return -1;
    }
    return 0;
}
int v4l2_stop_camera(int fd){
    int vid_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int err = ioctl(fd, VIDIOC_STREAMOFF, &vid_type);
    if(err){
        printf("can't start capture video.\n");
        return -1;
    }
    return 0;
}
int v4l2_exit_device(struct CustomVideoDevice *vid_dev){
    for(int i=0;i<vid_dev->video_buf_cnt;i++){
        if(vid_dev->p_uv_video_buf[i]){
            munmap(vid_dev->p_uv_video_buf[i], vid_dev->video_buf_max_leng);
            vid_dev->p_uv_video_buf[i] = NULL;
        }
    }
    close(vid_dev->fd);
    return 0;
}
int v4l2_put_frame_to_streaming(struct CustomVideoDevice *vid_dev){
    struct v4l2_buffer t_v4l2_buffer;
    t_v4l2_buffer.index = vid_dev->video_buf_cur_index;
    t_v4l2_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    t_v4l2_buffer.memory = V4L2_MEMORY_MMAP;
    if(ioctl(vid_dev->fd, VIDIOC_QBUF, &t_v4l2_buffer)){
        printf("queue buffer failed.\n");
        return -1;
    }
    return 0;
}

int v4l2_init_frame_for_streaming(struct CustomVideoDevice *vid_dev, int queue_cnt){
    struct v4l2_buffer t_v4l2_buffer;
    for(int i=0;i<queue_cnt;i++){
        memset(&t_v4l2_buffer, 0, sizeof(struct v4l2_buffer));
        t_v4l2_buffer.index = i;
        t_v4l2_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        t_v4l2_buffer.memory = V4L2_MEMORY_MMAP;
        if(ioctl(vid_dev->fd, VIDIOC_QBUF, &t_v4l2_buffer)){
            printf("queue buffer failed.\n");
            return -1;
        }
    }
    return 0;
}
int v4l2_get_frame_from_streaming(struct CustomVideoDevice *vid_dev,
                                struct VideoData *vid_data){
    struct pollfd tfds[1];
    int ret;
    struct v4l2_buffer t_v4l2_buffer;

    tfds[0].fd = vid_dev->fd;
    tfds[0].events = POLLIN;
    ret = poll(tfds, 1, -1);
    if(ret <= 0){
        printf("poll failed!\n");
        return -1;
    }
    memset(&t_v4l2_buffer, 0, sizeof(struct v4l2_buffer));
    t_v4l2_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    t_v4l2_buffer.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(vid_dev->fd, VIDIOC_DQBUF, &t_v4l2_buffer); // FIXME: buffer here?
    if(ret < 0){
        printf("dequeue buffer failed!\n");
        return -1;
    }
    vid_dev->video_buf_cur_index = t_v4l2_buffer.index;

    vid_data->per_pixel_bits = vid_dev->pixel_format==V4L2_PIX_FMT_YUYV?16:0;// TODO: extension
    vid_data->lines_bytes = vid_dev->width * vid_data->per_pixel_bits / 8;
    vid_data->total_bytes = t_v4l2_buffer.bytesused;
    vid_data->pdata = vid_dev->p_uv_video_buf[vid_dev->video_buf_cur_index];

    vid_data->width = vid_dev->width;
    vid_data->height = vid_dev->height;
    return 0;
}

int main(int argc,char* argv[]) {
    int rv;
    int video_fd = open(CAMERA_DEVICE, O_RDWR);
    if(video_fd < 0){
        printf("can't open DEV:%s\n", CAMERA_DEVICE);
        return -1;
    }
    struct v4l2_capability t_v4l2cap; // 查询设备属性
    struct v4l2_fmtdesc t_fmt_desc; // 显示所有支持的格式 
    struct v4l2_frmsizeenum t_frm_size;
    struct v4l2_format t_v4l2_format; // 设置图像帧格式
    struct v4l2_requestbuffers t_v4l2_req_buffers; // 申请缓冲区
    struct v4l2_buffer t_v4l2_buffer;

    struct CustomVideoDevice video_dev;
    struct VideoData video_data;

    device_init(&video_dev, video_fd, CAMERA_CAPTURE_PIX_FORMAT,
                CAMERA_CAPTURE_WIDTH,CAMERA_CAPTURE_HEIGHT, 
                CAMERA_BUF_REQ_NUM, 0, 0);
    rv = get_v4l2_capability(video_fd, &t_v4l2cap);
    if(rv < 0){
        return -1;
    }
    rv = get_support_format(video_fd, &t_fmt_desc, &t_frm_size);
    if(rv < 0){
        return -1;
    }
    rv = set_frame_format(video_fd, &t_v4l2_format);
    if(rv < 0){
        return -1;
    }
    rv = buffer_request(video_fd, &t_v4l2_req_buffers);
    if(rv < 0){
        return -1;
    }
    rv = buffer_frame_mem_mapping(video_fd, &t_v4l2_buffer, &video_dev);
    if(rv < 0){
        return -1;
    }
    v4l2_init_frame_for_streaming(&video_dev, CAMERA_BUF_REQ_NUM);
    v4l2_start_camera(video_fd);
    if(rv < 0){
        return -1;
    }
    v4l2_get_frame_from_streaming(&video_dev, &video_data);
    v4l2_put_frame_to_streaming(&video_dev);
    v4l2_stop_camera(video_fd);
    v4l2_exit_device(&video_dev);

}   