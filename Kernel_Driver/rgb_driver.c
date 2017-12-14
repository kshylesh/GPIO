#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/version.h>
#include<linux/sched.h>
#include<linux/kthread.h>
#include<linux/device.h>
#include<linux/string.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<asm/uaccess.h>
#include<linux/init.h>
#include<linux/types.h>
#include<linux/slab.h>
#include<linux/time.h>
#include<linux/math64.h>
#include<linux/timer.h>
#include<linux/string.h>
#include<linux/jiffies.h>
#include<linux/semaphore.h>
#include<linux/spinlock.h>
#include<linux/gpio.h>
#include<linux/delay.h>
#include"rgb_ioctl.h"
#define RED_ON 0X04    /*mask of red pin*/
#define GREEN_ON 0X02  /*mask of green pin*/
#define BLUE_ON 0X01    /*mask of blue pin*/
#define INTENSITY 20    /*Time interval of 20 milliseconds*/
#define DEVICE_NAME "RGB" 

static dev_t dev_num;     /*structure in which major and minor number is stored*/                 
static struct class *rgb_class; /*structure which holds the class reference*/

/*structure representing the mux combination for a gpio pin*/
typedef struct{
    int mux1,mux2;}mux;
    
/*structure representing the values that need to be put into the mux combination*/    
typedef struct{
    int mux1_val,mux2_val;}mux_val;
    
/*gpio and corresponding mux pins data set*/    
static mux mux_comb[14]={ {-1,-1},
                 {45,-1},
                 {77,-1},
                 {76,64},
                 {-1,-1},
                 {66,-1},
                 {68,-1},
                 {-1,-1},
                 {-1,-1},
                 {70,-1},
                 {74,-1},
                 {44,72},
                 {-1,-1},
                 {46,-1},};
                 
/*gpio and corresponding mux pin value data set*/                 
static mux_val mux_value[14]={ {-1,-1},
                 {1,-1},
                 {0,-1},
                 {0,0},
                 {-1,-1},
                 {0,-1},
                 {0,-1},
                 {-1,-1},
                 {-1,-1},
                 {0,-1},
                 {0,-1},
                 {0,0},
                 {-1,-1},
                 {0,-1},};                
                 
/*The corresponding gpio and direction pins for an input IO pin*/            
static int routing[14]={11,12,61,62,6,0,1,38,40,4,10,5,15,7};
static int direction[14]={32,28,34,16,36,18,20,-1,-1,22,26,24,42,30};

/*Structure representing the rgb device*/
 struct rgb_device             
{
    settings rgb_set; /*strcuture to hold ioctl configuration data*/
    struct cdev rgb_cdev;/*embedded cdev to represent as character device*/
    struct task_struct *trigger_thread;/*Background thread which blinks LED*/
    struct timer_list timer_cb;/*timer for device*/
    spinlock_t spin;/*A lock for the device*/
    int terminate;/*flag to indicate if stop signal been made*/
    int new_config;/*flag which indicates whether driver's configuration is changed or not*/
    int rgb_status;/*holds which all pins are enabled among RGB combination*/
    int on_time,off_time;/*holds the on and off times for which the LED should glow*/
    int seq;/*represents the sequence of glow at any instant of time*/
    char *rgb_name; /*name of device*/ 
    int gpio_mapping[3];/*holds gpio mapping for corresponding RGB IO pins*/
    int io_mapping[3];   /*holds RGB IO pins*/      
};              

struct rgb_device *rgb_dev;/*holds reference of RGB deice*/

