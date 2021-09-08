#include <GL/glew.h>
#include "renderer.h"
#include "math.h"
#include "log.h"
#include "shaders.h"
#include "game.h"
#include "window.h"
#include "model.h"
#include "shader_files.h"

#define NUM_ICOSPHERE_ELEMENTS 240

static const Vec3 icoSphereVerts[42] = {
	{0.000000, 0.000000, -1.000000},
	{0.425323, -0.309011, -0.850654},
	{-0.162456, -0.499995, -0.850654},
	{0.723607, -0.525725, -0.447220},
	{0.850648, 0.000000, -0.525736},
	{-0.525730, 0.000000, -0.850652},
	{-0.162456, 0.499995, -0.850654},
	{0.425323, 0.309011, -0.850654},
	{0.951058, -0.309013, 0.000000},
	{-0.276388, -0.850649, -0.447220},
	{0.262869, -0.809012, -0.525738},
	{0.000000, -1.000000, 0.000000},
	{-0.894426, 0.000000, -0.447216},
	{-0.688189, -0.499997, -0.525736},
	{-0.951058, -0.309013, 0.000000},
	{-0.276388, 0.850649, -0.447220},
	{-0.688189, 0.499997, -0.525736},
	{-0.587786, 0.809017, 0.000000},
	{0.723607, 0.525725, -0.447220},
	{0.262869, 0.809012, -0.525738},
	{0.587786, 0.809017, 0.000000},
	{0.587786, -0.809017, 0.000000},
	{-0.587786, -0.809017, 0.000000},
	{-0.951058, 0.309013, 0.000000},
	{0.000000, 1.000000, 0.000000},
	{0.951058, 0.309013, 0.000000},
	{0.276388, -0.850649, 0.447220},
	{0.688189, -0.499997, 0.525736},
	{0.162456, -0.499995, 0.850654},
	{-0.723607, -0.525725, 0.447220},
	{-0.262869, -0.809012, 0.525738},
	{-0.425323, -0.309011, 0.850654},
	{-0.723607, 0.525725, 0.447220},
	{-0.850648, 0.000000, 0.525736},
	{-0.425323, 0.309011, 0.850654},
	{0.276388, 0.850649, 0.447220},
	{-0.262869, 0.809012, 0.525738},
	{0.162456, 0.499995, 0.850654},
	{0.894426, 0.000000, 0.447216},
	{0.688189, 0.499997, 0.525736},
	{0.525730, 0.000000, 0.850652},
	{0.000000, 0.000000, 1.000000},
};

static const u16 icoSphereElements[240] = {
	0,1,2,3,1,4,0,2,5,0,5,6,0,6,7,3,4,8,9,10,11,12,13,14,15,16,17,18,19,20,3,8,21,9,11,22,12,14,23,15,17,24,18,20,25,26,
	27,28,29,30,31,32,33,34,35,36,37,38,39,40,40,37,41,40,39,37,39,35,37,37,34,41,37,36,34,36,32,34,34,31,41,34,33,31,33,
	29,31,31,28,41,31,30,28,30,26,28,28,40,41,28,27,40,27,38,40,25,39,38,25,20,39,20,35,39,24,36,35,24,17,36,17,32,36,23,
	33,32,23,14,33,14,29,33,22,30,29,22,11,30,11,26,30,21,27,26,21,8,27,8,38,27,20,24,35,20,19,24,19,15,24,17,23,32,17,16,
	23,16,12,23,14,22,29,14,13,22,13,9,22,11,21,26,11,10,21,10,3,21,8,25,38,8,4,25,4,18,25,7,19,18,7,6,19,6,15,19,6,16,15,
	6,5,16,5,12,16,5,13,12,5,2,13,2,9,13,4,7,18,4,1,7,1,0,7,2,10,9,2,1,10,1,3,10
};

static void RenderModel(Renderer *renderer, Object *obj);
static void RenderModelPassthrough(Object *obj);
static void RenderRiggedModelPassthrough(Object *obj);
static void RenderRiggedModel(Renderer *renderer, Object *obj);
static void UpdateMatrices(Renderer *renderer);
static void UpdateMatrixUniforms(Renderer *renderer, float *projView, float *view);

