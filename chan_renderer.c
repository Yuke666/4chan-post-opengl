#include <GL/glew.h>
#include <string.h>
#include <stdlib.h>
#include "json.h"
#include "shader_files.h"
#include "shaders.h"
#include "chan.h"
#include "log.h"
#include "html.h"
#include "text.h"
#include "chan_renderer.h"
#include "memory.h"

#define TEXT_SIZE 16
#define BUFFER_SIZE (0x01 << 20)

static void InitFrameBuffers(Thread4Chan *thread, int w, int h);

static void DumpPosts(Thread4Chan *thread, int post, JSON_Value *value){

	if(!value) return;

	// if(value->type != JSON_ARRAY && value->type != JSON_OBJECT){
	if(value->type != JSON_ARRAY){
		
		if(value->key && value->string){

			if(strcmp(value->key, "now") == 0)
				thread->posts[post].now = value->string;
			else if(strcmp(value->key, "tn_h") == 0)
				thread->posts[post].imgH = atoi(value->string);
			else if(strcmp(value->key, "tn_w") == 0)
				thread->posts[post].imgW = atoi(value->string);
			else if(strcmp(value->key, "tim") == 0)
				thread->posts[post].tim = value->string;
			else if(strcmp(value->key, "name") == 0)
				thread->posts[post].name = value->string;
			else if(strcmp(value->key, "trip") == 0)
				thread->posts[post].trip = value->string;
			else if(strcmp(value->key, "com") == 0){

				int ret = HTML_Parse(&thread->posts[post].comment, value->string, strlen(value->string),
					thread->stack, thread->stack + thread->stackSize, ALIGNMENT);
				
				if(ret < 0){
					
					// FUUUUUUUUUUUUUUUUUUUUUUUUCK

				} else {

					thread->posts[post].comment = thread->posts[post].comment->children;

					thread->stack += ret;
					thread->stackSize -= ret;
				}
			}
		}

		DumpPosts(thread, post, value->next);

		return;
	}

	DumpPosts(thread, post + 1, value->next);

	memset(&thread->posts[post], 0, sizeof(Post4Chan));

	DumpPosts(thread, post, value->children);

	++thread->numPosts;
}

static void Dump(Thread4Chan *thread, JSON_Value *value){

	if(!value) return;

	if(value->type == JSON_ARRAY && value->key && strcmp(value->key, "posts") == 0){
		thread->numPosts = 0;
		memset(thread->posts, 0, sizeof(thread->posts));
		DumpPosts(thread, 0, value->children);
		return;
	}

	Dump(thread, value->next);
	Dump(thread, value->children);
}

static void PrintComment(HTML_Tag *tag, float *x, float *y, Vec4 currColor){

	if(tag->next)
		PrintComment(tag->next, x, y, currColor);

	if(tag->string){
        glUseProgram(Shaders_GetProgram(PROGRAM_2d));
        glUniform4fv(Shaders_GetFUniformLoc(PROGRAM_2d, FUNIFORM_uniformColor), 1, &currColor.x);
		Text_Draw(*x, *y, 0, 0, CHAN_MAX_POST_WIDTH, tag->string);
		*y += TEXT_SIZE;
	}

	currColor = (Vec4){0, 0, 0, 1};

	if(tag->key){
		
		if(strcmp(tag->key, "br") == 0){
			*y += TEXT_SIZE;
		}
		else if(strcmp(tag->key, "a") == 0){

			currColor = (Vec4){221/255.0, 21/255.0, 32/255.0, 1};
		}
	}

	if(tag->children)
		PrintComment(tag->children, x, y, currColor);

}

