#ifndef CHAN_DEF
#define CHAN_DEF

int Chan_BeginPost(void);
// int Chan_GetCaptcha(void);
void Chan_SubmitPost(char *comment, char *name);
int Chan_SubmitCaptcha(char *captcha);
int Chan_GetCaptchaTexture(void);
char *Chan_GetCaptchaText(void);
unsigned char *Chan_GetThumbnail(char *tim, int *w, int *h);
int Chan_LoadThread(char *threadnumber, char *buffer);

#endif