static void RenderQuad(Renderer *renderer){

	glBindVertexArray(renderer->quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

static void NormalDepthPrepass_Close(NormalDepthPrepass *ndPrepass){
	glDeleteTextures(1, &ndPrepass->depthTexture);
	glDeleteTextures(1, &ndPrepass->normalTexture);
	glDeleteFramebuffers(1, &ndPrepass->framebuffer);
}

static void ShadowRenderer_Close(ShadowRenderer *sRenderer){
	glDeleteTextures(1, &sRenderer->depthTexture);
	glDeleteTextures(1, &sRenderer->shadowMaskTexture);
	glDeleteFramebuffers(1, &sRenderer->maskFramebuffer);
	glDeleteFramebuffers(1, &sRenderer->depthFramebuffer);
}

static void GenerateColorTex(int w, int h, GLuint *tex, int filterType, int internalFormat, int format, int type){
	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, *tex);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterType);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterType);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void GenerateDepthTex(int w, int h, GLuint *tex, int filterType){
	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, *tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	// glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterType);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterType);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
}

static void ShadowRenderer_RenderMasks(Renderer *renderer, int depthTexture){

	ShadowRenderer *sRenderer = &renderer->sRenderer;

	glBindFramebuffer(GL_FRAMEBUFFER, sRenderer->maskFramebuffer);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sRenderer->depthTexture);	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthTexture);	

	glUseProgram(Shaders_GetProgram(PROGRAM_shadow_mask));
	glUniformMatrix4fv(Shaders_GetFUniformLoc(PROGRAM_shadow_mask, FUNIFORM_invProjView), 1, GL_TRUE, renderer->invProjView);
	glUniformMatrix4fv(Shaders_GetFUniformLoc(PROGRAM_shadow_mask, FUNIFORM_lightMatrices), 
		MAX_SHADOW_SOURCES, GL_TRUE, sRenderer->matrices);

	RenderQuad(renderer);
}

static void LightPrepass_Resize(LightPrepass *lPrepass, int resX, int resY){

	glBindTexture(GL_TEXTURE_2D, lPrepass->diffuseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, resX, resY, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, lPrepass->specularTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, resX, resY, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
}

static void LightPrepass_Init(LightPrepass *lPrepass, int resX, int resY){

	glGenFramebuffers(1,&lPrepass->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, lPrepass->framebuffer);    

	GenerateColorTex(resX, resY, &lPrepass->diffuseTexture, GL_NEAREST, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lPrepass->diffuseTexture, 0);
	GenerateColorTex(resX, resY, &lPrepass->specularTexture, GL_NEAREST, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, lPrepass->specularTexture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG(LOG_RED, "LightPrepass: Error creating framebuffer.\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glUseProgram(Shaders_GetProgram(PROGRAM_light_prepass));
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_normalTexture), 1);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_depthTexture), 0);
}

static void LightPrepass_Render(Renderer *renderer){

	glDrawBuffers(2, (GLuint[]){GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});

	glBindFramebuffer(GL_FRAMEBUFFER, renderer->lPrepass.framebuffer);

	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(Shaders_GetProgram(PROGRAM_light_prepass));

	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_light_prepass, VUNIFORM_projView), 1, GL_TRUE, renderer->projView);
	glUniformMatrix4fv(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_invProj), 1, GL_TRUE, renderer->invProj);

	glBindVertexArray(renderer->sphereVao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->ndPrepass.depthTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, renderer->ndPrepass.normalTexture);

	// batch lights
	// global lights are quads

	int k;
	for(k = 0; k < renderer->nLights; k++){

		Light *light = &renderer->lights[k];

		Vec4 vcenter = Math_MatrixMult4((Vec4){light->pos.x,light->pos.y,light->pos.z,1}, renderer->view);

		vcenter.x /= vcenter.w;
		vcenter.y /= vcenter.w;
		vcenter.z /= vcenter.w;

		float dist = Math_Vec3Magnitude(Math_Vec3SubVec3(light->pos, renderer->camPos));

		// float radius = light->radius;

		float lightMax = MAX(MAX(light->color.x, light->color.y), light->color.z);
		float radius = (-light->linear + sqrt(light->linear * light->linear - 4 * 
			light->quadratic * (light->constant - (256 / 5.0) * lightMax))) / (2 * light->quadratic);

		// printf("%f %f\n", radius, dist);

		glUniform1f(Shaders_GetVUniformLoc(PROGRAM_light_prepass, VUNIFORM_radius), radius);
		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_lightRadius), radius);

		// glUniform3fv(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_lightCenter), 1, &light->pos.x);
		glUniform3fv(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_lightCenter), 1, &vcenter.x);
		glUniform3fv(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_color), 1, &light->color.x);
		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_lightConstant), light->constant);
		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_lightQuadratic), light->quadratic);
		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_light_prepass, FUNIFORM_lightLinear), light->linear);

		// if(dist < radius){
		
		// 	// glUniform3fv(Shaders_GetVUniformLoc(PROGRAM_light_prepass, VUNIFORM_center), 1, &renderer->camPos.x);

		// 	glCullFace(GL_FRONT);
		// 	// glDepthFunc(GL_GREATER);
		// 	glDrawElements(GL_TRIANGLES, NUM_ICOSPHERE_ELEMENTS, GL_UNSIGNED_SHORT, NULL);
		// 	// glDepthFunc(GL_LESS);
		// 	glCullFace(GL_BACK);

		// } else {

			glUniform3fv(Shaders_GetVUniformLoc(PROGRAM_light_prepass, VUNIFORM_center), 1, &light->pos.x);
			glDrawElements(GL_TRIANGLES, NUM_ICOSPHERE_ELEMENTS, GL_UNSIGNED_SHORT, NULL);
		// }
	}

	glBindVertexArray(0);
}