static void PrintPosts(Thread4Chan *thread){

	Vec4 bgColor = (Vec4){214/255.0f,218/255.0f,240/255.0f,1};
	Vec4 textColor = (Vec4){0,0,0,1};
	Vec4 white = (Vec4){1,1,1,1};

	char buffer[64];

	int num = 0;

	int k;
	for(k = 0; k < thread->numPosts; k++){

		Post4Chan *post = &thread->posts[k];

		if(!post->name)
			continue;
	
		glViewport(0, (num * CHAN_MAX_POST_HEIGHT), CHAN_MAX_POST_WIDTH, CHAN_MAX_POST_HEIGHT);
		// glViewport(0, (num * CHAN_MAX_POST_HEIGHT), CHAN_MAX_POST_WIDTH, CHAN_MAX_POST_HEIGHT);
	
		float orthoProj[16];
		Math_Ortho(orthoProj, 0, CHAN_MAX_POST_WIDTH, 0, CHAN_MAX_POST_HEIGHT, -10, 10);

		glUseProgram(Shaders_GetProgram(PROGRAM_2d));
		glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_2d, VUNIFORM_projView), 1, GL_TRUE, orthoProj);
		glUseProgram(Shaders_GetProgram(PROGRAM_textureless_2d));
		glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_textureless_2d, VUNIFORM_projView), 1, GL_TRUE, orthoProj);

		float offsetY = 0;
		float offsetX = 0;

		float y = offsetY;
		float x = offsetX;

        glUseProgram(Shaders_GetProgram(PROGRAM_textureless_2d));

        glUniform4fv(Shaders_GetFUniformLoc(PROGRAM_textureless_2d, FUNIFORM_uniformColor), 1, &bgColor.x);
		Text_DrawRect(0, (Rect2D){ 0, 0, CHAN_MAX_POST_WIDTH,CHAN_MAX_POST_HEIGHT}, (Rect2D){0,0,0,0});

		if(post->imgW && post->imgH && post->tim){

	        float w = post->imgW;
	        float h = post->imgH;

	        glUseProgram(Shaders_GetProgram(PROGRAM_2d));
	        glUniform4fv(Shaders_GetFUniformLoc(PROGRAM_2d, FUNIFORM_uniformColor), 1, &white.x);

			Text_DrawRect(post->image, (Rect2D){ 0, 0, w, h}, (Rect2D){ 0, 0, 1, 1});
			
			x += w + 6;
		}

        glUseProgram(Shaders_GetProgram(PROGRAM_2d));
        glUniform4fv(Shaders_GetFUniformLoc(PROGRAM_2d, FUNIFORM_uniformColor), 1, &textColor.x);

		if(post->trip)
			sprintf(buffer, "%s%s",  post->name, post->trip);
		else
			sprintf(buffer, "%s", post->name);
		
		if(post->filename)
			sprintf(buffer, "%s%s", buffer, post->filename);

		Text_Draw(x, y, 0, 0, CHAN_MAX_POST_WIDTH, buffer);

		y += TEXT_SIZE;

		if(post->now)
			Text_Draw(x, y, 0, 0, CHAN_MAX_POST_WIDTH, post->now);

		y += TEXT_SIZE;

		if(post->comment)
			PrintComment(post->comment, &x, &y, textColor);


		++num;
	}
}

