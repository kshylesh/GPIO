#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "rgb_header.h"


int counter = 0;//global counter to keep track of sequencing.

int mouseclick = 0;//global variable to check if mouseclick has happened

//pin_sel table list all the available gpio pins for multiplexing
//Rows indicate number of IO pins(IO0-IO13)
//Columns indicate corresponding value gpio,direction gpio,MUX1 gpio,MUX2 gpio,flag(to check if mux2 exists)
unsigned int pin_sel [][5]={{11,32,0,0,0},		//GPIO pin mapping to program IO0
						{12,28,45,0,0},			//GPIO pin mapping to program IO1
						{61,34,77,0,0},			//GPIO pin mapping to program IO2	
						{62,16,76,64,1},		//GPIO pin mapping to program IO3
						{6,36,0,0,0},			//GPIO pin mapping to program IO4
						{0,18,66,0,0},			//GPIO pin mapping to program IO5
						{1,20,68,0,0},			//GPIO pin mapping to program IO6
						{38,0,0,0,0},			//GPIO pin mapping to program IO7
						{40,0,0,0,0},			//GPIO pin mapping to program IO8
						{4,22,70,0,0},			//GPIO pin mapping to program IO9
						{10,26,74,0,1},			//GPIO pin mapping to program IO10
						{5,24,44,72,0},			//GPIO pin mapping to program IO11
						{15,42,0,0,0},			//GPIO pin mapping to program IO12
						{7,30,46,0,0},			//GPIO pin mapping to program IO13
						};

/****************************************************************
 * gpio_export
 ****************************************************************/
int gpio_export(unsigned int gpio)
{
	int fd, len;									//fd:file descriptor and len:length of gpio pin number
	char buf[64];									//buffer to hold the path name
 
	fd = open("/sys/class/gpio/export", O_WRONLY);	//open the corresponding file
	if (fd < 0) {									//Check if file open was successful
		perror("gpio/export");						//Error checking
		return fd;
	}	
 	
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);							//export the gpio specified in buf
	close(fd);
 
	return 0;
}


/****************************************************************
 * Get IO
 ****************************************************************/
void getIO()						//Function to configure user inputs with error handling
{
	char num[256];					//char to scan inputs
	unsigned long int temp=0;		//temporary variable for error checking
	int i,len;						//iteration variables
	printf("Enter 3 pins and duty cycle(respectively) to configure(0-13)\n");
	for (i=0;i<4;i++)									//scan for 4 inputs: 3 GPIO pins and 1 intensity pin respectively
	{
		fgets(num, 256, stdin);			

		if(strlen(num)-1 > 3){							//Error handling if scanned input length is greater than 3
			printf("Error:Enter only integer[0-13]\n"); 
			i--;										//decrement the iteration to discard duplicate
			continue;

		}
		
		for(len=0 ; len<strlen(num)-1; len++) 			//convert the string input to int by scanning all the characters
			temp=temp * 10 + (num[len] - '0');			//Convert string to corresponding integer values

		if(i==1){										//Check if the user has entered duplicate user pin
			if(user_pins[i-1]==temp){					//Check if the new pin is already configured .
				printf("Error:Pin already configured\n");
				i--;									//decrement the iteration to discard duplicate
			}
		}

		if(i==2){											//Check if the user has entered duplicate user pin
			if(user_pins[i-1] == temp || user_pins[i-2] == temp){ //Check if the new pin is already configured.
				printf("Error:Pin already configured\n");
				i--;										//decrement the iteration to discard duplicate
			}
		}
		if((i < 3) && (temp<0 || temp>13)){					//Check if the user enters input > 13 or < 0
			printf("Error:Invalid Pin Try Again\n");
			temp=0;											//initialize temp to 0 to hold next input
			i--;											//decrement the iteration to discard duplicate
			}

		else if((i == 3) && (temp < 0 || temp > 100)){		//Check if user entered invalid intensity < 0 or > 100
			printf("Error:Invalid Intensity\n");			
			temp=0;											//initialize temp to 0 to hold next input
			i--;											//decrement the iteration to discard duplicate
			}

		else{
			user_pins[i]=temp;								//Assign temporary varialbe to global user_pins array
			temp=0;											//Initialize temp back to 0 to hold next input 
		}
	}
return;
}




/****************************************************************
 * CONFIG IO
 ****************************************************************/
void IOConfig(void)
{	
	int clm,row;	//clm and row to scan the pin_sel table
	int fd;			//file descriptor to open file
	char buf[100];	//buffer to hold path of gpio pins
	for(row=0;row<3;row++){

				clm=0;

				if(pin_sel[user_pins[row]][clm]>=0){		
					gpio_export(pin_sel[user_pins[row]][clm]);			//Calls gpio_export function to export the corresponding input gpio pin
			
					snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", pin_sel[user_pins[row]][clm]);//Set the direction of the gpio pin
					fd = open(buf, O_WRONLY);
					if(fd<0)									//Check if the file descriptor returns error
						printf("Error setting direction\n");	//Error checking for direction
					write(fd,"out",3);							//if no error assign the direction as output
					
					gpio_pins[row]=pin_sel[user_pins[row]][clm];		//Save the list of gpio inputs 
					
					clm++;		//move to direction pin
				}
				
				else
					clm++;

				if(pin_sel[user_pins[row]][clm]!=0){					//Check if direction pin exists
					gpio_export(pin_sel[user_pins[row]][clm]);			//Setting Corresponding direction GPIO
					
					snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", pin_sel[user_pins[row]][clm]);
					fd = open(buf, O_WRONLY);						//open the corresponding direction GPIO file
						if(fd<0)									//Check if the file descriptor returns error
							printf("Error opening direction pin\n");//Error checking for direction pin
					write(fd,"out",3);		//if no error set the direction pin to output
					
					snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", pin_sel[user_pins[row]][clm]);
					fd = open(buf, O_WRONLY);					//open the corresponding value GPIO file
						if(fd<0)							//Check if the file descriptor returns error
						printf("Error Setting value\n");	//Error checking for setting value
					write(fd,"0",1);						//Set the GPIO direction pin value to 1
					
					clm++;	//move to mux1 pin
				}
				
				else
					clm++;

				if(pin_sel[user_pins[row]][clm]!=0){					//Check if mux1 pin exists
					gpio_export(pin_sel[user_pins[row]][clm]);			//Setting Corresponding direction GPIO
					
					snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", pin_sel[user_pins[row]][clm]);
					fd = open(buf, O_WRONLY);							//Open the file specified by th path buf
						if(fd<0)										//Check if the file descriptor returns error
						printf("Error Settin Mux1 Value\n");			//Error checking while setting mux1 value
					write(fd,"0",0);									//Set the MUX gpio pin value to 0 (as per the gpio configuration)
						
					clm++;	//move to mux2 pin
				}
				else
					clm++;

				if(pin_sel[user_pins[row]][clm]!=0){					//Check if mux2 pin exists
					gpio_export(pin_sel[user_pins[row]][clm]);			//Setting Corresponding direction GPIO

					snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", pin_sel[user_pins[row]][clm]);
					fd = open(buf, O_WRONLY);							//Open the file specified by th path buf
					if(fd<0)											//Check if the file descriptor returns error
						printf("Error Setting Mux2 Value\n");			//Error Checking while setting mux2 value
					write(fd,"0",0);									//Set the MUX gpio pin value to 0 (as per the gpio configuration)
				}
				close(fd);											//close file descriptor
			}
			return;

}