static void ShadowRenderer_Resize(ShadowRenderer *renderer, int resX, int resY){

	glBindTexture(GL_TEXTURE_2D, renderer->depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, resX, resY, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, renderer->shadowMaskTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resX, resY, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
}

static void ShadowRenderer_Init(ShadowRenderer *renderer, int resX, int resY){

	glGenFramebuffers(1,&renderer->depthFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->depthFramebuffer);    

	GenerateDepthTex(SHADOW_MAP_WIDTH * MAX_SHADOW_SOURCES, SHADOW_MAP_HEIGHT, &renderer->depthTexture, GL_NEAREST);
	// GenerateDepthTex(resX, resY, &renderer->depthTexture, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer->depthTexture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG(LOG_RED, "ShadowRenderer: Error creating framebuffer.\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenFramebuffers(1,&renderer->maskFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->maskFramebuffer);    

	GenerateColorTex(resX, resY, &renderer->shadowMaskTexture, GL_NEAREST, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->shadowMaskTexture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG(LOG_RED, "ShadowRenderer: Error creating framebuffer.\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glUseProgram(Shaders_GetProgram(PROGRAM_shadow_mask));
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_shadow_mask, FUNIFORM_shadowsTexture), 0);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_shadow_mask, FUNIFORM_depthTexture), 1);
}