void ChanRenderer_Load(Thread4Chan *thread, char *number, void *memory, int memSize){

	char *buffer = (char *)Memory_StackAlloc(TEMP_STACK, BUFFER_SIZE);

	printf("LOADING\n");

	int len = Chan_LoadThread(number, buffer);

	printf("LOADED\n");

	thread->stackSize = memSize;
	thread->memory = memory;

	thread->stack = thread->memory;

	int ret = JSON_Parse(&thread->top, buffer, len, thread->stack, thread->stack + thread->stackSize, ALIGNMENT);

	if(ret < 0){
		LOGF(LOG_RED,"%s\n", JSON_Error(ret));
		return;
	}

	printf("PARSED\n");

	thread->stack += ret;
	thread->stackSize -= ret;
	Dump(thread, thread->top);

	printf("DUMPED\n");

	Memory_StackPop(TEMP_STACK, 1);

	printf("POPPED\n");

	int k;
	for(k = 0; k < thread->numPosts && k < MAX_POSTS_ON_SCREEN; k++){
	
		if(!thread->posts[k].imgW || !thread->posts[k].imgH || !thread->posts[k].tim)
			continue;

		int w, h;
		unsigned char *pixels = Chan_GetThumbnail(thread->posts[k].tim, &w, &h);

		if(!pixels)
			continue;

		printf("%s\n", thread->posts[k].tim);
	
	    glGenTextures(1, &thread->posts[k].image);
	    glBindTexture(GL_TEXTURE_2D, thread->posts[k].image);
	    glPixelStorei(GL_UNPACK_ALIGNMENT,1);

	    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	    thread->posts[k].imgW = w;
	    thread->posts[k].imgH = h;
	
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

	    glBindTexture(GL_TEXTURE_2D, 0);

	    Memory_StackPop(MAIN_STACK, 1);
	}

	printf("HERE\n");

	InitFrameBuffers(thread, CHAN_TEXTURE_WIDTH, CHAN_TEXTURE_HEIGHT);

	glBindFramebuffer(GL_FRAMEBUFFER, thread->frameBuffer);
	
	unsigned int drawBuffer = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &drawBuffer);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glClearColor(0,0,0,1);

	glViewport(0, 0, thread->width, thread->height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	PrintPosts(thread);
	
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glViewport(0,0,WINDOW_INIT_WIDTH, WINDOW_INIT_HEIGHT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	Memory_StackPop(TEMP_STACK, 1);

	float orthoProj[16];
	Math_Ortho(orthoProj, 0, WINDOW_INIT_WIDTH, 0, WINDOW_INIT_HEIGHT, -10, 10);

	glUseProgram(Shaders_GetProgram(PROGRAM_2d));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_2d, VUNIFORM_projView), 1, GL_TRUE, orthoProj);
	glUseProgram(Shaders_GetProgram(PROGRAM_textureless_2d));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_textureless_2d, VUNIFORM_projView), 1, GL_TRUE, orthoProj);
}