/*A timer callback function to update the sequence of glow*/
void timer_callback(unsigned long arg)
{   
    struct rgb_device *rgb=(struct rgb_device *)arg;
    spin_lock(&(rgb->spin));
    rgb->seq++;                     /*updates the sequence*/
    if(rgb->seq==8)
        rgb->seq=1;
       
    spin_unlock(&(rgb->spin));    
    rgb->timer_cb.expires=jiffies+(HZ/2); /*expiration is set after 0.5 sec */
    add_timer(&(rgb->timer_cb));           /*timer is again added to queue*/
  }  
  
 /*A background thread which blinks the led*/                    
  static int background_task(void *arg)
{
    struct rgb_device *rgb_dev=(struct rgb_device *)arg;
    printk(KERN_INFO "Inside %s function\n",__FUNCTION__);
    /*Run till a stop is made on thread*/
    while(!(kthread_should_stop()))
    {   
        
        spin_lock(&(rgb_dev->spin));
        /*Turn pin on if it is to be on for the sequence and if it is enabled by user*/
         if((rgb_dev->seq & RED_ON)&&(rgb_dev->rgb_status & RED_ON))
         {  
            
            gpio_set_value_cansleep(routing[rgb_dev->rgb_set.red_pin],1);
        }
        
         if((rgb_dev->seq & GREEN_ON)&&(rgb_dev->rgb_status & GREEN_ON))
         {  
            
            gpio_set_value_cansleep(routing[rgb_dev->rgb_set.green_pin],1);
           }
         if((rgb_dev->seq & BLUE_ON)&&(rgb_dev->rgb_status & BLUE_ON))
         {
             
            gpio_set_value_cansleep(routing[rgb_dev->rgb_set.blue_pin],1);
         }  
          spin_unlock(&(rgb_dev->spin));
       
        msleep(rgb_dev->on_time); /*Keep pins high for on time*/
        spin_lock(&(rgb_dev->spin));
         /*Turn pin off if it is to be off for the sequence and if it is enabled by user*/
         if((rgb_dev->seq & RED_ON)&&(rgb_dev->rgb_status & RED_ON))
         {  
          
            gpio_set_value_cansleep(routing[rgb_dev->rgb_set.red_pin],0);
        }
        
          if((rgb_dev->seq & GREEN_ON)&&(rgb_dev->rgb_status & GREEN_ON))
         {  
            
            gpio_set_value_cansleep(routing[rgb_dev->rgb_set.green_pin],0);
           }
        if((rgb_dev->seq & BLUE_ON)&&(rgb_dev->rgb_status & BLUE_ON))
         {
             
            gpio_set_value_cansleep(routing[rgb_dev->rgb_set.blue_pin],0);
         }   
         spin_unlock(&(rgb_dev->spin));
        
        msleep(rgb_dev->off_time);/*Keep pins low for off time*/
     }
     
     printk(KERN_INFO "Out of background task\n");
     return 0;
}     
int rgb_open(struct inode *rgb_inode,struct file *rgb_file) /*invoked when the device file is opened*/
{
    struct rgb_device *rgb_devp;
    rgb_devp=container_of(rgb_inode->i_cdev,struct rgb_device,rgb_cdev); /*gets the parent structure*/
    rgb_file->private_data=rgb_devp;     /*Puts the reference of the device structure to private field so that it can be retreived later*/
    printk(KERN_INFO "%s called  %s function\n",rgb_devp->rgb_name,__FUNCTION__);
    return 0;
}

