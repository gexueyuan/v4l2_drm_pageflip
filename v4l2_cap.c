/*
 * v4l2_capture_example.c
 * V4L2 video capture example
 * AUTHOT : WANGTISHENG
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "drm_fourcc.h"
#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer {
        void *start;
        size_t length;
};

static char *dev_name;
static int fd = -1;        //DEVICE NUMBER
struct buffer *buffers;
static unsigned int n_buffers ;
static int frame_count = 2;
FILE *fp;                                //FILE POINTOR
struct v4l2_plane planes[1];

int src_width = 1280;
int src_height = 720;

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint32_t *vaddr;
	uint32_t fb_id;
};

struct buffer_object buf[2];
static int terminate;

struct bo
{
	int fd;
	void *ptr;
	size_t size;
	size_t offset;
	size_t pitch;
	unsigned handle;
};

int fd_disp;
drmEventContext ev = {};
drmModeConnector *conn;
drmModeRes *res;
uint32_t conn_id;
uint32_t crtc_id;


long long start_time,end_time,time_diff;

void print_ms_time(void)
{
    struct timeval te;
    gettimeofday(&te, NULL); // 获取当前时间
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // 毫秒
    printf("time: %lld ms\n",milliseconds);
}

void test_timestamp_printf(void)
{
        printf("\ntest print timestamp 5ms start:");
        print_ms_time();
        usleep(5000);
        printf("\ntest print timestamp 5ms end:");
        print_ms_time();
}
/**************************libdrm*************************************/

static int modeset_create_fb(int fd, struct buffer_object *bo, uint32_t color)
{
	struct drm_mode_create_dumb create = {};
 	struct drm_mode_map_dumb map = {};
	uint32_t i;

	create.width = bo->width;
	create.height = bo->height;
	create.bpp = 32;
	drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;
	drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch,
			   bo->handle, &bo->fb_id);

	map.handle = create.handle;
	drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

	bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, map.offset);

	for (i = 0; i < (bo->size / 4); i++)
		bo->vaddr[i] = color;

	return 0;
}
static int modeset_create_fb_cpisp(int fd, struct buffer_object *bo, uint32_t color)
{
	struct drm_mode_create_dumb create = {};
 	struct drm_mode_map_dumb map = {};
	uint32_t i;
        uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};

	create.width = bo->width;
	create.height = bo->height;
	create.bpp = 16;//yuyv 
	drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;
        handles[0] = create.handle;
        pitches[0] = create.pitch;

	//drmModeAddFB(fd, bo->width, bo->height, 24, 16, bo->pitch,
			  // bo->handle, &bo->fb_id);

        drmModeAddFB2WithModifiers(fd, bo->width, bo->height,
			    DRM_FORMAT_YUYV, handles, pitches, offsets, NULL, &bo->fb_id, 1);                   

	map.handle = create.handle;
	drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

	bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, map.offset);

	for (i = 0; i < (bo->size / 4); i++)
				bo->vaddr[i] = color;
	return 0;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = {};

	drmModeRmFB(fd, bo->fb_id);

	munmap(bo->vaddr, bo->size);

	destroy.handle = bo->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static void modeset_page_flip_handler(int fd, uint32_t frame,
				    uint32_t sec, uint32_t usec,
				    void *data)
{
	static int i = 0;
	uint32_t crtc_id = *(uint32_t *)data;

	i ^= 1;
//printf("modeset_page_flip_handler buff:%d\n",i);
	drmModePageFlip(fd, crtc_id, buf[i].fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, data);

	//usleep(500000);

}

static void sigint_handler(int arg)
{
	terminate = 1;
}

/**************************libdrm*************************************/