static void InitFrameBuffers(Thread4Chan *thread, int w, int h){

	thread->width = w;
	thread->height = h;

	glGenFramebuffers(1,&thread->frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, thread->frameBuffer);    

	glGenTextures(1, &thread->texture);
	glBindTexture(GL_TEXTURE_2D, thread->texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, thread->texture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		LOG(LOG_YELLOW, "Chan InitFrameBuffers: Error creating frameBuffer.\n");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ChanRenderer_Close(Thread4Chan *thread){

	int k;
	for(k = 0; k < thread->numPosts; k++)
		if(thread->posts[k].image)
			glDeleteTextures(1, &thread->posts[k].image);

	glDeleteTextures(1, &thread->texture);
	glDeleteFramebuffers(1, &thread->frameBuffer);
}

void ChanRenderer_Render(Thread4Chan *thread){

	glUseProgram(Shaders_GetProgram(PROGRAM_standard_3d));

	glBindVertexArray(thread->vao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, thread->texture);
	
	glDrawArrays(GL_TRIANGLES, 0, 36 * MAX_POSTS_ON_SCREEN);
	// glDrawArrays(GL_TRIANGLES, 0, 36);

	glBindVertexArray(0);
}

void ChanRenderer_Init(Thread4Chan *thread){

	glGenVertexArrays(1, &thread->vao);
	glBindVertexArray(thread->vao);
	glGenBuffers(1, &thread->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, thread->vbo);

    int stride = sizeof(Vec3) + sizeof(Vec2);

    glEnableVertexAttribArray(POS_LOC);
    glVertexAttribPointer(POS_LOC, 3, GL_FLOAT, GL_FALSE, stride, 0);

    glEnableVertexAttribArray(UV_LOC);
    glVertexAttribPointer(UV_LOC, 2, GL_FLOAT, GL_FALSE, stride, (void *)sizeof(Vec3));

    glBufferData(GL_ARRAY_BUFFER, stride * MAX_POSTS_ON_SCREEN * 36, NULL, GL_STATIC_DRAW);

    float verts[] = {
	   	// First face (PZ)
		0.0f, 0.0f, 1.0f, 0.0, 0.0,
		1.0f, 0.0f, 1.0f, 0.0, 0.0,
		1.0f, 1.0f, 1.0f, 0.0, 0.0,
		1.0f, 1.0f, 1.0f, 0.0, 0.0,
		0.0f, 1.0f, 1.0f, 0.0, 0.0,
		0.0f, 0.0f, 1.0f, 0.0, 0.0,
		// Second face (MZ)
		0.0f, 0.0f, 0.0f, 1.0, 0.0,
		0.0f, 1.0f, 0.0f, 1.0, 1.0,
		1.0f, 1.0f, 0.0f, 0.0, 1.0,
		1.0f, 1.0f, 0.0f, 0.0, 1.0,
		1.0f, 0.0f, 0.0f, 0.0, 0.0,
		0.0f, 0.0f, 0.0f, 1.0, 0.0,
		// Third face (PX)
		1.0f, 0.0f, 0.0f, 0.0, 0.0,
		1.0f, 1.0f, 0.0f, 0.0, 0.0,
		1.0f, 1.0f, 1.0f, 0.0, 0.0,
		1.0f, 1.0f, 1.0f, 0.0, 0.0,
		1.0f, 0.0f, 1.0f, 0.0, 0.0,
		1.0f, 0.0f, 0.0f, 0.0, 0.0,
		// Fourth face (MX)
		0.0f, 0.0f, 0.0f, 0.0, 0.0,
		0.0f, 0.0f, 1.0f, 0.0, 0.0,
		0.0f, 1.0f, 1.0f, 0.0, 0.0,
		0.0f, 1.0f, 1.0f, 0.0, 0.0,
		0.0f, 1.0f, 0.0f, 0.0, 0.0,
		0.0f, 0.0f, 0.0f, 0.0, 0.0,
		// Fifth face (PY)
		0.0f, 1.0f, 0.0f, 0.0, 0.0,
		0.0f, 1.0f, 1.0f, 0.0, 0.0,
		1.0f, 1.0f, 1.0f, 0.0, 0.0,
		1.0f, 1.0f, 1.0f, 0.0, 0.0,
		1.0f, 1.0f, 0.0f, 0.0, 0.0,
		0.0f, 1.0f, 0.0f, 0.0, 0.0,
		// Sixth face (MY)
		0.0f, 0.0f, 0.0f, 0.0, 0.0,
		1.0f, 0.0f, 0.0f, 0.0, 0.0,
		1.0f, 0.0f, 1.0f, 0.0, 0.0,
		1.0f, 0.0f, 1.0f, 0.0, 0.0,
		0.0f, 0.0f, 1.0f, 0.0, 0.0,
		0.0f, 0.0f, 0.0f, 0.0, 0.0,
	};

	int size = 36 * stride;

	int k;
	for(k = 0; k < MAX_POSTS_ON_SCREEN; k++){

		float postVerts[36*5];
		memcpy(postVerts, verts, sizeof(postVerts));

		int j;
		for(j = 0; j < 36; j++){

			Vec3 *vert = (Vec3 *)&postVerts[(j*5)];
			Vec2 *uv = (Vec2 *)&postVerts[(j*5)+3];

			vert->x *= 7;
			vert->y *= 7;
			vert->z *= 0.1;

			vert->y *= (CHAN_MAX_POST_HEIGHT/(float)CHAN_TEXTURE_HEIGHT);

			vert->x -= vert->x/2;
			vert->y += (1.5 * k) + 0.2;

			uv->y *= (CHAN_MAX_POST_HEIGHT/(float)CHAN_TEXTURE_HEIGHT);
			uv->y += k * (CHAN_MAX_POST_HEIGHT/(float)CHAN_TEXTURE_HEIGHT);
		}

		glBufferSubData(GL_ARRAY_BUFFER, k * size, size, postVerts);
	}

	glBindVertexArray(0);
}