#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<linux/input.h>
#include"rgb_ioctl.h"
int mouseclick=0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; //Initailize pthread mutex

void *mouseevent(void *arg){		//Function to detect mouse click
   int fd;
    struct input_event in_eve;//struct to capture input events
   	
   	//check if the mouse file was opened succesfully 
    if((fd = open ("/dev/input/event2", O_RDONLY | O_NONBLOCK)) == -1){	//opening the file descriptor for mouse file
      
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
     printf("Mouse click detected ,thread exiting\n");
     pthread_exit(0); //stop the pthread
}

int set_vars(int fd) //function to get user inputs
{
    int v;
    
    settings s; //Settings file to hold the user inputs
 
    printf("Enter Duty cycle: \n");
    scanf("%d", &v);
    
    s.duty_cycle = v;  //Assign duty cycle
    
    
    printf("Enter Red pin: \n");
    scanf("%d", &v);
    s.red_pin = v; //Assign Red pin
    printf("Enter green pin: \n");
    scanf("%d", &v);
    s.green_pin=v; //Assign Green pin
    printf("Enter blue pin: \n");
    scanf("%d", &v);
    s.blue_pin=v; //Assign Blue pin
     
        if (ioctl(fd, CONFIG_PINS, &s) == -1) //Pass the setting file to kernel space and check if there was an IO error
        {
            perror("query_apps ioctl set");
            return -1; //Return -1 while setting configuration in kernel space
        }
   return 0; 
}

int main(void)
{
    int fd,status,ret,option;
    fd=open("/dev/RGBled",O_RDWR);
    if(fd<0)
    printf("Error while opening the file\n");
    pthread_t mouse;	 //Create pthread for mouse
	pthread_create(&mouse,NULL,mouseevent,NULL); //start the mouse event thread parallely
    while(1) //Infinety run the program until mouse event and exit option
    {
             
            	pthread_mutex_lock(&lock);	//mutex lock to assign mouse event
            	if(mouseclick==1)			//Check if the mouse button was clicked
            	{
            	    pthread_mutex_unlock(&lock);//mutex unlock if condition satisfied
            	    break;   
            	}
            	else				
            	pthread_mutex_unlock(&lock);	//mutex unlock if condition not satisfied
           
       
        //Interactive options to run the application  
        printf("Select any option\n");			
        printf("1:Run old configuration\n");
        printf("2:Make a new configuration and run\n");
        printf("Press any number other than 1 and 2 if mouse is clicked\n"); 
        scanf("%d",&option);
        switch(option)
          {
            case 1: //If case 1 is selected run the saved configuration
                  enable:   printf("Which all pins need to be enabled\n");//Get the bits to be set to blink LED
                            scanf("%d",&status);
                    if((status>7)||(status<0)) //If the bits are >7 or <0 throw error
                    {
                        printf("Invalid option try once more\n");
                        goto enable; //Prompt user to try again
                        }
                    else  
                    ret=write(fd,(char *)&status,sizeof(int)); //Call write function to update the bit information

                    if(ret<0) //If the file descriptor failed to open throw error
                        printf("Error while writing to the file\n");
                    break;
            case 2: //If case 2 is selected load the new configuration file
                config: printf("Enter new configuration details\n");
                    ret=set_vars(fd);  //Call the function to prompt for user inputs
                    
                    if(ret!=0) //Check the return value of ser_vars if the return value is not 0 the settings are invalid
                    {
                        printf("Invalid configuration inputs,enter again\n");
                        goto config; //prompt user to enter again
                     }
                     else
                     {
                     printf("New configuration successfully set\n");
             enable_config:        printf("Which all pins need to be enabled\n"); //Get the bits to be set to blink LED
                                   scanf("%d",&status);
                    if((status>7)||(status<0)) //If the bits are >7 or <0 throw error
                    {
                        printf("Invalid option try once more\n");
                        goto enable_config; //Prompt user to try again
                        }
                    else
                    ret=write(fd,(char *)&status,sizeof(int));//Call write function to update the bit information
                    if(ret<0)  //If the file descriptor failed to write the settings throw error
                    {
                        printf("Error while writing to the file\n");
                        goto config;//Go back to configuration details for new inputs
                        }
                    else 
                        printf("New configuration running successfully\n"); //If no error then all settings are set successfully
                        
                     }       
                     break;
            default : //If the user inputs invalid option break print error message and try again
                     printf("Invalid option\n");
                     break;
                                 
                 }
               sleep(1);  
    
            }
   pthread_join(mouse,NULL); //Main loop waits until the pthread mouse exits        
   printf("Out of the while loop\n");         
   if(mouseclick==1) //If there is a mouse click exit the background process 
   {
    printf("click condition satisfied\n");
    lseek(fd,0,SEEK_CUR); //Send a seek  function to exit background process in kernel
    printf("background task stopped successfully\n");
   }
   
   close(fd); //Close the file descriptor
   
    return 0; //Exit main
}    
    