static void errno_exit(const char *s)
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
        int r;
        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);
        return r;
}
//处理函数
static void process_image(const void *p, int size)
{
        fwrite(p,size, 1, fp);



}
static void process_image_2(const void *p, int size,struct buffer_object *bo)
{
        memcpy(bo->vaddr,p,size);
      /* 
              static int k = 0;
	      int i = 0;
             
        for (i = 0; i < (bo->size / 4); i++){
  		if(k == 0)
	                        bo->vaddr[i] = 0xff00ff;
                else
	                         bo->vaddr[i] = 0x00ffff;       
        }

        k ^=1;	

*/
//	printf("bo->size:%d,image size:%d\n",bo->size,size);

}

static int read_frame(FILE *fp)
{
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
		buf.length = 1;
		buf.m.planes = planes;
        
        if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) 
                errno_exit("VIDIOC_DQBUF");
        
		printf("bus.index is %d,length is %d bytesused is %d\n",buf.index,buf.length,buf.bytesused);
        process_image(buffers[buf.index].start, buf.m.planes[0].length);

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF"); 

        return 1;
}

static int read_frame_to_fb(struct buffer_object *bo)
{
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
		buf.length = 1;
		buf.m.planes = planes;
        
        if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) 
                errno_exit("VIDIOC_DQBUF");
        
	//	printf("bus.index is %d,length is %d bytesused is %d\n",buf.index,buf.length,buf.bytesused);
        process_image_2(buffers[buf.index].start, buf.m.planes[0].length,bo);
        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF"); 

        return 1;
}

static void mainloop(void)
{
        unsigned int count;
        count = frame_count;
        while (count-- >0) {
            printf("No.%d\n",frame_count - count);        //显示当前帧数目
            read_frame(fp);
        }
        printf("\nREAD AND SAVE DONE!\n");
}

static void mainloop_2(void)
{
	int fd;
	drmEventContext ev = {};
	drmModeConnector *conn;
	drmModeRes *res;
	uint32_t conn_id;
	uint32_t crtc_id;
        static int j = 0;
        struct bo *bo;
        uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};

	signal(SIGINT, sigint_handler);

	ev.version = DRM_EVENT_CONTEXT_VERSION;
	ev.page_flip_handler = modeset_page_flip_handler;

	fd = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);

	res = drmModeGetResources(fd);
	crtc_id = res->crtcs[0];
	conn_id = res->connectors[0];

	conn = drmModeGetConnector(fd, conn_id);
	buf[0].width = conn->modes[0].hdisplay;
	buf[0].height = conn->modes[0].vdisplay;
	buf[1].width = conn->modes[0].hdisplay;
	buf[1].height = conn->modes[0].vdisplay;
	printf("buf init \n");
	usleep(500000);

	modeset_create_fb_cpisp(fd, &buf[0],0xff0000);
	modeset_create_fb_cpisp(fd, &buf[1],0x0000ff);


	printf("modeset_create_fb_cpisp \n");
/*
        bo = bo_create(fd, pipes[0].fourcc, dev->mode.width,
		       dev->mode.height, handles, pitches, offsets);

	if (bo == NULL)
		return;

	ret = drmModeAddFB2WithModifiers(fd, dev->mode.width, dev->mode.height,
			    pipes[0].fourcc, handles, pitches, offsets, modifiers, &fb_id, NULL);
*/
	//usleep(500000);

	printf("begin  read_frame_to_fb \n");
        read_frame_to_fb(&buf[0]);
	//usleep(500000);

	printf("end  read_frame_to_fb \n");
	drmModeSetCrtc(fd, crtc_id, buf[0].fb_id,
			0, 0, &conn_id, 1, &conn->modes[0]);

	drmModePageFlip(fd, crtc_id, buf[0].fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, &crtc_id);
	//usleep(500000);
        read_frame_to_fb(&buf[1]);

	while (!terminate) {
		drmHandleEvent(fd, &ev);
                read_frame_to_fb(&buf[j]);
                j ^= 1;

	}

	modeset_destroy_fb(fd, &buf[1]);
	modeset_destroy_fb(fd, &buf[0]);

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);

	close(fd);
}


