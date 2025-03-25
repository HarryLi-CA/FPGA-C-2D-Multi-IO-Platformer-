#include <math.h>
#include <stdbool.h>

#define PS2_BASE 0xFF200100
#define PS2_BASE2 0xFF200108
	
#define HEX3_HEX0_BASE 0xFF200020
#define HEX5_HEX4_BASE 0xFF200030
#define AUDIO_BASE  0xFF203040  
#define SWITCH_BASE 0xFF200040 
#define LEDR_BASE 0xFF200000

#define JUMP_AUDIO_DELAY 1500

#define HIGH_AUDIO 0x7FFFFFFF // 24-bit max value
#define LOW_AUDIO 0x000000  // low value


short back_color = 0xffff;

volatile int pixel_buffer_start; // global variable
short int Buffer1[240][512]; // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

int mouseX = 0; int mouseY = 0;

#define playerSpriteRows 6
#define playerSpriteCols 4
int playerSprite[playerSpriteRows][playerSpriteCols] = {
    {0x0, 0x0, 0x0, 0x0},
    {0x0, 0x0, 0x0, 0x0},
    {0x0, 0x0, 0x0, 0x0},
    {0x0, 0x0, 0x0, 0x10000},
    {0x0, 0x0, 0x0, 0x10000},
    {0x0, 0x0, 0x0, 0x10000},
};


struct audio_t {
    volatile unsigned int control; // The control/status register
    volatile unsigned char rarc; // the 8 bit RARC register
    volatile unsigned char ralc; // the 8 bit RALC register
    volatile unsigned char wsrc; // the 8 bit WSRC register
    volatile unsigned char wslc; // the 8 bit WSLC register
    volatile unsigned int ldata;
    volatile unsigned int rdata;
    };	
    struct audio_t* audiofr = (struct audio_t*) AUDIO_BASE;

void clear_screen();
void draw_line(int x0, int y0, int x1, int y1, short int line_color);

void draw_sprite(int x, int y, int rows, int cols, int sprite[rows][cols], bool isErase);

void draw_box(int x, int y, short int line_color);

void swap(int* a, int* b);
void wait_for_vsync();


void play_audio(int delay);


int toggle = 0; // control high value to low vlue
int counter = 0;

volatile int * PS2_ptr = (int *)PS2_BASE;
int PS2_data, RVALID;
struct bytePacket{
    char byte1; //oldest byte received
    char byte2;
    char byte3; // most recent byte
};
bool using_mouse = false;

struct keyStates
{
    bool upPressed;
    bool downPressed;
    bool leftPressed;
    bool rightPressed;
};
enum KeyCode{
    NOKEY,
    LEFT,
    RIGHT,
    UP,
    DOWN
};



void handle_key_states(enum KeyCode key, bool isPress, struct keyStates* currentKeyState){
    switch (key)
    {
    case LEFT:
        currentKeyState->leftPressed = isPress;
        break;
    case RIGHT:
        currentKeyState->rightPressed = isPress;
        break;
    case UP:
        currentKeyState->upPressed = isPress;
        break;
    case DOWN:
        currentKeyState->downPressed = isPress;
        break;
    default:
        break;
    }
}

void handle_ps2(volatile int* currentPS2, struct bytePacket* bP, int* byteCounter, struct keyStates* currentKeyState){
    PS2_data = *(currentPS2); // read the Data register in the PS/2 port
    RVALID = PS2_data & 0x8000; // extract the RVALID field
    if (RVALID) {
    /* shift the next data byte into the display */
    *byteCounter++;
    
    bP->byte1 = bP->byte2;
    bP->byte2 = bP->byte3;
    bP->byte3 = PS2_data & 0xFF;
    char byte1 = bP->byte1;
    char byte2 = bP->byte2;
    char byte3 = bP->byte3;

    char isMouseValue = byte1  & 0x8;
    if(!using_mouse){
        bool isPress = byte2 != 0xF0;
        char specByte = isPress ? byte2: byte1;
        enum KeyCode curKey;
        switch (specByte)
        {
        case 0xE0:
            switch (byte3)
            {
            case 0x6B:
                curKey = LEFT;
                break;
            case 0x72:
                curKey = DOWN;
                break;
            case 0x75:
                curKey = UP;
                break;
            case 0x74:
                curKey = RIGHT;
                break;
            default:
                curKey = NOKEY;
                break;
            }
            break;
        
        default:
            curKey = NOKEY;
            break;
        }
        if(curKey == NOKEY)
            return;
        handle_key_states(curKey, isPress, currentKeyState);
    }

    if(using_mouse && isMouseValue == 0x8 && byteCounter >= 3){
        char state = byte1;
        char d = byte2;
        int rel_x = d - ((state << 4) & 0x100);
        d = byte3;
        int rel_y = d - ((state << 3) & 0x100);
        mouseX += rel_x;
        mouseY += rel_y;
        byteCounter = 0;
    }
    if ((byte2 == (char)0xAA) && (byte3 == (char)0x00))
    {
        // mouse inserted; initialize sending of data
        *(currentPS2) = 0xF4;
        byteCounter = 0;
        using_mouse = true;
    }
    }
}