static void NormalDepthPrepass_Resize(NormalDepthPrepass *ndPrepass, int resX, int resY){
	
	glBindTexture(GL_TEXTURE_2D, ndPrepass->depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, resX, resY, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, ndPrepass->normalTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resX, resY, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
}

static void NormalDepthPrepass_Init(NormalDepthPrepass *ndPrepass, int resX, int resY){

	glGenFramebuffers(1,&ndPrepass->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, ndPrepass->framebuffer);    

	GenerateColorTex(resX, resY, &ndPrepass->normalTexture, GL_NEAREST, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ndPrepass->normalTexture, 0);

	GenerateDepthTex(resX, resY, &ndPrepass->depthTexture, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, ndPrepass->depthTexture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG(LOG_RED, "NormalDepthPrepass: Error creating framebuffer.\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void ShadowRenderer_RenderShadowMask(Renderer *renderer){
	(void)renderer;
}

static void SSAO_Render(Renderer *renderer){

	NormalDepthPrepass *ndPrepass = &renderer->ndPrepass;
	SSAORenderer *ssao = &renderer->ssao;

	// glUseProgram(Shaders_GetProgram(PROGRAM_shadow_mask));
	glUseProgram(Shaders_GetProgram(PROGRAM_ssao));
	// glBindTexture(GL_TEXTURE_2D, sRenderer->depthTexture);

	// preform ssao

	glUniform1f(Shaders_GetVUniformLoc(PROGRAM_ssao, VUNIFORM_aspect), renderer->aspectRatio);
	glUniform1f(Shaders_GetVUniformLoc(PROGRAM_ssao, VUNIFORM_tanHalfFov), renderer->tanHalfFov);
	glUniformMatrix4fv(Shaders_GetFUniformLoc(PROGRAM_ssao, FUNIFORM_proj), 1, GL_TRUE, renderer->proj);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ndPrepass->depthTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ndPrepass->normalTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, ssao->noiseTexture);

	glBindFramebuffer(GL_FRAMEBUFFER, ssao->ssaoFramebuffer);

	RenderQuad(renderer);

	// blur

	glBindFramebuffer(GL_FRAMEBUFFER, ssao->blurFramebuffer);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ssao->ssaoTexture);

	glUseProgram(Shaders_GetProgram(PROGRAM_ssao_blur_h));
	glUniform1f(Shaders_GetFUniformLoc(PROGRAM_ssao_blur_h, FUNIFORM_blurSize), SSAO_NOISE_SIZE);

	RenderQuad(renderer);

	// draw into ndprepass noraml

	glUseProgram(Shaders_GetProgram(PROGRAM_ssao_blur_v));
	glUniform1f(Shaders_GetFUniformLoc(PROGRAM_ssao_blur_v, FUNIFORM_blurSize), SSAO_NOISE_SIZE);

	glBindFramebuffer(GL_FRAMEBUFFER, ndPrepass->framebuffer);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	// glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ssao->blurTexture);

	RenderQuad(renderer);

	// end
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

static void SSAO_SetRadius(float radius){
	glUseProgram(Shaders_GetProgram(PROGRAM_ssao));
	glUniform1f(Shaders_GetFUniformLoc(PROGRAM_ssao, FUNIFORM_radius), radius);
}

static void SSAO_Resize(SSAORenderer *ssao,int resX, int resY){

	glUseProgram(Shaders_GetProgram(PROGRAM_ssao));
	glUniform2f(Shaders_GetFUniformLoc(PROGRAM_ssao, FUNIFORM_noiseScale), 
		resX/(float)SSAO_NOISE_SIZE, resY/(float)SSAO_NOISE_SIZE);

	glBindTexture(GL_TEXTURE_2D, ssao->ssaoTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, resX, resY, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, ssao->blurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, resX, resY, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
}

static void SSAO_Close(SSAORenderer *ssao){

	glDeleteFramebuffers(1, &ssao->ssaoFramebuffer);
	glDeleteFramebuffers(1, &ssao->blurFramebuffer);
	glDeleteTextures(1, &ssao->blurTexture);
	glDeleteTextures(1, &ssao->noiseTexture);
	glDeleteTextures(1, &ssao->ssaoTexture);
}

// remember, the result is stored in the alpha channel of the normal map from the normal depth prepass

static void SSAO_Init(SSAORenderer *ssao, int resX, int resY){

	glUseProgram(Shaders_GetProgram(PROGRAM_ssao));
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_ssao, FUNIFORM_depthTexture), 0);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_ssao, FUNIFORM_normalTexture), 1);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_ssao, FUNIFORM_randomTexture), 2);

	glUniform2f(Shaders_GetFUniformLoc(PROGRAM_ssao, FUNIFORM_noiseScale),
		resX/(float)SSAO_NOISE_SIZE, resY/(float)SSAO_NOISE_SIZE);

	Vec3 kernel[SSAO_KERNEL_SIZE];

	int k;
	for(k = 0; k < SSAO_KERNEL_SIZE; k++){

		kernel[k].x = ((rand() % 1000) / 500.0f) - 1;
		kernel[k].y = ((rand() % 1000) / 500.0f) - 1;
		kernel[k].z = -((rand() % 1000) / 1000.0f);

		kernel[k] = Math_Vec3Normalize(kernel[k]);

		float scale = k / (float)SSAO_KERNEL_SIZE;

		kernel[k] = Math_Vec3MultFloat(kernel[k], Math_Lerp(0.1f, 1.0f, scale * scale));
	}

	glUniform3fv(Shaders_GetFUniformLoc(PROGRAM_ssao, FUNIFORM_kernel), SSAO_KERNEL_SIZE, &kernel[0].x);

	u8 vectors[SSAO_NOISE_SIZE * SSAO_NOISE_SIZE * 2];

	for(k = 0; k < SSAO_NOISE_SIZE * SSAO_NOISE_SIZE; k++){

		Vec3 vec;

		vec.x = (((rand() % 1000) / 500.0f) - 1);
		vec.y = (((rand() % 1000) / 500.0f) - 1);
		vec.z = 0;

		vec = Math_Vec3Normalize(vec);

		vectors[(k*2)] = ((vec.x * 0.5) + 0.5) * 0xFF;
		vectors[(k*2)+1] = ((vec.y * 0.5) + 0.5) * 0xFF;
	}

	glGenTextures(1,&ssao->noiseTexture);
	glBindTexture(GL_TEXTURE_2D, ssao->noiseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, SSAO_NOISE_SIZE, SSAO_NOISE_SIZE, 0, GL_RG, GL_UNSIGNED_BYTE, vectors);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1,&ssao->ssaoFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, ssao->ssaoFramebuffer);

	GenerateColorTex(resX, resY, &ssao->ssaoTexture, GL_NEAREST, GL_RED, GL_RED, GL_UNSIGNED_BYTE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao->ssaoTexture, 0);

	glGenFramebuffers(1,&ssao->blurFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, ssao->blurFramebuffer);    

	GenerateColorTex(resX, resY, &ssao->blurTexture, GL_NEAREST, GL_RED, GL_RED, GL_UNSIGNED_BYTE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao->blurTexture, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer_Resize(Renderer *renderer, int resX, int resY){

	renderer->resX = resX;
	renderer->resY = resY;

	resX *= HALF_RES;
	resY *= HALF_RES;

	NormalDepthPrepass_Resize(&renderer->ndPrepass, resX, resY);
	ShadowRenderer_Resize(&renderer->sRenderer, resX, resY);
	SSAO_Resize(&renderer->ssao,resX, resY);

	glUseProgram(Shaders_GetProgram(PROGRAM_ssao_blur_h));
	glUniform1f(Shaders_GetFUniformLoc(PROGRAM_ssao_blur_h, FUNIFORM_texelSize), 1.0 / (float)resX);
	glUseProgram(Shaders_GetProgram(PROGRAM_ssao_blur_v));
	glUniform1f(Shaders_GetFUniformLoc(PROGRAM_ssao_blur_v, FUNIFORM_texelSize), 1.0 / (float)resY);
}

void Renderer_SetPerspective(Renderer *renderer, float fov, float a, float n, float f){

	renderer->tanHalfFov = tan(fov/2);
	renderer->aspectRatio = a;

	Math_Perspective(renderer->proj, fov, a, n, f);
}

void Renderer_Init(Renderer *renderer, int resX, int resY){

	renderer->resX = resX;
	renderer->resY = resY;

	resX *= HALF_RES;
	resY *= HALF_RES;

	glUseProgram(Shaders_GetProgram(PROGRAM_quad));
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_quad, FUNIFORM_tex), 0);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_quad, FUNIFORM_ssao), 1);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_quad, FUNIFORM_light), 2);

	glUseProgram(Shaders_GetProgram(PROGRAM_standard_3d));
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_tex), 0);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_ssao), 1);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_light), 2);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_shadowMask), 3);

	glUseProgram(Shaders_GetProgram(PROGRAM_skinned));
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_tex), 0);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_ssao), 1);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_light), 2);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_shadowMask), 3);

	NormalDepthPrepass_Init(&renderer->ndPrepass, resX, resY);
	ShadowRenderer_Init(&renderer->sRenderer, resX, resY);
	SSAO_Init(&renderer->ssao, resX, resY);
	LightPrepass_Init(&renderer->lPrepass, resX, resY);

	glUseProgram(Shaders_GetProgram(PROGRAM_ssao_blur_h));
	glUniform1f(Shaders_GetFUniformLoc(PROGRAM_ssao_blur_h, FUNIFORM_texelSize), 1.0 / (float)resX);
	glUseProgram(Shaders_GetProgram(PROGRAM_ssao_blur_v));
	glUniform1f(Shaders_GetFUniformLoc(PROGRAM_ssao_blur_v, FUNIFORM_texelSize), 1.0 / (float)resY);


	Renderer_SetPerspective(renderer, CAMERA_FOV, CAMERA_ASPECT, CAMERA_NEAR, CAMERA_FAR);

	memcpy(renderer->invProj, renderer->proj, sizeof(renderer->proj));
	Math_InverseMatrix(renderer->invProj);

	glUseProgram(Shaders_GetProgram(PROGRAM_standard_3d));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d, VUNIFORM_model), 1, GL_TRUE, math_Identity);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_normalSSAO), 1);

	glUseProgram(Shaders_GetProgram(PROGRAM_standard_3d_passthrough));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d_passthrough, VUNIFORM_model), 1, GL_TRUE, math_Identity);

	glUseProgram(Shaders_GetProgram(PROGRAM_textureless_3d));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d_passthrough, VUNIFORM_model), 1, GL_TRUE, math_Identity);

	glUseProgram(Shaders_GetProgram(PROGRAM_skinned));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_skinned, VUNIFORM_model), 1, GL_TRUE, math_Identity);
	glUniform1i(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_normalSSAO), 1);

	glUseProgram(Shaders_GetProgram(PROGRAM_particles));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_particles, VUNIFORM_model), 1, GL_TRUE, math_Identity);

	glUseProgram(Shaders_GetProgram(PROGRAM_floor));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_floor, VUNIFORM_model), 1, GL_TRUE, math_Identity);

	glUseProgram(Shaders_GetProgram(PROGRAM_skinned_passthrough));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_skinned_passthrough, VUNIFORM_model), 1, GL_TRUE, math_Identity);

	float orthoProj[16];
	Math_Ortho(orthoProj, 0, resX, 0, resY, -10, 10);

	glUseProgram(Shaders_GetProgram(PROGRAM_2d));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_2d, VUNIFORM_projView), 1, GL_TRUE, orthoProj);
	glUseProgram(Shaders_GetProgram(PROGRAM_textureless_2d));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_textureless_2d, VUNIFORM_projView), 1, GL_TRUE, orthoProj);

	// init quad drawing

	glGenVertexArrays(1, &renderer->quadVao);
	glBindVertexArray(renderer->quadVao);

	glGenBuffers(1, &renderer->quadVbo);

	glBindBuffer(GL_ARRAY_BUFFER, renderer->quadVbo);

	Vec2 rectVerts[] = {{-1,-1}, {1,-1}, {1,1}, {1,1}, {-1,1}, {-1,-1}};

    glEnableVertexAttribArray(POS_LOC);
    glVertexAttribPointer(POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBufferData(GL_ARRAY_BUFFER, sizeof(Vec2) * 6, &rectVerts[0].x, GL_STATIC_DRAW);
 
	// init sphere drawing

    glGenVertexArrays(1, &renderer->sphereVao);
    glBindVertexArray(renderer->sphereVao);

    glGenBuffers(1, &renderer->sphereVbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->sphereVbo);

    glEnableVertexAttribArray(POS_LOC);
    glVertexAttribPointer(POS_LOC, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(icoSphereVerts), icoSphereVerts, GL_STATIC_DRAW);

    glGenBuffers(1, &renderer->sphereEbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->sphereEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(icoSphereElements), icoSphereElements, GL_STATIC_DRAW);

	glBindVertexArray(0);
}

