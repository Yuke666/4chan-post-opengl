#include <GL/glew.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <jerror.h>
#include "memory.h"
#include "deflate.h"

typedef struct {
	int width;
	int height;
	int channels;
	char *pixels;
} Image;

#define BUFFER_SIZE (0x01 << 20)
#define GOOGLE_URL "google.com"
// #define KEY "6Ldp2bsSAAAAAAJ5uyx_lx34lJeEpTLVkP5k04qc"
#define KEY "6LeAfRcTAAAAACbnFgUsHoR92JFxOpU8aXaWLSY7"
#define GOOGLE_IMAGE_URL "/recaptcha/api2/payload?k="KEY"&c="
#define GOOGLE_FALLBACK "/recaptcha/api/fallback?k="KEY
// #define CHAN_POST_SUFFIX "/vg/post"
#define CHAN_URL "sys.4chan.org"
#define A_CHAN_URL "a.4cdn.org"
#define CHAN_IMAGES_URL "i.4cdn.org"
#define GET_PREFIX "/vg/"
#define IMAGE_BUFFER_SIZE 4096

static unsigned int captchaTexture;
static char captchaChallenge[512];
static char captchaText[512];

static int Connect(char *to){

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	struct addrinfo *result;

	int res = getaddrinfo(to, "80", &hints, &result);

	if(res != 0){
		printf("Error: getaddrinfo %s\n", gai_strerror(res));
		return -1;
	}

	int sock = -1;

	struct addrinfo *next = result;

	while(next){

		sock = socket(next->ai_family, next->ai_socktype, next->ai_protocol);

		if(sock < 0)
			continue;

		if(connect(sock, next->ai_addr, next->ai_addrlen) != -1)
			break;

		next = next->ai_next;
	}

	freeaddrinfo(result);

	if(sock < 0){
		printf("Error: connecting\n");
		return -1;
	}

	return sock;
}

static void WriteGetHeader(char *host, char *referer, int sock, char *format, ...){

	char buffer[512];

	va_list args;
	va_start(args, format);

	vsnprintf(buffer, sizeof(buffer), format, args);

	va_end(args);

	char header[1024];

	char *text = "GET %s HTTP/1.1\r\n"
					"Host: %s\r\n"
					"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:43.0) Gecko/20100101 Firefox/43.0\r\n"
					"Accept: */*\r\n"
					"Accept-Encoding: */*\r\n"
					"Referer: http://www.%s\r\n"
					// "Connection: keep-alive\r\n"
					"\r\n\r\n\0";


    sprintf(header, text, buffer, host, referer);

	write(sock, header, strlen(header) + 1);
}

static void LoadJPEG(Image *img, unsigned char *buffer, int bufferLen){
    
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr jerror;
    
    info.err = jpeg_std_error(&jerror);

    jpeg_create_decompress(&info);

	jpeg_mem_src(&info, buffer, bufferLen);
    jpeg_read_header(&info, TRUE);
    jpeg_start_decompress(&info);

    img->pixels = NULL;
    img->width =  info.output_width;
    img->height = info.output_height;
    img->channels = info.num_components;
    // img->channels = 3;

    int data_size = img->width * img->height * info.num_components;

    img->pixels = (char *)Memory_StackAlloc(MAIN_STACK, sizeof(char) * data_size);
    
    char *rowptr;
    
    while(info.output_scanline < info.output_height){

        rowptr = (char *)img->pixels + img->channels * info.output_width * info.output_scanline;

        jpeg_read_scanlines(&info, (JSAMPARRAY)&rowptr, 1);
    }

    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
}

