
#include "fastmemcpy.h"
#include "../mmx_defs.h"

// mga_vid drawing functions

static int mga_next_frame=0;

static mga_vid_config_t mga_vid_config;
static uint8_t *vid_data, *frames[4];
static int f;

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
    int x,y;
    uint32_t bespitch = (mga_vid_config.src_width + 31) & ~31;
    switch(mga_vid_config.format){
    case MGA_VID_FORMAT_YV12:
    case MGA_VID_FORMAT_IYUV:
    case MGA_VID_FORMAT_I420:
        vo_draw_alpha_yv12(w,h,src,srca,stride,vid_data+bespitch*y0+x0,bespitch);
        break;
    case MGA_VID_FORMAT_YUY2:
        vo_draw_alpha_yuy2(w,h,src,srca,stride,vid_data+2*(bespitch*y0+x0),2*bespitch);
        break;
    case MGA_VID_FORMAT_UYVY:
        vo_draw_alpha_yuy2(w,h,src,srca,stride,vid_data+2*(bespitch*y0+x0)+1,2*bespitch);
        break;
    }
}


//static void
//write_slice_g200(uint8_t *y,uint8_t *cr, uint8_t *cb,uint32_t slice_num)

static void
draw_slice_g200(uint8_t *image[], int stride[], int width,int height,int x,int y)
{
	uint8_t *src;
	uint8_t *src2;
	uint8_t *dest;
	uint32_t bespitch,h,w;

	bespitch = (mga_vid_config.src_width + 31) & ~31;

	dest = vid_data + bespitch*y + x;
	mem2agpcpy_pic(dest, image[0], width, height, bespitch, stride[0]);

        width/=2;height/=2;x/=2;y/=2;

	dest = vid_data + bespitch*mga_vid_config.src_height + bespitch*y + 2*x;
        src = image[1];
        src2 = image[2];
	for(h=0; h < height; h++)
	{
#ifdef HAVE_MMX
		asm(
			"xorl %%eax, %%eax		\n\t"
			"1:				\n\t"
			PREFETCH" 64(%1, %%eax)		\n\t"
			PREFETCH" 64(%2, %%eax)		\n\t"
			"movq (%1, %%eax), %%mm0	\n\t"
			"movq 8(%1, %%eax), %%mm2	\n\t"
			"movq %%mm0, %%mm1		\n\t"
			"movq %%mm2, %%mm3		\n\t"
			"movq (%2, %%eax), %%mm4	\n\t"
			"movq 8(%2, %%eax), %%mm5	\n\t"
			"punpcklbw %%mm4, %%mm0		\n\t"
			"punpckhbw %%mm4, %%mm1		\n\t"
			"punpcklbw %%mm5, %%mm2		\n\t"
			"punpckhbw %%mm5, %%mm3		\n\t"
			MOVNTQ" %%mm0, (%0, %%eax, 2)	\n\t"
			MOVNTQ" %%mm1, 8(%0, %%eax, 2)	\n\t"
			MOVNTQ" %%mm2, 16(%0, %%eax, 2)	\n\t"
			MOVNTQ" %%mm3, 24(%0, %%eax, 2)	\n\t"
			"addl $16, %%eax			\n\t"
			"cmpl %3, %%eax			\n\t"
			" jb 1b				\n\t"
			::"r"(dest), "r"(src), "r"(src2), "r" (width-15)
			: "memory", "%eax"
		);
		for(w= (width&(~15)); w < width; w++)
		{
			dest[2*w+0] = src[w];
			dest[2*w+1] = src2[w];
		}
#else
		for(w=0; w < width; w++)
		{
			dest[2*w+0] = src[w];
			dest[2*w+1] = src2[w];
		}
#endif
		dest += bespitch;
                src += stride[1];
                src2+= stride[2];
	}
#ifdef HAVE_MMX
	asm(
		EMMS" \n\t"
		SFENCE" \n\t"
		::: "memory"
		);
#endif
}

static void
draw_slice_g400(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *src;
    uint8_t *dest;
    uint32_t bespitch,bespitch2;
    int i;

    bespitch = (mga_vid_config.src_width + 31) & ~31;
    bespitch2 = bespitch/2;

    dest = vid_data + bespitch * y + x;
    mem2agpcpy_pic(dest, image[0], w, h, bespitch, stride[0]);
    
    w/=2;h/=2;x/=2;y/=2;
    
    dest = vid_data + bespitch*mga_vid_config.src_height + bespitch2 * y + x;
    mem2agpcpy_pic(dest, image[1], w, h, bespitch2, stride[1]);

    dest = vid_data + bespitch*mga_vid_config.src_height
                    + bespitch*mga_vid_config.src_height / 4 
                    + bespitch2 * y + x;
    mem2agpcpy_pic(dest, image[2], w, h, bespitch2, stride[2]);

}

static uint32_t
draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{

#if 0
	printf("vo: %p/%d %p/%d %p/%d  %dx%d/%d;%d  \n",
	    src[0],stride[0],
	    src[1],stride[1],
	    src[2],stride[2],
	    w,h,x,y);
#endif

	if (mga_vid_config.card_type == MGA_G200)
            draw_slice_g200(src,stride,w,h,x,y);
	else
            draw_slice_g400(src,stride,w,h,x,y);
	return 0;
}

static void
vo_mga_flip_page(void)
{

//    printf("-- flip to %d --\n",mga_next_frame);

#if 1
	ioctl(f,MGA_VID_FSEL,&mga_next_frame);
	mga_next_frame=(mga_next_frame+1)%mga_vid_config.num_frames;
	vid_data=frames[mga_next_frame];
#endif

}


static void
write_frame_yuy2(uint8_t *y)
{
        int len=2*mga_vid_config.src_width;
	uint32_t bespitch = (mga_vid_config.src_width + 31) & ~31;

	mem2agpcpy_pic(vid_data, y, len, mga_vid_config.src_height, 2*bespitch, len);
}


static uint32_t
draw_frame(uint8_t *src[])
{
    switch(mga_vid_config.format){
    case MGA_VID_FORMAT_YUY2:
    case MGA_VID_FORMAT_UYVY:
        write_frame_yuy2(src[0]);break;
    }
    return 0;
}

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
//    case IMGFMT_RGB|24:
//    case IMGFMT_BGR|24:
        return 1;
    }
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}


static int mga_init(){
	char *frame_mem;

	mga_vid_config.num_frames=4;
	mga_vid_config.version=MGA_VID_VERSION;
	if (ioctl(f,MGA_VID_CONFIG,&mga_vid_config))
	{
		perror("Error in mga_vid_config ioctl()");
                printf("Your mga_vid driver version is incompatible with this MPlayer version!\n");
		return -1;
	}
	ioctl(f,MGA_VID_ON,0);

	frames[0] = (char*)mmap(0,mga_vid_config.frame_size*mga_vid_config.num_frames,PROT_WRITE,MAP_SHARED,f,0);
	frames[1] = frames[0] + 1*mga_vid_config.frame_size;
	frames[2] = frames[0] + 2*mga_vid_config.frame_size;
	frames[3] = frames[0] + 3*mga_vid_config.frame_size;
	mga_next_frame = 0;
	vid_data = frames[mga_next_frame];

	//clear the buffer
	memset(frames[0],0x80,mga_vid_config.frame_size*mga_vid_config.num_frames);

  return 0;

}

static int mga_uninit(){
	ioctl( f,MGA_VID_OFF,0 );
	munmap(frames[0],mga_vid_config.frame_size*mga_vid_config.num_frames);
	close(f);
}

static uint32_t preinit(const char *arg)
{
  return 0;
}