/*ioctl function to set the configuration data*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int rgb_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long rgb_ioctl(struct file *rgb_file, unsigned int cmd, unsigned long arg)
#endif
{
    struct rgb_device *rgb_devp=rgb_file->private_data;
    int i=0,temp=0;
    settings *set=(settings *)arg; /*argument is typecasted to the structure which conatins config data*/
    printk(KERN_INFO "Inside %s function\n",__FUNCTION__);
    switch (cmd)
    {
        case QUERY_GET_VARIABLES:
            
          
            if (copy_to_user((settings *)arg, &(rgb_devp->rgb_set), sizeof(settings))!=0)
            {
                return -EACCES;
            }
            break;
       
        case CONFIG_PINS:
            /*checks if valid inputs are given by user*/
            if((set->duty_cycle<0)||(set->duty_cycle>100)||(set->red_pin<0)||(set->red_pin>13)||(set->green_pin<0)||(set->green_pin>13)||(set->blue_pin<0)||(set->blue_pin>13)||(set->green_pin==set->blue_pin)||(set->red_pin==set->blue_pin)||(set->green_pin==set->red_pin))
                return -1;
             /*stops the background thread if valid new inputs are given*/   
            if(rgb_devp->trigger_thread!=NULL)
            {
            i=kthread_stop(rgb_devp->trigger_thread);  
            if(i!=0)
                printk(KERN_INFO "Error stopping thread in ioctl\n");
            else
                rgb_devp->trigger_thread=NULL; 
             }  
         /*free the pin for the old configuration*/         
            for(i=0;i<3;i++)
        {
            if(rgb_devp->gpio_mapping[i]==-1);/*no need to free pins if the pin is disabled*/
            else
            {
              gpio_set_value_cansleep(rgb_devp->gpio_mapping[i],0);
              gpio_unexport(rgb_devp->gpio_mapping[i]);
              gpio_free(rgb_devp->gpio_mapping[i]);
              if(direction[rgb_devp->io_mapping[i]]!=-1)/*free direction pin*/
              {
                gpio_unexport(direction[rgb_devp->io_mapping[i]]);
                gpio_free(direction[rgb_devp->io_mapping[i]]);
              }
              if(mux_comb[rgb_devp->io_mapping[i]].mux1!=-1)/*free the mux pin*/
              {
                gpio_unexport(mux_comb[rgb_devp->io_mapping[i]].mux1);
                gpio_free(mux_comb[rgb_devp->io_mapping[i]].mux1);
              }
              if(mux_comb[rgb_devp->io_mapping[i]].mux2!=-1)/*free the second mux if any*/
              {
                gpio_unexport(mux_comb[rgb_devp->io_mapping[i]].mux2);
                gpio_free(mux_comb[rgb_devp->io_mapping[i]].mux2);
              }
                
             
            }
        }
            printk(KERN_INFO "All previous configuration pins freed\n"); 
            /*copies the new configuration data*/  
             if (copy_from_user(&(rgb_devp->rgb_set), (settings *)arg, sizeof(settings))!=0)
            {
                return -EACCES;
            }
            /*keeps a copy of new IO pins*/
            rgb_devp->io_mapping[0]=rgb_devp->rgb_set.red_pin;
            rgb_devp->io_mapping[1]=rgb_devp->rgb_set.green_pin;
            rgb_devp->io_mapping[2]=rgb_devp->rgb_set.blue_pin;
            /*Calculates on time and off time based on duty cycle*/
            temp=INTENSITY*rgb_devp->rgb_set.duty_cycle;
            temp=temp/100;
            rgb_devp->on_time=temp;
            rgb_devp->off_time=INTENSITY-rgb_devp->on_time;
            /*sets the flag indicating a new configuration has been made*/
            rgb_devp->new_config=1;
            
            break;
        default:
            return -EINVAL;
    }
 
    return 0;
}
 
