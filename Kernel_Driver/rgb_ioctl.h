#ifndef QUERY_IOCTL_H
#define QUERY_IOCTL_H
#include <linux/ioctl.h>

/*structure containing entries for configuration data*/ 
typedef struct
{
    int duty_cycle,red_pin,blue_pin,green_pin;
    
} settings;
 
#define QUERY_GET_VARIABLES _IOR('s', 1, settings *) /*command to get configuration data*/
#define CONFIG_PINS _IOW('s',2,settings *) /*command to set configuration data*/
 
#endif
