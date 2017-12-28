//Written By Stephen Dowhy
//CS1550 Project 1
//Driver program to demonstrate library functionality.
//User can continuously enter 1-3 to do various functions.
//Ctrl+C to exit loop.

#include <stdio.h>
#include <stdlib.h>

typedef unsigned short color_t;


int main (int argc, char** argv)
{
	char buffer[256];
	int buffer_size = 256;
	int user_choice;
	
	printf("Enter 1 to draw a rectangle.\n");
	printf("If a rectangle is drawn, it can be moved with WASD\n");
	printf("Enter 2 to draw a filled circle.\n");
	printf("Enter 3 to draw a string.\n");
	
	while (fgets(buffer, buffer_size, stdin))
	{
		user_choice = atoi(buffer);
			
		init_graphics();
		clear_screen();
		if (user_choice == 1)
		{
			int x = 320;
			int y = 240;
			draw_rect(x, y, 150, 50, 15);
			
			while (1)
			{
				char key = getkey();
				
				if(key == 'w') 
				{
					y -= 5;
				}
				else if(key == 's') 
				{
					y += 5;
				}
				else if(key == 'a') 
				{
					x -= 5;
				}
				else if(key == 'd') 
				{
					x += 5;
				}
				else
				{
					break;
				}
				
				clear_screen();
				draw_rect(x, y, 200, 100, 20);
				sleep_ms(20);
				
			};
	  
		}
		else if (user_choice == 2)
		{
			fill_circle(320, 240, 150, 15);
		}
		else if (user_choice == 3)
		{
			char string_buffer[256];
			
			printf("\nEnter a string.\n");
			fgets(string_buffer, buffer_size, stdin);
			
			//Currently seg faults after running. Don't know what I changed to break it.
			draw_text(320, 240, string_buffer, 15);
		}
		
		sleep_ms(20);
		exit_graphics();
		
		printf("\nEnter 1 to draw a rectangle.\n");
		printf("Enter 2 to draw a filled circle.\n");
		printf("Enter 3 to draw a string.\n");
	}
	
}