ssize_t rgb_write(struct file *rgb_file,const char __user *enable_pins,size_t length,loff_t *pos) 
{   
    int i=0;
    struct rgb_device *rgb_devp=rgb_file->private_data;
      printk(KERN_INFO "%s called  %s function\n",rgb_devp->rgb_name,__FUNCTION__);
     /*gets the data from user space buffer which indicates which all pins needs to be enabled*/ 
    if(copy_from_user(&(rgb_devp->rgb_status),(int *)enable_pins,sizeof(int))!=0)
    {
        printk(KERN_INFO "Bad address while copying to kernel space\n"); 
        return -1;
     }
   /*stops the thread before enabling the new pins*/  
   if(rgb_devp->trigger_thread!=NULL)
   {    
        i=kthread_stop(rgb_devp->trigger_thread);
        if(i!=0)
            printk(KERN_INFO "Error stopping thread from write\n");
        else
           rgb_devp->trigger_thread=NULL;
     } 
    /*sets the direction, mux, gpio pins for corresponding IO pins if it needs to be enabled
    and if enabled it also checks if routing has to be done or not if it has already routed it.*/          
        
    if(rgb_devp->rgb_status & RED_ON)
    { if(rgb_devp->new_config==1)
    { 
     if(direction[rgb_devp->io_mapping[0]]!=-1)
              {
                gpio_request(direction[rgb_devp->io_mapping[0]],"red_direction");
                gpio_export(direction[rgb_devp->io_mapping[0]],0);
                gpio_direction_output(direction[rgb_devp->io_mapping[0]],0);
              }
              if(mux_comb[rgb_devp->io_mapping[0]].mux1!=-1)
              {
                gpio_request(mux_comb[rgb_devp->io_mapping[0]].mux1,"red_mux_1");
                gpio_export(mux_comb[rgb_devp->io_mapping[0]].mux1,0);
                gpio_direction_output(mux_comb[rgb_devp->io_mapping[0]].mux1,mux_value[rgb_devp->io_mapping[0]].mux1_val);
                
              }
               if(mux_comb[rgb_devp->io_mapping[0]].mux2!=-1)
              {
                gpio_request(mux_comb[rgb_devp->io_mapping[0]].mux2,"red_mux_2");
                gpio_export(mux_comb[rgb_devp->io_mapping[0]].mux2,0);
                gpio_direction_output(mux_comb[rgb_devp->io_mapping[0]].mux2,mux_value[rgb_devp->io_mapping[0]].mux2_val);
                
              }
              
        gpio_request(routing[rgb_devp->rgb_set.red_pin],"gpio_red");
        gpio_export(routing[rgb_devp->rgb_set.red_pin],0);
        gpio_direction_output(routing[rgb_devp->rgb_set.red_pin],0);
        rgb_devp->gpio_mapping[0]=routing[rgb_devp->rgb_set.red_pin];
        
        }
    } 
    else
    rgb_devp->gpio_mapping[0]=-1;
    if(rgb_devp->rgb_status & GREEN_ON)
    {   if(rgb_devp->new_config==1)
    {
        if(direction[rgb_devp->io_mapping[1]]!=-1)
              {
                gpio_request(direction[rgb_devp->io_mapping[1]],"green_direction");
                gpio_export(direction[rgb_devp->io_mapping[1]],0);
                gpio_direction_output(direction[rgb_devp->io_mapping[1]],0);
              }
              if(mux_comb[rgb_devp->io_mapping[1]].mux1!=-1)
              {
                gpio_request(mux_comb[rgb_devp->io_mapping[1]].mux1,"green_mux_1");
                gpio_export(mux_comb[rgb_devp->io_mapping[1]].mux1,0);
                gpio_direction_output(mux_comb[rgb_devp->io_mapping[1]].mux1,mux_value[rgb_devp->io_mapping[1]].mux1_val);
                
              }
               if(mux_comb[rgb_devp->io_mapping[1]].mux2!=-1)
              {
                gpio_request(mux_comb[rgb_devp->io_mapping[1]].mux2,"green_mux_2");
                gpio_export(mux_comb[rgb_devp->io_mapping[1]].mux2,0);
                gpio_direction_output(mux_comb[rgb_devp->io_mapping[1]].mux2,mux_value[rgb_devp->io_mapping[1]].mux2_val);
                
              }
        gpio_request(routing[rgb_devp->rgb_set.green_pin],"gpio_green");
        gpio_export(routing[rgb_devp->rgb_set.green_pin],0);
        gpio_direction_output(routing[rgb_devp->rgb_set.green_pin],0);
        rgb_devp->gpio_mapping[1]=routing[rgb_devp->rgb_set.green_pin];
        
        } 
    }
    else
    rgb_devp->gpio_mapping[1]=-1; 
    if(rgb_devp->rgb_status & BLUE_ON)
    { if(rgb_devp->new_config==1)
    { 
     if(direction[rgb_devp->io_mapping[2]]!=-1)
              {
                gpio_request(direction[rgb_devp->io_mapping[2]],"blue_direction");
                gpio_export(direction[rgb_devp->io_mapping[2]],0);
                gpio_direction_output(direction[rgb_devp->io_mapping[2]],0);
              }
              if(mux_comb[rgb_devp->io_mapping[2]].mux1!=-1)
              {
                gpio_request(mux_comb[rgb_devp->io_mapping[2]].mux1,"blue_mux_1");
                gpio_export(mux_comb[rgb_devp->io_mapping[2]].mux1,0);
                gpio_direction_output(mux_comb[rgb_devp->io_mapping[2]].mux1,mux_value[rgb_devp->io_mapping[2]].mux1_val);
                
              }
               if(mux_comb[rgb_devp->io_mapping[2]].mux2!=-1)
              {
                gpio_request(mux_comb[rgb_devp->io_mapping[2]].mux2,"blue_mux_2");
                gpio_export(mux_comb[rgb_devp->io_mapping[2]].mux2,0);
                gpio_direction_output(mux_comb[rgb_devp->io_mapping[2]].mux2,mux_value[rgb_devp->io_mapping[2]].mux2_val);
                
              }
        gpio_request(routing[rgb_devp->rgb_set.blue_pin],"gpio_blue");
        gpio_export(routing[rgb_devp->rgb_set.blue_pin],0);
        gpio_direction_output(routing[rgb_devp->rgb_set.blue_pin],0);
        rgb_devp->gpio_mapping[2]=routing[rgb_devp->rgb_set.blue_pin];
        
        }
    } 
    else
    rgb_devp->gpio_mapping[0]=-1;   
       printk(KERN_INFO "The mapped gpio pins are %d\t %d\t %d\t\n",rgb_dev->gpio_mapping[0],rgb_dev->gpio_mapping[1],rgb_dev->gpio_mapping[2]);
     /*Makes the background thread to run again after the setting*/
     if(rgb_dev->trigger_thread==NULL)       
     rgb_dev->trigger_thread=kthread_run(&background_task,rgb_dev,"Background_task");
    else 
    printk(KERN_INFO "background is not null\n");
    
    rgb_devp->terminate=0;
    /*sets the flag to zero indicating it is no more a new configuration*/
    rgb_devp->new_config=0; 
    return 0;
    
    
}