void init_display(void)
{

	int fd;
        struct bo *bo;
        uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};

	fd_disp = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);

	res = drmModeGetResources(fd_disp);
	crtc_id = res->crtcs[0];
	conn_id = res->connectors[0];

	conn = drmModeGetConnector(fd_disp, conn_id);
	buf[0].width = conn->modes[0].hdisplay;
	buf[0].height = conn->modes[0].vdisplay;
	buf[1].width = conn->modes[0].hdisplay;
	buf[1].height = conn->modes[0].vdisplay;
	printf("buf init \n");
	

	modeset_create_fb_cpisp(fd_disp, &buf[0],0xff0000);
	modeset_create_fb_cpisp(fd_disp, &buf[1],0x0000ff);


	printf("modeset_create_fb_cpisp finished \n");



        printf("init drm pipline \n");

	drmModeSetCrtc(fd_disp, crtc_id, buf[0].fb_id,
			0, 0, &conn_id, 1, &conn->modes[0]);

	//drmModePageFlip(fd_disp, crtc_id, buf[0].fb_id,
	//		DRM_MODE_PAGE_FLIP_EVENT, &crtc_id);
	//usleep(500000);


}

void start_display(void)
{

        static int m = 0;
        printf("start_display ");
        print_ms_time();
	signal(SIGINT, sigint_handler);

	ev.version = DRM_EVENT_CONTEXT_VERSION;
	ev.page_flip_handler = modeset_page_flip_handler;

        printf("drmModePageFlip start ");
        print_ms_time();

	drmModePageFlip(fd_disp, crtc_id, buf[0].fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, &crtc_id);

        printf("drmModePageFlip end ");
        print_ms_time();

	while (!terminate) {
		//printf("wait for buf[%d] \n",m);
		drmHandleEvent(fd_disp, &ev);
                //printf("flip done ");                       
                //print_ms_time();
                read_frame_to_fb(&buf[m]);
		//printf("cpy to buf[%d] \n",m);
                m ^= 1;

	}

	modeset_destroy_fb(fd_disp, &buf[1]);
	modeset_destroy_fb(fd_disp, &buf[0]);

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);

	close(fd_disp);


}


static void stop_capturing(void)
{
        enum v4l2_buf_type type;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
            errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(void)
{
        unsigned int i;
        enum v4l2_buf_type type;

        for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
				buf.length = 1;
				buf.m.planes = planes;

                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        printf("start_capturing start:");
        print_ms_time();
        if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                errno_exit("VIDIOC_STREAMON");
        printf("start_capturing end:");        
        print_ms_time();        
}

static void uninit_device(void)
{
        unsigned int i;

        for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap(buffers[i].start, buffers[i].length))
                        errno_exit("munmap");

        free(buffers);
}



static void init_mmap(void)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count = 2;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support memory mapping\n", 
                            dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) {
                fprintf(stderr, "Insufficient buffer memory on %s\n",
                         dev_name);
                exit(EXIT_FAILURE);
        }

        buffers = calloc(req.count, sizeof(*buffers));

        if (!buffers) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;

                CLEAR(buf);

				memset(&buf, 0, sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = n_buffers;
				buf.length = 1;
				buf.m.planes = planes;

                if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit("VIDIOC_QUERYBUF");

				printf("plane's length is %d\n",buf.m.planes[0].length);
				buf.length = 
                buffers[n_buffers].length = buf.m.planes[0].length;
                buffers[n_buffers].start =
                        mmap(NULL /* start anywhere */,
                              buf.m.planes[0].length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.planes[0].m.mem_offset);

                if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit("mmap");
        }
}


