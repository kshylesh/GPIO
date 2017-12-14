//Header file with function declarations//

#define EVENT_PATH "/dev/input/event2"		//specifies the mouse event file
#define intensity 20000				//specifies the intensity of LED blinking
#define RON 0x01				//Mask to blink RED
#define GON 0X02				//Mask to blink GREEN
#define	BON 0X04				//Mask to blink BLUE


int gpio_export(unsigned int gpio);		//Function declaration of gpio exporting

void getIO();					//Function declaration for scanning user pins(IO0-IO13)

void IOConfig(void);				//Function declaration for configuring gpio pins

unsigned int user_pins[3],gpio_pins[3];		//Global arrays user_pins:to hold the IOs gpio_pins gpio_pins:to hold the exported gpio_pin

int counter;					//Global counter for sequencing

int mouseclick;					//Global counter for mouseclick event

unsigned int pin_sel [14][5];			//Defining table as globall 2D array