/*This is invoked from user space when mouse detection is made and carries out cleaning task*/    
loff_t rgb_seek(struct file *rgb_file,loff_t off,int seek)
{   
    int i=0;
     struct rgb_device *rgb_devp=rgb_file->private_data;
     printk(KERN_INFO "Inside %s function:\n",__FUNCTION__);
     /*sets the flag indicating a mouse detection has been made*/
     rgb_devp->terminate=1;
     /*stops the background thread which glows the LED**/
     if(rgb_devp->trigger_thread!=NULL)
      {    
       i=kthread_stop(rgb_devp->trigger_thread);
       
      if(i!=0)
            printk(KERN_INFO "Thread not returning\n"); 
      else
      {
        rgb_devp->trigger_thread=NULL;
        printk("Background task stopped, now freeing resources\n");
        /*Frees all the pins associated to glow led from a corresponding IO pin*/
        for(i=0;i<3;i++)
        {   printk(KERN_INFO "test\n");
            if(rgb_devp->gpio_mapping[i]==-1);
            else
            {
              gpio_set_value_cansleep(rgb_devp->gpio_mapping[i],0);
              gpio_unexport(rgb_devp->gpio_mapping[i]);
              gpio_free(rgb_devp->gpio_mapping[i]);
              if(direction[rgb_devp->io_mapping[i]]!=-1)
              {
                gpio_unexport(direction[rgb_devp->io_mapping[i]]);
                gpio_free(direction[rgb_devp->io_mapping[i]]);
              }
              if(mux_comb[rgb_devp->io_mapping[i]].mux1!=-1)
              {
                gpio_unexport(mux_comb[rgb_devp->io_mapping[i]].mux1);
                gpio_free(mux_comb[rgb_devp->io_mapping[i]].mux1);
              }
              if(mux_comb[rgb_devp->io_mapping[i]].mux2!=-1)
              {
                gpio_unexport(mux_comb[rgb_devp->io_mapping[i]].mux2);
                gpio_free(mux_comb[rgb_devp->io_mapping[i]].mux2);
              }
            }
        }
      }
      }
      else
        printk("Trigger thread is NULL\n");      
     return 0;
}     

