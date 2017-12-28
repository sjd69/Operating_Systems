//Written by Stephen Dowhy
//Cs1550 Project 1
//Library with various drawing functionality.

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <termios.h>
#include "iso_font.h"

//Unsigned 16-bit value that holds three encoded RGB values
typedef unsigned short color_t;

//Globals
//framebuffer holds file descriptor
int framebuffer;

//Screen information
int screen_size;
color_t* m_map;
struct fb_var_screeninfo var_screeninfo;
struct fb_fix_screeninfo fix_screeninfo;

//Holds terminal settings
struct termios term;

//Function prototypes
void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void fill_circle(int x, int y, int r, color_t c);
void draw_character(int x, int y, color_t c, int val);
void draw_text(int x, int y, const char *text, color_t c);


//Initializes graphics functionality.
void init_graphics()
{
	//"opens" framebuffer. sets it to the file descriptor.
	framebuffer = open("/dev/fb0", O_RDWR);
	
	//Detects and sets screen resolution
	ioctl(framebuffer, FBIOGET_VSCREENINFO, &var_screeninfo);
	ioctl(framebuffer, FBIOGET_FSCREENINFO, &fix_screeninfo);
	
	//Save screen size for later use.
	screen_size = var_screeninfo.yres_virtual * fix_screeninfo.line_length;
	
	//Saves a void* in our address space so we can set individual pixels.
	m_map = mmap(NULL, screen_size, PROT_WRITE, MAP_SHARED, framebuffer, 0);
	
	//Unsets canonical mode and disables echo
	ioctl(STDIN_FILENO, TCGETS, &term);
	term.c_lflag &= ~ICANON;
	term.c_lflag &= ~ECHO;
	ioctl(STDIN_FILENO, TCSETS, &term);
}

//Re-enables functionality and closes/unmaps un-needed values
void exit_graphics()
{
	
	//Close framebuffer
	close(framebuffer);
	
	//unmaps the memory address
	munmap(m_map, screen_size);
	
	//Re-enables canonical mode and echo
	ioctl(STDIN_FILENO, TCSETS, &term);
	term.c_lflag |= ICANON;
	term.c_lflag |= ECHO;
	ioctl(STDIN_FILENO, TCSETS, &term);
}

//Wipes the screen for drawing
void clear_screen()
{
	//Clears screen using ANSI escape code
	write(STDIN_FILENO, "\033[2J", 8);
}

//Reads char from user input
char getkey() 
{
	
	struct timeval time_val;
	int val;
	fd_set rfds;
	
	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	
	time_val.tv_sec = 10;
	time_val.tv_usec = 0;
	val = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &time_val);
	
	if (val > 0) 
	{
		char input;
		read(STDIN_FILENO, &input, sizeof(char));
		return input;
	}
	else
	{
		return NULL;
	}
	
}

void sleep_ms(long ms)
{
	struct timespec time_spec;
	time_spec.tv_sec = (ms / 1000);
	time_spec.tv_nsec = (ms * 1000000);

	nanosleep(&time_spec, NULL);
}

void draw_pixel(int x, int y, color_t color) 
{
	*(m_map + (y * var_screeninfo.xres_virtual) + x) = color;
}

void draw_rect(int x1, int y1, int width, int height, color_t c)
{
	int max_y = (y1 + height);
	int max_x = (x1 + width);
	int x;
	int y;
	
	for (x = x1; x < max_x; x++) 
	{
		for (y = y1; y < max_y; y++) 
		{
			if (x == x1 || x == (max_x - 1))
			{
				draw_pixel(x, y, c);
			}
			
			if (y == y1 || y == (max_y - 1))
			{
				draw_pixel(x, y, c);
			}
		}
	}
}

//Users mid-point circle algorithm which draws a circle at x and y with radius r
//then fills it inward.
void fill_circle(int x, int y, int r, color_t c)
{
	int x1;
	int y1;
	int i;
	for (i = r; i > 0; i--)
	{
		y1 = 0;
		x1 = i;
		int decision_over_2 = 1 - x1;
		
		while (y1 <= x1)
		{
			draw_pixel( x1+x, y1+y, c);
			draw_pixel( y1+x, x1+y, c);
			draw_pixel( -x1+x, y1+y, c);
			draw_pixel( -y1+x, x1+y, c);
			draw_pixel( -x1+x, -y1+y, c);
			draw_pixel( -y1+x, -x1+y, c);
			draw_pixel( x1+x, -y1+y, c);
			draw_pixel( y1+x, -x1+y, c);
			
			y1++;
			if (decision_over_2 <= 0)
			{
				decision_over_2 += 2 * y1 + 1;
			}
			else
			{
				x1--;
				decision_over_2 += 2 * (y1 - x1) +1;
			}
		}
	}
	
}

void draw_character(int x, int y, color_t c, int val)
{
	int i;
	int j;
	
	for (i = 0; i < ISO_CHAR_HEIGHT; i++)
	{
		char byte = iso_font[(val * 16) + i];
		
		for (j = 0; j < 8; j++)
		{
			char result = ((byte & 1 << j) >> j);
			
			if ((int) result == 1)
			{
				draw_pixel(x + j, y + i, c);
			}
		}
	}
}
void draw_text(int x, int y, const char *text, color_t  c)
{
	const char* text_ptr;
	int offset = 0;
	
	for (text_ptr = text; *text_ptr != '/0'; text_ptr++)
	{
		draw_character(x, (y + offset), c, *text_ptr);
		offset += 8;
	}
}

