#ifndef CHAN_RENDERER_DEF
#define CHAN_RENDERER_DEF

#include "html.h"
#include "json.h"

#define MAX_POSTS_ON_SCREEN 6
#define MAX_POSTS 1024
#define CHAN_MAX_POST_WIDTH 1024 
#define CHAN_MAX_POST_HEIGHT 128 
#define CHAN_TEXTURE_WIDTH 1024 
#define CHAN_TEXTURE_HEIGHT (CHAN_MAX_POST_HEIGHT * MAX_POSTS_ON_SCREEN) 

typedef struct {
	HTML_Tag 		*comment;
	char 			*name;
	char 			*trip;
	char 			*filename;
	unsigned int 	image;
	int				imgW;
	int				imgH;
	char 			*now;
	char 			*tim;
} Post4Chan;


typedef struct {
	void 				*stack;
	int 				stackSize;
	Post4Chan 			posts[MAX_POSTS];
	int 				numPosts;
	JSON_Value 			*top;
	char 				*memory;
	int 				width;
	int 				height;
	unsigned int 		frameBuffer;
	unsigned int 		texture;
	unsigned int 		images;
	unsigned int 		vao;
	unsigned int 		vbo;
} Thread4Chan;

void ChanRenderer_Load(Thread4Chan *thread, char *number, void *memory, int memSize);
void ChanRenderer_Init(Thread4Chan *thread);
void ChanRenderer_Render(Thread4Chan *thread);
void ChanRenderer_Close(Thread4Chan *thread);

#endif