static void init_device(void)
{
        struct v4l2_capability cap; //查询设备功能
        struct v4l2_format fmt;//帧格式
        struct v4l2_fmtdesc fmtdesc;

        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {        //测试参数
                if (EINVAL == errno) {
                        fprintf(stderr, "%s is no V4L2 device\n",
                                 dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_QUERYCAP");
                }
        }
        printf("driver name is %s, card:%s buf_info:%s \n",cap.driver,cap.card,cap.bus_info);
        CLEAR(fmtdesc);
        //query the supported fmt
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmtdesc.index = 0;

        while(xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
        {
            printf("%d.%s\t",fmtdesc.index+1,fmtdesc.description);
            printf("%c%c%c%c\n",fmtdesc.pixelformat&0xFF,
                    (fmtdesc.pixelformat>>8) & 0xFF,
                    (fmtdesc.pixelformat>>16) & 0xFF,
                    (fmtdesc.pixelformat>>24) & 0xFF);
            fmtdesc.index++;
        }
        CLEAR(fmt); 
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.width = 1280;
        fmt.fmt.pix_mp.height = 720;
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;

        if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))        //设置格式
                errno_exit("VIDIOC_S_FMT");        
        init_mmap();
}

static void close_device(void)
{
        if (-1 == close(fd))
                errno_exit("close");

        fd = -1;
}

static void open_device(void)
{
        //加上O_NONBLOCK会出现如下错误
        //VIDIOC_DQBUF error 11, Resource temporarily unavailable
        fd = open(dev_name, O_RDWR /* required */ /*| O_NONBLOCK*/, 0);

        if (-1 == fd) {
                fprintf(stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(1);
        }
}
static int  get_inputcam_cap(void)
{
    char cam_name[] = "/dev/v4l-subdev3";
    int cam_fd = 0;
    struct v4l2_subdev_frame_size_enum fmt_size;
    struct v4l2_subdev_frame_interval_enum fie;
    struct v4l2_subdev_frame_interval s_fi;
    struct v4l2_subdev_format s_fmt;

    cam_fd = open(cam_name, O_RDWR, 0);
    if(-1 == cam_fd)
    {
        fprintf(stderr, "Cannot open '%s':%d, %s\n",
            cam_name, errno, strerror(errno));
        exit(1);
    }
    CLEAR(fmt_size);
    fmt_size.index = 0;
    fmt_size.pad = 0;

    while(xioctl(cam_fd, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fmt_size) == 0)
    {
        fmt_size.index++;
        printf("%d: min_width:%d max_width:%d min_height:%d max_height:%d \n",
            fmt_size.index,fmt_size.min_width,fmt_size.max_width,fmt_size.min_height,fmt_size.max_height);
        CLEAR(fie);
        fie.index = 0;
        fie.height = fmt_size.max_height;
        fie.width  = fmt_size.max_width;
        while(xioctl(cam_fd, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) == 0)
        {
            fie.index++;
            printf("%d: %dx%d@%d supported\n",fie.index, fie.width,fie.height,fie.interval.denominator/fie.interval.numerator);
        }

    }
    CLEAR(s_fi);
    s_fi.pad = 0;
    s_fi.interval.denominator = 15;
    s_fi.interval.numerator =  1;
    //set the fmt and frame interval
    if( 0 > xioctl(cam_fd, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &s_fi))
    {
        fprintf(stderr,"cannot set frame interval :%d, %s\n",errno,strerror(errno));
        exit(1);
    }
    CLEAR(s_fmt);
    s_fmt.pad = 0;
    s_fmt.which = 1; //active formats
    s_fmt.format.width = src_width;
    s_fmt.format.height = src_height;
    s_fmt.format.code = MEDIA_BUS_FMT_YUYV8_2X8;

    if(0 > xioctl(cam_fd, VIDIOC_SUBDEV_S_FMT, &s_fmt))
    {
        fprintf(stderr,"cannot set frame fmt :%d, %s\n",errno,strerror(errno));
        exit(1);
    }
    close(cam_fd);
    return 0;
}

int main(int argc, char **argv)
{
        dev_name = "/dev/video0";

        test_timestamp_printf();
        init_display();
        open_device();     //打开capture设备
        init_device();     //查询，设置视频格式
        start_capturing(); //请求分配缓冲，获取内核空间，
         //mainloop_2();        //主要处理均在该函数中实现
        start_display();
     
        stop_capturing();
        uninit_device();
        close_device();
        //fprintf(stderr, "\n");
        return 0;
}
