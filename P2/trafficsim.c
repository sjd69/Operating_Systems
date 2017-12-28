#include <linux/unistd.h>
#include <linux/mman.h>
#include <stdlib.h>
#include <stdio.h>

struct sem_node {
	struct task_struct* task;
	struct node* next;
};

struct cs1550_sem {
	int value;
	struct sem_node* head;
	struct sem_node* tail;
};

void down(struct cs1550_sem* sem) {
	syscall(__NR_cs1550_down);
}

void up(struct cs1550_sem* sem) {
	syscall(__NR_cs1550_up);
}

int main(void) {
	//Allocate space in the RAM for 3 semaphores
	void* sem_ptr = (void*) mmap(NULL, sizeof(struct cs1550_sem) * 3, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	
	//Split the allocated share space into 3 portions.
	//First portion for the north, second for the south, third for the mutex
	struct cs1550_sem* north = (struct cs1550_sem*) sem_ptr;
	struct cs1550_sem* south = (struct cs1550_sem*) (sem_ptr + 1);
	struct cs1550_sem* mutex = (struct cs1550_sem*) (sem_ptr + 2);
	
	//Initialize semaphores to default values
	north->value = 1;
	north->head = NULL;
	north->tail = NULL;
	
	south->value = 1;
	south->head = NULL;
	south->tail = NULL;
	
	mutex->value = 1;
	mutex->head = NULL;
	mutex->tail = NULL;
	
	void* count_ptr = (void*) mmap(NULL, sizeof(int)*3, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	int* car_count_ptr = (int*) count_ptr;
	int* north_car_ptr = (int*) (count_ptr + 1);
	int* south_car_ptr = (int*) (count_ptr + 2);
	*car_count_ptr = 0;
	*north_car_ptr = *south_car_ptr = 1;
	
	printf("Starting traffic sim...\n");
	
	//North producer
	if (fork() == 0) {
		printf("North producer created\n");
		
		System.out.println("CAR PTR = %d\n", *car_count_ptr);
		while(1) {
			down(north);
			down(mutex);
			(*car_count_ptr)++;
			(*north_car_ptr++);
			
			if ((*north_car_ptr == 1) && (*south_car_ptr == 1)) {
				printf("Car %d coming from the north direction blew their horn at time \n", *car_count_ptr);
			} else {
				printf("\n\n%d\n\n", *north_car_ptr);
				printf("Car %d coming from the north arrived in the queue at time \n", *car_count_ptr);
			}
			while((rand() % 10) < 8) {
				down(north);
				(*car_count_ptr)++;
				(*north_car_ptr++);
				//printf("Car %d coming from the north arrived in the queue at time \n", *car_count_ptr);
			}
			printf("No more cars from the north.\n");
			up(mutex);
			sleep(20);
		}
	}
	
	//If south producer, else the consumer (flagman)
	if (fork() == 0) {
		printf("South producer created\n");
		
		while(1) {
			down(south);
			down(mutex);
			(*car_count_ptr)++;
			(*south_car_ptr++);
			
			if ((*north_car_ptr == 1) && (*south_car_ptr == 1)) {
				printf("Car %d coming from the south direction blew their horn at time \n", *car_count_ptr);
			} else {
				printf("\n\n%d\n\n", *south_car_ptr);
				printf("Car %d coming from the south arrived in the queue at time \n", *car_count_ptr);
			}
			while((rand() % 10) < 8) {
				down(south);
				(*car_count_ptr)++;
				(*south_car_ptr++);
				printf("Car %d coming from the south arrived in the queue at time \n", *car_count_ptr);
			}
			printf("No more cars from the south.\n");
			up(mutex);
			sleep(20);
		}
	} else {
		int dir_of_pass = 0;
		int car_counter = 0;
		int is_awake = 0;
		
		while(1) {
			if ((*north_car_ptr <= 1) && (*south_car_ptr == 1)) {
				printf("Flagperson is now awake.\n");
				is_awake = 1;
				dir_of_pass = 1;
			} else if ((*south_car_ptr <= 1) && (*north_car_ptr == 1)) {
				printf("Flagperson is now awake.\n");
				is_awake = 1;
				dir_of_pass = 2;
			}
			
			while (is_awake == 1) {
				down(mutex);
				
				if (*north_car_ptr <= -9) {
					printf("test north -9\n");
					dir_of_pass = 1;
				} else if (*south_car_ptr <= -9) {
					printf("test south -9\n");
					dir_of_pass = 2;
				} else if ((dir_of_pass == 1) && (*north_car_ptr == 1)) {
					printf("test north to south\n");
					dir_of_pass = 2;
				} else if ((dir_of_pass == 2) && (*south_car_ptr == 1)) {
					printf("test south to north\n");
					dir_of_pass = 1;
				}
				
				if ((dir_of_pass == 1) && (*north_car_ptr <= 0)) {
					car_counter++;
					(*north_car_ptr++);
					printf("Car %d from the north has left the construction zone", car_counter);
					up(north);
				} else if ((dir_of_pass == 2) && (*south_car_ptr <= 0)) {
					car_counter++;
					(*south_car_ptr++);
					printf("Car %d from the south has left the construction zone", car_counter);
					up(south);
				}
				
				if ((*north_car_ptr == 1) && (*south_car_ptr == 1)) {
					printf("Flagperson is now asleep.");
					is_awake = 0;
					up(mutex);
				} else {
					up(mutex);
					sleep(2);
				}
				
				
			}
			
		}
	}
	
}