void Renderer_Close(Renderer *renderer){

	glDeleteVertexArrays(1, &renderer->quadVao);
	glDeleteBuffers(1, &renderer->quadVbo);

	glDeleteVertexArrays(1, &renderer->sphereVbo);
	glDeleteBuffers(1, &renderer->sphereEbo);
	glDeleteBuffers(1, &renderer->sphereVbo);

	NormalDepthPrepass_Close(&renderer->ndPrepass);
	ShadowRenderer_Close(&renderer->sRenderer);
	SSAO_Close(&renderer->ssao);
}

void Renderer_AddObjectToFrame(Renderer *renderer, Object *object){
	
	if(renderer->nObjects+1 >= MAX_ON_SCREEN_OBJECTS) return;

	renderer->objects[renderer->nObjects++] = object;
}

void Renderer_AddLightToFrame(Renderer *renderer, Light light){
	
	if(renderer->nLights+1 >= MAX_ON_SCREEN_LIGHTS) return;

	renderer->lights[renderer->nLights++] = light;
}

void Renderer_Render(Renderer *renderer){

	// i should sort geometry front to back first
	// skip small objects
	// remove some of these clears
	// disable depth testing. gl_EQUAL needed

	glClearColor(0,0,0,0);

	int k;

	UpdateMatrices(renderer);

	glDisable(GL_BLEND);

	// shadows

	ShadowRenderer *sRenderer = &renderer->sRenderer;
	NormalDepthPrepass *ndPrepass = &renderer->ndPrepass;

	glBindFramebuffer(GL_FRAMEBUFFER, sRenderer->depthFramebuffer);

	glClear(GL_DEPTH_BUFFER_BIT);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	for(k = 0; k < sRenderer->nSources; k++){
	
		glViewport(k * SHADOW_MAP_WIDTH, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
	
		float view[16];

		float *projView = &sRenderer->matrices[k * 16];
		Vec3 forward = sRenderer->sources[k].forward;
		Vec3 pos = sRenderer->sources[k].pos;
		Vec3 up = sRenderer->sources[k].up;

		Math_Perspective(projView, sRenderer->sources[k].fov, 1, SHADOW_CASTER_NEAR, SHADOW_CASTER_FAR);
		Math_LookAt(view, pos, Math_Vec3AddVec3(pos, forward), up);
		
		Math_MatrixMatrixMult(projView, projView, view);

		UpdateMatrixUniforms(renderer, projView, view);

		int j;
		for(j = 0; j < renderer->nObjects; j++){

			if(!renderer->objects[j]->occluder)
				continue;

			if(renderer->objects[j]->skeleton)
				RenderRiggedModelPassthrough(renderer->objects[j]);
			else
				RenderModelPassthrough(renderer->objects[j]);
		}
	}

	UpdateMatrixUniforms(renderer, renderer->projView, renderer->view);

	// end

	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glViewport(0, 0, renderer->resX/2, renderer->resY/2);

	// nd prepass

	glBindFramebuffer(GL_FRAMEBUFFER, ndPrepass->framebuffer);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for(k = 0; k < renderer->nObjects; k++){

		if(renderer->objects[k]->skeleton)
			RenderRiggedModelPassthrough(renderer->objects[k]);
		else
			RenderModelPassthrough(renderer->objects[k]);
	}

	// end

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	// light prepass

	LightPrepass_Render(renderer);

	// ssao render, replace alpha of specular with ssao

	SSAO_Render(renderer);

	// finally

	ShadowRenderer_RenderMasks(renderer, renderer->ndPrepass.depthTexture);

	// end
	
	glDepthMask(GL_TRUE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
	// glDepthFunc(GL_EQUAL);

	glViewport(0, 0, renderer->resX, renderer->resY);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(Shaders_GetProgram(PROGRAM_quad));

	glActiveTexture(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D, sRenderer->shadowMaskTexture);
	glBindTexture(GL_TEXTURE_2D, renderer->lPrepass.diffuseTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ndPrepass->normalTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, sRenderer->shadowMaskTexture);

	RenderQuad(renderer);

	// glBindFramebuffer(GL_READ_FRAMEBUFFER, ndPrepass->framebuffer);
	// glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	// glBlitFramebuffer(0,0,renderer->resX*HALF_RES,renderer->resX*HALF_RES,0,0,
	// 	renderer->resX,renderer->resY, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	// final render

	// for(k = 0; k < renderer->nObjects; k++){

	// 	if(renderer->objects[k]->skeleton)
	// 		RenderRiggedModel(renderer, renderer->objects[k]);
	// 	else
	// 		RenderModel(renderer, renderer->objects[k]);
	// }

	// //end



	// do transparent objects
	glDisable(GL_BLEND);

	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);


	renderer->nObjects = 0;
}

static void UpdateMatrices(Renderer *renderer){
	
	memcpy(renderer->invView, renderer->view, sizeof(renderer->view));
	Math_InverseMatrix(renderer->invView);

	Math_MatrixMatrixMult(renderer->projView, renderer->proj, renderer->view);

	memcpy(renderer->invProjView, renderer->projView, sizeof(renderer->projView));
	Math_InverseMatrix(renderer->invProjView);

	renderer->camPos.x = renderer->invView[3];
	renderer->camPos.y = renderer->invView[7];
	renderer->camPos.z = renderer->invView[11];
	renderer->camForward.x = renderer->view[2];
	renderer->camForward.y = renderer->view[6];
	renderer->camForward.z = renderer->view[10];

	UpdateMatrixUniforms(renderer, renderer->projView, renderer->view);
}

static void UpdateMatrixUniforms(Renderer *renderer, float *projView, float *view){

	glUseProgram(Shaders_GetProgram(PROGRAM_standard_3d));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d, VUNIFORM_projView), 1, GL_TRUE, projView);

	glUseProgram(Shaders_GetProgram(PROGRAM_standard_3d_passthrough));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d_passthrough, VUNIFORM_projView), 1, GL_TRUE, projView);
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d_passthrough, VUNIFORM_view), 1, GL_TRUE, view);

	glUseProgram(Shaders_GetProgram(PROGRAM_textureless_3d));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d_passthrough, VUNIFORM_projView), 1, GL_TRUE, projView);

	glUseProgram(Shaders_GetProgram(PROGRAM_skinned));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_skinned, VUNIFORM_projView), 1, GL_TRUE, projView);

	glUseProgram(Shaders_GetProgram(PROGRAM_floor));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_floor, VUNIFORM_projView), 1, GL_TRUE, projView);

	glUseProgram(Shaders_GetProgram(PROGRAM_skinned_passthrough));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_skinned_passthrough, VUNIFORM_projView), 1, GL_TRUE, projView);
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_skinned_passthrough, VUNIFORM_view), 1, GL_TRUE, view);

	glUseProgram(Shaders_GetProgram(PROGRAM_particles));
	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_particles, VUNIFORM_projView), 1, GL_TRUE, projView);


	Vec3 camRight = (Vec3){view[0], view[4], view[8]};
	Vec3 camUp = (Vec3){view[1], view[5], view[9]};

	glUniform3fv(Shaders_GetVUniformLoc(PROGRAM_particles, VUNIFORM_camRight), 1, &camRight.x);
	glUniform3fv(Shaders_GetVUniformLoc(PROGRAM_particles, VUNIFORM_camUp), 1, &camUp.x);
}

