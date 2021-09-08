#ifndef RENDERER_DEF
#define RENDERER_DEF

#define MAX_ON_SCREEN_OBJECTS 1024

#include "model.h"

#include "object.h"

#define MAX_ON_SCREEN_LIGHTS 1024
#define SSAO_NOISE_SIZE 4
#define SSAO_KERNEL_SIZE 32
#define HALF_RES 0.5
#define SHADOW_CASTER_FAR 1000
#define SHADOW_CASTER_NEAR 0.1
#define SHADOW_MAP_HEIGHT 1024
#define SHADOW_MAP_WIDTH 1024
#define MAX_SHADOW_SOURCES 4

typedef struct {
	Vec3 pos;
	Vec3 forward;
	Vec3 up;
	float fov;
} ShadowSource;

typedef struct {
	Vec3 pos;
	Vec3 color;
	float constant;
	float linear;
	float quadratic;
} Light;

typedef struct {

	unsigned int 	depthTexture;
	unsigned int 	shadowMaskTexture;
	unsigned int 	depthFramebuffer;
	unsigned int 	maskFramebuffer;

	ShadowSource 	sources[MAX_SHADOW_SOURCES];
	int 			nSources;

	float 			matrices[16 * MAX_SHADOW_SOURCES];

} ShadowRenderer;

typedef struct {
	unsigned int framebuffer;
	unsigned int normalTexture;
	unsigned int depthTexture;
} NormalDepthPrepass;

typedef struct {
	unsigned int framebuffer;
	unsigned int diffuseTexture;
	unsigned int specularTexture;
} LightPrepass;

typedef struct {
	unsigned int 		noiseTexture;
	unsigned int 		ssaoTexture;
	unsigned int 		blurTexture;
	unsigned int 		ssaoFramebuffer;
	unsigned int 		blurFramebuffer;
} SSAORenderer;

typedef struct {

	float 				view[16];
	float 				proj[16];
	float 				projView[16];
	float 				invView[16];
	float 				invProj[16];
	float 				invProjView[16];

	Vec3 				camPos;
	Vec3 				camForward; 

	Object 				*objects[MAX_ON_SCREEN_OBJECTS];
	int 				nObjects;

	Light 				lights[MAX_ON_SCREEN_LIGHTS];
	int 				nLights;

	int 				resX;
	int 				resY;

	NormalDepthPrepass 	ndPrepass;
	ShadowRenderer 		sRenderer;
	LightPrepass 		lPrepass;

	unsigned int 		quadVao;
	unsigned int 		quadVbo;

	SSAORenderer		ssao;

	float 				tanHalfFov;
	float 				aspectRatio;

	unsigned int 		sphereVao;
	unsigned int 		sphereVbo;
	unsigned int 		sphereEbo;


} Renderer;

void Renderer_Close(Renderer *renderer);
void Renderer_Render(Renderer *renderer);
void Renderer_AddObjectToFrame(Renderer *renderer, Object *object);
void Renderer_AddOLightToFrame(Renderer *renderer, Light light);
void Renderer_Init(Renderer *renderer, int resX, int resY);
void Renderer_Resize(Renderer *renderer, int resX, int resY);
void Renderer_SetPerspective(Renderer *renderer, float fov, float a, float n, float f);

#endif