int audio_value(){
    volatile int * audio_ptr = (int *) AUDIO_BASE;
    int fifospace = *(audio_ptr + 1); // read the audio port fifospace register
    if ((fifospace & 0x000000FF) > 0) // check RARC to see if there is at least one sample input 
    {
        // load both input microphone channels - just get one sample from each
        int left = *(audio_ptr + 2); // lread the leftdata register - input FIFO
        int right = *(audio_ptr + 3); // same for rightdata
        return (left+right)/2;
    }
    else
        return 0;
}

void play_audio(int delay){
    int half_period = JUMP_AUDIO_DELAY;

//chcking FIFO
    int space_right  = (audiofr ->wsrc & 0xFF); //space remaining in right channel fifo
    int space_left = (audiofr ->wslc& 0xFF); //space remaining in left channel (have to make sure not looking at bits below 7th bit since those are right capacity)
    int amp = HIGH_AUDIO;
//making output		
    if (space_left > 0 && space_right > 0) { //space in fifo for new sample/input
        // Write a sample
        audiofr ->ldata = amp; //left channel waveform = amplitude
        audiofr ->rdata = -amp; //right channel wavforme = amplitude
        
        counter++; //keeps track of how many samples have been output
        if (counter >= half_period) { //if more samples than the amount of samples in the toggle 1 state of square wave, put to 0
            //new cycle needs to begin
            counter = 0;
            // Toggle amplitude from +some_value to -some_value
            amp *= -1;
        }
    }

}

void swap(int * a, int * b)
{
	*a=(*a)^(*b);
	*b=(*a)^(*b);
	*a=(*b)^(*a);
}
// code for subroutines (not shown)
void draw_box(int x, int y, short int line_color)
{
	for(int i = -1; i <=1; i++){
		for(int j = -1; j <=1; j++){
			int curx = x+i;
			int cury = y+j;
			if(y >=0 && y<240 && x >=0 && x<319){
				plot_pixel(curx,cury, line_color);
			}
		}
	}
}

void draw_sprite(int x, int y, int rows, int cols, int sprite[rows][cols], bool isErase){
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            int curx = x+j;
			int cury = y+i;
			if(sprite[i][j] <=0xffff){
                if(isErase)
				    plot_pixel(curx,cury, back_color);
                else
				    plot_pixel(curx,cury, (short)sprite[i][j]);
			}
        }
    }
    
}

void draw_line(int x0, int y0, int x1, int y1, short int line_color)
{
	bool is_steep = abs(y1 - y0) > abs(x1 - x0);
	if (is_steep){
		swap(&x0, &y0);
		swap(&x1, &y1);
	}
	if (x0 > x1){
		swap(&x0, &x1);
		swap(&y0, &y1);
	}
	int deltax = x1 - x0;
	int deltay = abs(y1 - y0);
	int error = -(deltax / 2);
	int y = y0;
	int y_step;
	if (y0 < y1) 
		y_step = 1;
	else 
		y_step = -1;
	for (int x = x0; x < x1; x++){
		if (is_steep)
			plot_pixel(y, x, line_color);
		else
			plot_pixel(x, y, line_color);
		error = error + deltay;
		if (error > 0) {
			y = y + y_step;
			error = error - deltax;
		}
	}
}
void clear_screen()
{
	for(int i = 0; i < 320; i++)
		for(int j = 0; j < 240; j++)
			plot_pixel(i,j,back_color);
}
void plot_pixel(int x, int y, short int line_color)
{
    if(x >=0 && x<320 && y >=0 && y<240){
        volatile short int *one_pixel_address;
        
        one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);
        
        *one_pixel_address = line_color;
    }
}


void wait_for_vsync()
{
	volatile int * pixel_ctrl_ptr = (int *) 0xff203030; // base address
	int status;
	*pixel_ctrl_ptr = 1; // start the synchronization process
	// - write 1 into front buffer address register
	status = *(pixel_ctrl_ptr + 3); // read the status register
	while ((status & 0x01) != 0) // polling loop waiting for S bit to go to 0
	{
		status = *(pixel_ctrl_ptr + 3);
	}
}