static void RenderRiggedModel(Renderer *renderer, Object *obj){

	glUseProgram(Shaders_GetProgram(PROGRAM_skinned));

	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_skinned, VUNIFORM_model), 1, GL_TRUE, obj->matrix);

	glUniform4fv(Shaders_GetVUniformLoc(PROGRAM_skinned, VUNIFORM_bones), 3*obj->skeleton->nBones, &obj->skeleton->matrices[0].x);

	glActiveTexture(GL_TEXTURE0);
	
	glBindVertexArray(obj->model.vao);

	int curr = 0;

	int k;
	for(k = 0; k < obj->model.nMaterials; k++){
		
		glBindTexture(GL_TEXTURE_2D, obj->model.materials[k].texture);

		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_specularHardness), obj->model.materials[k].specularHardness);
		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_ambient), obj->model.materials[k].ambient);
		glUniform3fv(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_diffuse), 1, &obj->model.materials[k].diffuse.x);
		glUniform3fv(Shaders_GetFUniformLoc(PROGRAM_skinned, FUNIFORM_specular), 1, &obj->model.materials[k].specular.x);

		glDrawElements(GL_TRIANGLES, obj->model.nElements[k], GL_UNSIGNED_SHORT, (void *)(curr * sizeof(GLushort)));
		curr += obj->model.nElements[k];
	}

	glBindVertexArray(0);
}