int rgb_close(struct inode *rgb_inode,struct file *rgb_file) /*invoked when the device file is closed*/
{
   struct rgb_device *rgb_devp=rgb_file->private_data; /*gets the reference of the device structure from its specific file structure*/
   printk(KERN_INFO "%s called  %s function\n",rgb_devp->rgb_name,__FUNCTION__);
   return 0;
}

/*assigns all the file operation function pointers*/
static struct file_operations rgb_ops=
{
    .owner=THIS_MODULE,
    .open=rgb_open,
    .release=rgb_close,
    .write=rgb_write,
    
    .llseek=rgb_seek,
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = rgb_ioctl
#else
    .unlocked_ioctl = rgb_ioctl
#endif
};

/*function invoked when the module is loaded*/
int rgb_init(void)
{
    int i=0;
    printk(KERN_INFO "Inside %s function\n",__FUNCTION__);
    
    if(alloc_chrdev_region(&dev_num,0,1,DEVICE_NAME)<0) /*allocates the major and minor number dynamically*/
    {
        printk(KERN_INFO "unsuccessful major number allocation\n");
        return -1;
     }
   if((rgb_class=class_create(THIS_MODULE,DEVICE_NAME))==NULL) /*creates the class for device files*/
   {    
        printk(KERN_INFO "Unable to create class\n");
        unregister_chrdev_region(dev_num,1);
        return -1;   
   }
  
       rgb_dev=(struct rgb_device*)kmalloc(sizeof(struct rgb_device),GFP_KERNEL);/*allocates memory to our device structure*/
       memset(rgb_dev,0,sizeof(struct rgb_device));
        if(!rgb_dev)
        {
            printk(KERN_INFO "Cant allocate memory to data structure\n");
            return -ENOMEM;
         }
    /*sets the initialisation default data*/     
    rgb_dev->rgb_set.duty_cycle=50;
    rgb_dev->on_time=10;
    rgb_dev->off_time=10;
    rgb_dev->new_config=1;
    rgb_dev->rgb_set.red_pin=10;
    rgb_dev->rgb_set.green_pin=0;
    rgb_dev->rgb_set.blue_pin=1;
    rgb_dev->rgb_status=7;
    rgb_dev->terminate=0;
    rgb_dev->seq=1; 
    spin_lock_init(&(rgb_dev->spin));
    init_timer(&(rgb_dev->timer_cb));/*initialzes the timer*/
    rgb_dev->timer_cb.function=&timer_callback;/*assigns the callback function that needs to be invoked when timer expires*/
    rgb_dev->timer_cb.expires=jiffies+(HZ/2);/*sets the expiration time to 0.5 sec*/
    rgb_dev->timer_cb.data=(unsigned long)rgb_dev;/*gives the device structure as an argument to timer callback function*/
    for(i=0;i<3;i++)
        rgb_dev->gpio_mapping[i]=-1; /*initially sets gpio mapping to -1, until proper mapping is done by write function*/
    rgb_dev->io_mapping[0]=10;
    rgb_dev->io_mapping[1]=0;
    rgb_dev->io_mapping[2]=1;
    
    
    rgb_dev->rgb_name="RGB_LED";
    
        if(device_create(rgb_class,NULL,MKDEV(MAJOR(dev_num),MINOR(dev_num)),NULL,"RGBled")==NULL) /*creates the info for udev so that it creates a device node with name  "dataqueue0" and "dataqueue1"*/
        {
            class_destroy(rgb_class);
            unregister_chrdev_region(dev_num,1);
            printk(KERN_INFO "Registration error\n");
            return -1;
         }
    
     
        cdev_init(&(rgb_dev->rgb_cdev),&rgb_ops);/*links the file operations with the character device*/
        rgb_dev->rgb_cdev.owner=THIS_MODULE; /*sets the owner of the device*/
        if(cdev_add(&(rgb_dev->rgb_cdev),MKDEV(MAJOR(dev_num),MINOR(dev_num)),1)==-1) /*adds the character device to the linked list maintained by kernel to traverse the character device*/
        {
            device_destroy(rgb_class,MKDEV(MAJOR(dev_num),MINOR(dev_num)));
            class_destroy(rgb_class);
            unregister_chrdev_region(dev_num,1);
            return -1;
         }
     
   add_timer(&(rgb_dev->timer_cb)); /*adds the timer to the queue*/
   
   
      printk(KERN_INFO "Device created\n");
      return 0;
  }