int main(void)
{
    *(PS2_ptr) = 0xFF; // reset
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    volatile int *switch_ptr = (int *) SWITCH_BASE; 
    volatile int *LEDR_ptr = (int *) LEDR_BASE;

    int ps2ByteCounterl = 0;
    int ps2ByteCounter2 = 0;

    struct bytePacket ps2BytePacket1;
    struct bytePacket ps2BytePacket2;

    int toggle = 0; // control high value to low vlue
    int delay, counter = 0;
    struct keyStates currentKeyState;
    currentKeyState.upPressed = false;
    currentKeyState.downPressed = false;
    currentKeyState.leftPressed = false;
    currentKeyState.rightPressed = false;

    // declare other variables(not shown)
	int x[8]; int y[8]; int vx[8]; int vy[8];
    // initialize location and direction of rectangles(not shown)
	for(int i = 0; i < 8; i++){
		x[i] = rand()%318 + 1;
		y[i] = rand()%238 + 1;
		vx[i] = (rand()%2)*2-1;
		vy[i] = (rand()%2)*2-1;
	}

    /* set front pixel buffer to Buffer 1 */
    *(pixel_ctrl_ptr + 1) = (int) &Buffer1; // first store the address in the  back buffer

    /* now, swap the front/back buffers, to set the front buffer location */
    wait_for_vsync();
    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen(); // pixel_buffer_start points to the pixel buffer

    /* set back pixel buffer to Buffer 2 */
    *(pixel_ctrl_ptr + 1) = (int) &Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
    clear_screen(); // pixel_buffer_start points to the pixel buffer
	
	short int line_color = 0x0;
    while (1)
    {
        int switch_value = *switch_ptr & 0x3FF; // read 10-bit switches
        if(switch_value != 0){
            int audioVal = audio_value();
            *LEDR_ptr = 1;
            int currentPow = switch_value*100;
            if(audioVal > HIGH_AUDIO){
                audioVal *= 1024;
                audioVal /= HIGH_AUDIO;
                *LEDR_ptr = audioVal;
            }
            play_audio(JUMP_AUDIO_DELAY);
        }

        /* Erase any boxes and lines that were drawn in the last iteration */
        
		for(int i = 0; i < 8; i++){
			draw_sprite(x[i], y[i], 6, 4, playerSprite, true);
			draw_line(x[(i+1)%8],  y[(i+1)%8],  x[i],  y[i], back_color);
		}

        // code for updating the locations of boxes (not shown)
		for(int i = 0; i < 8; i++){
			x[i] += vx[i];
			y[i] += vy[i];
			if(x[i] <= 0 || x[i] >=320)
				vx[i] *=-1;
			if(y[i] <= 0 || y[i] >=239)
				vy[i] *=-1;
		}

        handle_ps2(PS2_BASE, &ps2BytePacket1,&ps2ByteCounterl , &currentKeyState);
        handle_ps2(PS2_BASE2, &ps2BytePacket1,&ps2ByteCounter2, &currentKeyState);

        int ledValue = 0;
        if(currentKeyState.upPressed){
            ledValue = ledValue | 0x1;
        }
        if(currentKeyState.downPressed){
            ledValue = ledValue | 0x2;
        }
        if(currentKeyState.leftPressed){
            ledValue = ledValue | 0x4;
        }
        if(currentKeyState.rightPressed){
            ledValue = ledValue | 0x8;
        }
        *LEDR_ptr = ledValue;

        draw_sprite(mouseX, mouseY, 6, 4, playerSprite, false);
        // code for drawing the boxes and lines (not shown)
		for(int i = 0; i < 8; i++){
			draw_sprite(x[i], y[i], 6, 4, playerSprite, false);
			draw_line( x[(i+1)%8],  y[(i+1)%8],  x[i],  y[i], 0x001F);
		}

		if(*(pixel_ctrl_ptr) == (int) &Buffer1){
		*(pixel_ctrl_ptr) = (int) &Buffer2;
		*(pixel_ctrl_ptr + 1) = (int) &Buffer1;
		}
		else{
		*(pixel_ctrl_ptr) = (int) &Buffer1;
		*(pixel_ctrl_ptr + 1) = (int) &Buffer2;
		}
        wait_for_vsync(); // swap front and back buffers on VGA vertical sync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
    
    }
}