static void RenderRiggedModelPassthrough(Object *obj){

	glUseProgram(Shaders_GetProgram(PROGRAM_skinned_passthrough));

	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_skinned_passthrough, VUNIFORM_modelInvTranspose), 1, GL_TRUE, obj->matrixInvTrans);

	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_skinned_passthrough, VUNIFORM_model), 1, GL_TRUE, obj->matrix);

	glUniform4fv(Shaders_GetVUniformLoc(PROGRAM_skinned_passthrough, VUNIFORM_bones), 3*obj->skeleton->nBones, &obj->skeleton->matrices[0].x);
	
	glBindVertexArray(obj->model.vao);

	int curr = 0;

	int k;
	for(k = 0; k < obj->model.nMaterials; k++){

		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_skinned_passthrough, FUNIFORM_specularIntensity), obj->model.materials[k].specularIntensity);

		glDrawElements(GL_TRIANGLES, obj->model.nElements[k], GL_UNSIGNED_SHORT, (void *)(curr * sizeof(GLushort)));
		curr += obj->model.nElements[k];
	}

	glBindVertexArray(0);
}

static void RenderModelPassthrough(Object *obj){

	glUseProgram(Shaders_GetProgram(PROGRAM_standard_3d_passthrough));

	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d_passthrough, VUNIFORM_modelInvTranspose), 1, GL_TRUE, obj->matrixInvTrans);

	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d_passthrough, VUNIFORM_model), 1, GL_TRUE, obj->matrix);
	
	glBindVertexArray(obj->model.vao);

	int curr = 0;

	int k;
	for(k = 0; k < obj->model.nMaterials; k++){

		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_standard_3d_passthrough, FUNIFORM_specularIntensity), obj->model.materials[k].specularIntensity);

		glDrawElements(GL_TRIANGLES, obj->model.nElements[k], GL_UNSIGNED_SHORT, (void *)(curr * sizeof(GLushort)));
		curr += obj->model.nElements[k];
	}

	glBindVertexArray(0);
}

static void RenderModel(Renderer *renderer, Object *obj){

	glUseProgram(Shaders_GetProgram(PROGRAM_standard_3d));

	glUniformMatrix4fv(Shaders_GetVUniformLoc(PROGRAM_standard_3d, VUNIFORM_model), 1, GL_TRUE, obj->matrix);

	glActiveTexture(GL_TEXTURE0);
	
	glBindVertexArray(obj->model.vao);

	int curr = 0;

	int k;
	for(k = 0; k < obj->model.nMaterials; k++){
		
		glBindTexture(GL_TEXTURE_2D, obj->model.materials[k].texture);

		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_specularHardness), obj->model.materials[k].specularHardness);
		glUniform1f(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_ambient), obj->model.materials[k].ambient);
		glUniform3fv(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_diffuse), 1, &obj->model.materials[k].diffuse.x);
		glUniform3fv(Shaders_GetFUniformLoc(PROGRAM_standard_3d, FUNIFORM_specular), 1, &obj->model.materials[k].specular.x);

		glDrawElements(GL_TRIANGLES, obj->model.nElements[k], GL_UNSIGNED_SHORT, (void *)(curr * sizeof(GLushort)));
		curr += obj->model.nElements[k];
	}

	glBindVertexArray(0);
}