/*invoked when the module is unloaded*/  
void rgb_exit(void)
{
    int i=0;
    printk(KERN_INFO "Inside %s function\n",__FUNCTION__);
    
    /*frees the gpio pins if not stopped by mouse event detection*/
    if(rgb_dev->terminate==0)
    {
    for(i=0;i<3;i++)
        {
            if(rgb_dev->gpio_mapping[i]==-1);/*no need to free pins if the pin is disabled*/
            else
            {
              gpio_set_value_cansleep(rgb_dev->gpio_mapping[i],0);
              gpio_unexport(rgb_dev->gpio_mapping[i]);
              gpio_free(rgb_dev->gpio_mapping[i]);
              if(direction[rgb_dev->io_mapping[i]]!=-1)/*free direction pin*/
              {
                gpio_unexport(direction[rgb_dev->io_mapping[i]]);
                gpio_free(direction[rgb_dev->io_mapping[i]]);
              }
              if(mux_comb[rgb_dev->io_mapping[i]].mux1!=-1)/*free the mux pin*/
              {
                gpio_unexport(mux_comb[rgb_dev->io_mapping[i]].mux1);
                gpio_free(mux_comb[rgb_dev->io_mapping[i]].mux1);
              }
              if(mux_comb[rgb_dev->io_mapping[i]].mux2!=-1)/*free the second mux if any*/
              {
                gpio_unexport(mux_comb[rgb_dev->io_mapping[i]].mux2);
                gpio_free(mux_comb[rgb_dev->io_mapping[i]].mux2);
              }
                
             
            }
        }
      }    
      
    
        
       del_timer_sync(&(rgb_dev->timer_cb)); /*removes the timer from queue and associated call back function */   
        cdev_del(&(rgb_dev->rgb_cdev));/*delete the character device from linked list*/
        device_destroy(rgb_class,MKDEV(MAJOR(dev_num),MINOR(dev_num)));/*destroy the node*/
        kfree(rgb_dev);/*frees the memory allocated to device*/
    
    class_destroy(rgb_class);/*remove the class to which the queue belong*/
    unregister_chrdev_region(dev_num,1); /*unregister the major and minor numbers allocated*/  
 }
 module_init(rgb_init);/*registers the init function*/
 module_exit(rgb_exit);/*resgisters the exit function*/
 MODULE_LICENSE("GPL");/*sets the license to GPL so that all exported symbol of GPL compliant can be used by this module*/
        
