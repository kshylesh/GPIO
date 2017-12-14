#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <linux/input.h>
#include <ctype.h>
#include "rgb_header.h"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; //Initailize pthread mutex

void *mouseevent(void *arg){		//Function to detect mouse click
   int fd;
    struct input_event in_eve;//struct to capture input events
   	
   	//check if the mouse file was opened succesfully 
    if((fd = open (EVENT_PATH, O_RDONLY | O_NONBLOCK)) == -1){	//opening the file descriptor for mouse file
      
        perror("Mouse Button:Failed to open the device:");		//Check if the device is connected
        exit(EXIT_FAILURE);
    }

    while(read(fd, &in_eve, sizeof(struct input_event))){   		//continuously read the event for mouse click

        if(in_eve.type == 1 && (in_eve.code == 272 || in_eve.code == 273) && in_eve.value == 1)	//Checking for right click or left click to stop execution
               
            {       
            	pthread_mutex_lock(&lock);	//mutex lock to assign mouse event
            	mouseclick = 1;				//assign mouse click to 1 to stop other loops
            	pthread_mutex_unlock(&lock);//unlock the mutex to free lock
            	break;
            }         
        
     }
     pthread_exit(0); //stop the pthread
}


/*Main thread */
int main(){

	int FD[3],itr;//FD[]array to open 3 gpio files,itr for iterating through 3 gpios 
	char buf[64];//buffer to hold the gpio path

	getIO();	//call getIO function to get useinputs
	IOConfig();	//call IOConfig function to configure GPIO pins
	
	for(itr=0 ; itr<3 ; itr++){
		snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpio_pins[itr]);//openin the corresponding GPIO file
		FD[itr] = open(buf,O_WRONLY);		//open the file and assign the file descriptor to FD
		if(FD[itr]<0)						//Check if the file was opened successfully
			printf("Error opeining file\n");//Error handling for GPIO
	}

	//create pthread mouse for mouse events
	pthread_t mouse;	
	pthread_create(&mouse,NULL,mouseevent,NULL);
	
	//Loop to run continuously with 50ms period between sequencing
	printf("Running:Click mouse button to exit\n");
	while(1)
	{
		if(counter>=8)						//check if the counter fininshed all sequencing
			counter=1;						// Assing 1 to loop back sequencing
		for(itr=0; itr<25; itr++){			//iterate 25 times for one sequence= 25*20000=50ms period
	
		if(counter & RON)					//Apply mask to check if corresponding color needed to be ON
		write(FD[0],"1",1);
		
		if(counter & GON)					//Apply mask to check if corresponding color needed to be ON
		write(FD[1],"1",1);	
	
		if(counter & BON)					//Apply mask to check if corresponding color needed to be ON
		write(FD[2],"1",1);
		
		usleep(intensity * user_pins[3] * 0.01);	//Duty cycle calculation (user_pins[3]:user intensity * 0.01(convert % to decimal))
		
		
		if(counter & RON)					//Apply mask to check if corresponding color needed to be OFF
		write(FD[0],"0",1);
	
		if(counter & GON)					//Apply mask to check if corresponding color needed to be OFF
		write(FD[1],"0",1);

		if(counter & BON)					//Apply mask to check if corresponding color needed to be OFF
		write(FD[2],"0",1);

		usleep(intensity * (1-(user_pins[3] * 0.01)));	//sleep for (1-ontime cycle)
	
		}counter++;						

		if(mouseclick)				//check if mouseclick event has occured
			break;					
			
	}
	
	/*Write 0 to all LEDs*/
	write(FD[0],"0",1);
	write(FD[1],"0",1);	
	write(FD[2],"0",1);
	
	/*close the file descriptor*/
	for(itr=0 ; itr<3 ; itr++)
		close(FD[itr]);
	
	pthread_join(mouse,NULL);	

	return 0;
}