static int ReadChunked(int sock, char *into){

	char *init = into;

	char buffer[BUFFER_SIZE];

	int len = read(sock, buffer, BUFFER_SIZE);

	if(len <= 0) return 0;

	char *pos = strstr(buffer, "\r\n\r\n");

	if(!pos) return 0;

	pos += 4;

	int packet;
	sscanf(pos, "%x", &packet);

	printf("%i\n",packet);

	pos = strstr(pos, "\r\n");

	if(!pos) return 0;

	pos += 2; // skip \r\n

	len -= (pos - buffer);

	while(1){

		if(packet < len){

			memcpy(into, pos, packet);

			into += packet;

			pos = buffer + packet + 2;

			sscanf(pos, "%x", &packet);

			printf("%i\n",packet );

			if(!pos) break;
			if(!packet) break;
		
			if(*pos == '\r') pos += 2; // skip \r\n

			len -= (pos - buffer);
	
			if(len < 0) break;
		}

		memcpy(into, pos, len);

		into += len;

		if(!packet) break;

		packet -= len;

		if(packet < 0)
			break;

		len = read(sock, buffer, BUFFER_SIZE);

		if(len <= 0) break;

		pos = buffer;
	}

	return into - init;
}

static char *ReadContent(int sock, int *contentLen){

	char buffer[BUFFER_SIZE];

	int len = read(sock, buffer, BUFFER_SIZE);

	char *search = "Content-Length: ";

	char *pos = strstr(buffer, search);

	if(!pos){
		close(sock);
		printf("Error\n");
		return NULL;
	}

	pos += strlen(search);

	sscanf(pos, "%i", contentLen);

	search = "\r\n\r\n";

	pos = strstr(buffer, search);

	pos += strlen(search);

	len -= pos - buffer;

	char *ret = (char *)Memory_StackAlloc(TEMP_STACK, *contentLen);

	memcpy(ret, pos, len);

	pos = ret + len;

	while(1){
	
		len = read(sock, pos, BUFFER_SIZE);
	
		if(len <= 0) break;

		pos += len;

		if(pos >= ret + *contentLen)
			break;
	}

	return ret;
}

int Chan_BeginPost(void){

	int sock, contentLen;
	char *end, *search, *pos;
	char buffer[BUFFER_SIZE];

	// get challenge.

	sock = Connect(GOOGLE_URL);

	if(sock < 0) return -1;

	WriteGetHeader(GOOGLE_URL, GOOGLE_FALLBACK, sock, "%s", GOOGLE_FALLBACK);

	ReadChunked(sock, buffer);

	close(sock);

	search = "\"c\" value=\"";
	pos = strstr(buffer, search);
	pos += strlen(search);
	end = strchr(pos, '"');
	*end = 0;
	strcpy(captchaChallenge, pos);

	search = "class=\"fbc-imageselect-message-text\">";
	pos = strstr(buffer, search);
	pos += strlen(search);
	search = "</label>";
	end = strstr(pos, search);
	*end = 0;

	strcpy(captchaText, pos);

	// it's chunked, but the challenge is in the first packet

	// get image.

	sock = Connect(GOOGLE_URL);

	if(sock < 0) return -1;

	WriteGetHeader(GOOGLE_URL, GOOGLE_FALLBACK, sock, "%s%s", GOOGLE_IMAGE_URL, captchaChallenge);

	unsigned char *image = (unsigned char *)ReadContent(sock, &contentLen);

	close(sock);

	Image img;

	LoadJPEG(&img, image, contentLen);
	
	Memory_StackPop(TEMP_STACK, 1);

    glGenTextures(1, &captchaTexture);
    glBindTexture(GL_TEXTURE_2D, captchaTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width, img.height, 0, GL_RGB, GL_UNSIGNED_BYTE, img.pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

	Memory_StackPop(MAIN_STACK, 1);

    return 1;
}
/*

final post:

params:
	k: 6Le-wvkSAAAAAPBMRTvw0Q4Muexq9bi0DJwx_mJ-

Accept	
Accept-Encoding	
gzip, deflate
Accept-Language	
en-US,en;q=0.5
Connection	
keep-alive
Cookie	
NID=76=N9yJHZeMNB2abQVsfCmOakIdWqV0oVJhOIwqJ5GFRL2ICWQrNd0-52kqTrkxhG6NzmczwPcrZ5WCjaVS7TOCjl_0EZgMgH8Ob8B7u21l6HpnvPyocquwKUIFz6XRKcPwLKBt6aioxrG3pd3i-Ow1Fb3CI1lnJX9lbqbRB6D3OVbrzw
; OGPC=5061451-14:5061821-9:; OGP=-5061451:-5061821:; _ga=GA1.1.1206350322.1454608391
Host	
www.google.com
Referer	
https://www.google.com/recaptcha/api/fallback?k=6Le-wvkSAAAAAPBMRTvw0Q4Muexq9bi0DJwx_mJ-
User-Agent	
Mozilla/5.0 (X11; Linux x86_64; rv:43.0) Gecko/20100101 Firefox/43.0

Content-Length: 402
Content-Type: application/x-www-form-urlencode

c:
03AHJ_VutEgz-t6DR3E3r6CaGSKW0Iz2z94SKhwFfVXVFab03Ft2xJhxZkT5PHSTLJ8IY8yXYpH0uUqvQrWVEB0A0meEwUQL3dhP
U1TVPdSOMoJsUQQGtkOOVOgepNpdi-J-5ec1sWnF_KW2o0S8mB_6ONy2WvMXfxk_BOhpRxPlwH20OhoM_mvuaOnM__2r7oamcD19
9VLxmBJwRj6k0_7VjtxRaXpdHSV9Vx9Xnp22D36XSJ1PTo-NUvJPlA8_DFxofzIsD7vjoGNLfLFTuIyoPZdP-30oBzzHsW6wBTMm
38yPYKWvwCGTX8rnwczVCTVLqVMqBTXYk5WyouRw5HVUBF4XcHZKxo5Q
response: 1
response: 3
response: 4
response: 6

c=03AHJ_VutEgz-t6DR3E3r6CaGSKW0Iz2z94SKhwFfVXVFab03Ft2xJhxZkT5PHSTLJ8IY8yXYpH0uUqvQrWVEB0A0meEwUQL3d
hPU1TVPdSOMoJsUQQGtkOOVOgepNpdi-J-5ec1sWnF_KW2o0S8mB_6ONy2WvMXfxk_BOhpRxPlwH20OhoM_mvuaOnM__2r7oamcD
199VLxmBJwRj6k0_7VjtxRaXpdHSV9Vx9Xnp22D36XSJ1PTo-NUvJPlA8_DFxofzIsD7vjoGNLfLFTuIyoPZdP-30oBzzHsW6wBTMm38yPYKWvwCGTX8rnwczVCTVLqVMqBTXYk5WyouRw5HVUBF4XcHZKxo5Q
&response=1&response=3&response=4&response=6

*/

char *Chan_GetCaptchaText(void){
	return captchaText;
}

int Chan_GetCaptchaTexture(void){

	return captchaTexture;
}

int Chan_SubmitCaptcha(char *captcha){

	char post[512];

	char *pos = post + sprintf(post, "c=%s", captchaChallenge);


	int k;
	for(k = 0; k < (int)strlen(captcha); k++){

		pos += sprintf(pos, "&response=%c", captcha[k]);
	}

	char header[2048];

	char *text = "POST %s HTTP/1.1\r\n"
					"Host: www.google.com\r\n"
					"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:43.0) Gecko/20100101 Firefox/43.0\r\n"
					"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
					"Accept-Language=en-US,en;q=0.5\r\n"
					"Accept-Encoding: gzip, deflate\r\n"
					"Referer: http://www.%s%s\r\n"
					// "Connection: keep-alive\r\n"
					"Content-Type: application/x-www-form-urlencoded\r\n"
					"Content-Length: %i\r\n"
					"\r\n%s";


    sprintf(header, text, GOOGLE_FALLBACK, GOOGLE_URL, GOOGLE_FALLBACK, strlen(post), post);

    int len = strlen(header);

	int sock = Connect(GOOGLE_URL);

	write(sock, header, len);

	// love this gzip shit.
	int htmlLen;
	char *html = ReadContent(sock, &htmlLen);

	Memory_StackPop(TEMP_STACK, 1);

	close(sock);

	return 1;
}

// make sure you switch key to 4chans.
// make sure you switch key to 4chans.
// make sure you switch key to 4chans.
// make sure you switch key to 4chans.
// make sure you switch key to 4chans.
// make sure you switch key to 4chans.
// make sure you switch key to 4chans.
// make sure you switch key to 4chans.

void Chan_Post(){

	// char post[512];
	// memset(post, 0, sizeof(post));

	// printf("%s\n",captchaChallenge );
	// printf("%s\n",captcha );

	// sprintf(post, "recaptcha_challenge_field=%s"
	// 				"&recaptcha_response_field=%s"
	// 				"&com=%s"
	// 				"&email="
	// 				"&name="
	// 				"&pwd="
	// 				"&resto=130978176"
	// 				"&mode=regist", captchaChallenge, captcha);


	// int postLen = strlen(post);

	// int k;
	// for(k = 0; k < postLen; k++)
	// 	if(post[k] == ' ')
	// 		post[k] = '+';

	// printf("%s\n", post);

	// char header[1024];

	// char *text = "POST %s HTTP/1.1\r\n"
	// 				"Host: %s\r\n"
	// 				"User-Agent: Mozilla/5.0\r\n"
	// 				"Accept: */*\r\n"
	// 				"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
	// 				"Accept-Language: en-US,en;q=0.5\r\n"
	// 				"Accept-Encoding: gzip, deflate\r\n"
	// 				"Referer:  http://www.%s\r\n"
	// 		        // "Content-Type: multipart/form-data\r\n"
	// 				"Connection: keep-alive\r\n"
	// 		        "Content-Type: application/x-www-form-urlencoded\r\n"
	// 				"Content-Length: %i\n"
	// 				"\r\n\r\n%s\0";


 //    sprintf(header, text, CHAN_POST_SUFFIX, CHAN_URL, CHAN_URL, postLen, post);

 //    printf("%s\n\n%i\n\n",header, (int)strlen(header));

 //    int len = strlen(header);

 //    header[len] = 0;

	// // int sock = Connect(CHAN_URL);

	// // if(sock < 0) return -1;

	// // write(sock, header, len + 1);

	// // char buffer[BUFFER_SIZE];

	// // memset(buffer, 0, sizeof(buffer));

	// // // len = read(sock, buffer, BUFFER_SIZE);

	// // ReadChunked(sock, buffer);

	// // puts(buffer);

	// // close(sock);

	// return 1;
}


void WriteHeader(int sock, char *format, ...){

	char buffer[128];

	memset(buffer, 0, sizeof(buffer));

	va_list args;
	va_start(args, format);

	vsnprintf(buffer, sizeof(buffer), format, args);

	va_end(args);

	char header[2048];

	memset(header, 0, sizeof(header));

	char *text = "GET %s HTTP/1.1\n"
					"Host: %s\n"
					"User-Agent: Mozilla/5.0\n"
					"Accept: */*\n"
					"Accept-Encoding: gzip, deflate\n"
					"Connection: keep-alive\n"
					"\r\n\r\n\0";

    sprintf(header, text, buffer, CHAN_URL);

	write(sock, header, strlen(header) + 1);
}

unsigned char *Chan_GetThumbnail(char *tim, int *w, int *h){

	int sock = Connect(CHAN_IMAGES_URL);

	if(sock <= 0) return NULL;

	WriteGetHeader(CHAN_IMAGES_URL, "", sock, "%s%ss.jpg", GET_PREFIX, tim);

	int contentLen;
	unsigned char *image = (unsigned char *)ReadContent(sock, &contentLen);

	close(sock);

	if(!image) return NULL;

	Image img;

	LoadJPEG(&img, image, contentLen);

	Memory_StackPop(TEMP_STACK, 1);

	*w = img.width;
	*h = img.height;

	printf("%i\n",img.channels );

	return (unsigned char *)img.pixels;
}

int Chan_LoadThread(char *threadnumber, char *buffer){

	int sock = Connect(A_CHAN_URL);

	if(sock <= 0) return 0;

	WriteGetHeader(A_CHAN_URL, "", sock, "%sthread/%s.json", GET_PREFIX, threadnumber);

	int ret = ReadChunked(sock, buffer);

	close(sock);

    return ret;
}