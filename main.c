#include <math.h>
#include <stdbool.h>
#include<stdio.h>
#include<stdlib.h>

int * const PS2_BASE = (int *)0xFF200100;
int * const PS2_BASE2 = (int *)0xFF200108;

int * const HEX3_HEX0_BASE = (int *)0xFF200020;
int * const HEX5_HEX4_BASE = (int *)0xFF200030;
int * const AUDIO_BASE = (int *)0xFF203040;
int * const SWITCH_BASE = (int *)0xFF200040;
int * const LEDR_BASE = (int *)0xFF200000;

const int JUMP_AUDIO_DELAY = 1000;

const int HIGH_AUDIO = 0x7FFFFFFF; // 24-bit max value
const int LOW_AUDIO = 0x000000;  // low value

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SCEEN_BOUNDS 50

short back_color = 0xefdf;

volatile int pixel_buffer_start; // global variable
short int Buffer1[240][512]; // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];


struct sprite_t
{
    const int height;
    const int width;
    const int scaling;
    const int* value;
};


#define playerSpriteRows 8
#define playerSpriteCols 8
#define playerScaling 2
const int playerImage[playerSpriteRows][playerSpriteCols] = {
    // Row 1: Top border (black outline)
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    // Row 2: Circuit design
    {0x0000, 0x07E0, 0x07E0, 0x07FF, 0x07E0, 0x07E0, 0x07E0, 0x0000},
    // Row 3: Circuit design
    {0x0000, 0x07E0, 0xFFE0, 0x07E0, 0x07E0, 0x07FF, 0x07E0, 0x0000},
    // Row 4: Circuit design
    {0x0000, 0x07FF, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0x0000},
    // Row 5: Circuit design
    {0x0000, 0x07E0, 0x07E0, 0x07FF, 0x07E0, 0x07E0, 0x07E0, 0x0000},
    // Row 6: Circuit design
    {0x0000, 0x07E0, 0xFFE0, 0x07E0, 0x07FF, 0x07E0, 0x07E0, 0x0000},
    // Row 7: Circuit design
    {0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0x07E0, 0x0000},
    // Row 8: Bottom border (black outline)
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}
};

#define mouseSpriteRows 14
#define mouseSpriteCols 9
#define mouseScaling 1

const int mouseImage[mouseSpriteRows][mouseSpriteCols] = {
    {0x0000, 0x0, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000}, 
    {0x0000, 0xFFE0, 0x0, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000},
    {0x0000, 0xFFE0, 0xFFE0, 0x0, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000},
    {0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0, 0x100000, 0x100000, 0x100000, 0x100000}, 
    {0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0, 0x100000, 0x100000, 0x100000},  
    {0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0, 0x100000, 0x100000},
    {0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0, 0x100000}, 
    {0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0}, 
    {0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0},  
    {0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0, 0x100000}, 
    {0x0000, 0xFFE0, 0xFFE0, 0, 0, 0, 0, 0x100000, 0x100000},   
    {0x0000, 0xFFE0, 0x0, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000},
    {0x0000, 0x0, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000},
    {0x0000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000, 0x100000},
};

const struct sprite_t mouse = {mouseSpriteRows, mouseSpriteCols, mouseScaling, (int*)mouseImage};
const struct sprite_t player = {playerSpriteRows, playerSpriteCols, playerScaling, (int*)playerImage};

struct vector2{
    int x;
    int y;
};

struct gameObject{
    bool isInactive; // controls whether or not to ignore
    struct vector2 position;
    struct vector2 velocity;
    struct vector2 acceleration;
    struct vector2 dimensions;
    const struct sprite_t* sprite;
    bool isInvisible;
    bool isFixedToScreen;
    bool isCollidable;
    bool isStatic;
    bool isPlayer;
    bool isEnemy;
};

static struct gameObject mouseObject = {false, {100, 100}, {0, 0}, {0, 0}, {0, 0}, &mouse, false, true, false, true, false, false};
static struct gameObject screenObject = {true, {0, 0}, {0, 0}, {0, 0}, {0, 0}, NULL, true, true, false, true, false};


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

void draw_box(int x, int y, short int line_color);

void wait_for_vsync();


int counter = 0;

volatile int * PS2_ptr = (int *)PS2_BASE;
int PS2_data, RVALID;
struct bytePacket{
    char byte1; //oldest byte received
    char byte2;
    char byte3; // most recent byte
    int byteCounter;
    bool usingMouse;
};

struct keyStates
{
    bool upPressed;
    bool downPressed;
    bool leftPressed;
    bool rightPressed;
    bool exitPressed;
};

enum KeyCode{
    NOKEY,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    ESC,
    ERROR_OVERFLOW,
};


// Handle the key states, and update the current key state
void handle_key_states(const enum KeyCode key, const bool isPress, struct keyStates* currentKeyState){
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
    case ESC:
        currentKeyState->exitPressed = isPress;
        break;
    case ERROR_OVERFLOW:
        currentKeyState->upPressed = false;
        currentKeyState->downPressed = false;
        currentKeyState->leftPressed = false;
        currentKeyState->rightPressed = false;
        currentKeyState->exitPressed = false;
        break;

    default:
        break;
    }
}

// Handle the PS2 input, and update the key states, and mouse position, initial template from DE1-SoC Documentation
void handle_ps2(volatile int* currentPS2, struct bytePacket* bP, struct keyStates* currentKeyState){
    PS2_data = *(currentPS2); // read the Data register in the PS/2 port
    RVALID = PS2_data & 0x8000; // seek if the data is valid
    while (RVALID) {
    /* shift the next data byte into the display */
    bP->byteCounter++;
    
    bP->byte1 = bP->byte2;
    bP->byte2 = bP->byte3;
    bP->byte3 = PS2_data & 0xFF;
    char byte1 = bP->byte1;
    char byte2 = bP->byte2;
    char byte3 = bP->byte3;

    char isMouseValue = byte1  & 0x8;
    if(!bP->usingMouse){
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
            switch (byte3)
            {
            case 0x76:
                curKey = ESC;
                break;
            case 0xFF:
                curKey = ERROR_OVERFLOW;
                break;
            case 0x00:
                curKey = ERROR_OVERFLOW;
                break;
            
            default:
                curKey = NOKEY;
                break;
            }
            break;
        }
        if(curKey == NOKEY)
            return;
        handle_key_states(curKey, isPress, currentKeyState);
    }

    if(bP->usingMouse && isMouseValue == 0x8 && bP->byteCounter >= 3){
        char state = byte1;
        char d = byte2;
        int rel_x = d - ((state << 4) & 0x100);
        d = byte3;
        int rel_y = d - ((state << 3) & 0x100);
        mouseObject.position.x += rel_x;
        mouseObject.position.y += rel_y;
        
        if(mouseObject.position.x < 0)
            mouseObject.position.x = 0;
        if(mouseObject.position.x >= SCREEN_WIDTH)
            mouseObject.position.x = SCREEN_WIDTH - 1;
        if(mouseObject.position.y < 0)
            mouseObject.position.y = 0;
        if(mouseObject.position.y >= SCREEN_HEIGHT)
            mouseObject.position.y = SCREEN_HEIGHT - 1;

        bP->byteCounter = 0;
    }
    if ((byte2 == (char)0xAA) && (byte3 == (char)0x00))
    {
        // mouse inserted; initialize sending of data
        *(currentPS2) = 0xF4;
        bP->byteCounter = 0;
        bP->usingMouse = true;
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

void play_audio(const int* delay, const int time){
    if(*delay <= 0)
        return;
    for(int i =0; i < time; i++){
        int half_period = *delay;
        int space_right  = (audiofr ->wsrc & 0xFF);
        int space_left = (audiofr ->wslc& 0xFF);
        int amp = HIGH_AUDIO;

        if (space_left > 0 && space_right > 0) { // check if enough space in both channels
            audiofr ->ldata = amp; 
            audiofr ->rdata = amp;
            counter++;
            if (counter >= half_period) {
                counter = 0;
                amp *= -1;
            }
        }
    }

}

static inline void swap(int * const a, int * const b)
{
	*a=(*a)^(*b);
	*b=(*a)^(*b);
	*a=(*b)^(*a);
}
void plot_pixel(const int x, const int y, const short int line_color)
{
    if(x >=0 && x<320 && y >=0 && y<240){
        volatile short *one_pixel_address;
        
        one_pixel_address = (short *)(pixel_buffer_start + (y << 10) + (x << 1));
        
        *one_pixel_address = line_color;
    }
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

void draw_sprite(const struct gameObject* gO, bool isErase) {
    for (int i = 0; i < gO->sprite->height; i++) {
        for (int j = 0; j < gO->sprite->width; j++) {
            int color = gO->sprite->value[i * gO->sprite->width + j]; // Access 1D array as 2D
            if (color <= 0xFFFF) { // Ignore transparent pixels
                // Scale each pixel
                for (int dy = 0; dy < gO->sprite->scaling; dy++) {
                    for (int dx = 0; dx < gO->sprite->scaling; dx++) {
                        int curx = gO->position.x + j * gO->sprite->scaling + dx;
                        int cury = gO->position.y + i * gO->sprite->scaling + dy;
                        if(!gO->isFixedToScreen){
                            curx -= screenObject.position.x;
                            cury -= screenObject.position.y;
                        }
                        
                        if (isErase)
                            plot_pixel(curx, cury, back_color);
                        else
                            plot_pixel(curx, cury, (short)color);
                    }
                }
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
void draw_background(const bool isErase) {
    /*
    volatile short *pixel_ptr = (short *)pixel_buffer_start;
    int start_x = screenObject.position.x;
    int end_x = start_x + SCREEN_WIDTH;

    for (int x = start_x; x < end_x; x++) {
        int screen_x = x - screenObject.position.x;
        if (screen_x >= 0 && screen_x < SCREEN_WIDTH) {
            short color = isErase ? back_color : 0xcccc;
            *(pixel_ptr + screen_x * (SCREEN_WIDTH + (512 - SCREEN_WIDTH)) + x) = color; // Update only the first row
        }
    }
        */
}

void clear_screen()
{
    volatile short *pixel_ptr = (short *)pixel_buffer_start;
    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        for (int j = 0; j < SCREEN_WIDTH; j++) {
            *(pixel_ptr + i * (SCREEN_WIDTH + (512 - SCREEN_WIDTH)) + j) = back_color;
        }
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

// FUNCTION MADE BY GITHUB COPILOT FOR DEBUGGING PURPOSES
void integerToHEX(int value){
    volatile int * HEX3_HEX0_ptr = (int *)HEX3_HEX0_BASE;
    volatile int * HEX5_HEX4_ptr = (int *)HEX5_HEX4_BASE;
    const unsigned char seven_seg_decode_table[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
        0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};
    unsigned char hex_segs[] = {0, 0, 0, 0, 0, 0, 0, 0};
    unsigned int shift_buffer, nibble;
    unsigned char code;
    int i;
    shift_buffer = value;
    for (i = 0; i < 8; ++i) {
        nibble = shift_buffer & 0x0000000F; // character is in rightmost nibble
        code = seven_seg_decode_table[nibble];
        hex_segs[i] = code;
        shift_buffer = shift_buffer >> 4;
    }
    *(HEX3_HEX0_ptr) = *(int *)(hex_segs);
    *(HEX5_HEX4_ptr) = *(int *)(hex_segs + 4);
}

#define maxGameObjects 10
/*
enum gameStates{
    MENU,
    GAME,
    RESET,
    GAMEOVER,
};*/

int main(void)
{
    *(PS2_ptr) = 0xFF; // reset
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    volatile int *switch_ptr = (int *) SWITCH_BASE; 
    volatile int *LEDR_ptr = (int *) LEDR_BASE;

    struct bytePacket ps2BytePacket1;
    ps2BytePacket1.usingMouse = false;
    ps2BytePacket1.byte1 = 0;
    ps2BytePacket1.byte2 = 0;
    ps2BytePacket1.byte3 = 0;
    ps2BytePacket1.byteCounter = 0;

    struct bytePacket ps2BytePacket2;
    ps2BytePacket2.usingMouse = false;
    ps2BytePacket2.byte1 = 0;
    ps2BytePacket2.byte2 = 0;
    ps2BytePacket2.byte3 = 0;
    ps2BytePacket2.byteCounter = 0;

    struct keyStates currentKeyState;
    currentKeyState.upPressed = false;
    currentKeyState.downPressed = false;
    currentKeyState.leftPressed = false;
    currentKeyState.rightPressed = false;

    struct gameObject player1;
    player1.sprite = &player;
    player1.dimensions.x = player1.sprite->width * player1.sprite->scaling;
    player1.dimensions.y = player1.sprite->height * player1.sprite->scaling;
    player1.isInvisible = false;
    player1.isFixedToScreen = false;
    player1.isCollidable = true;
    player1.isStatic = false;
    player1.isPlayer = true;
    player1.isEnemy = false;


    struct gameObject enemy1;
    enemy1.sprite = &player;
    enemy1.dimensions.x = enemy1.sprite->width * enemy1.sprite->scaling;
    enemy1.dimensions.y = enemy1.sprite->height * enemy1.sprite->scaling;
    enemy1.isInvisible = false;
    enemy1.isFixedToScreen = false;
    enemy1.isCollidable = true;
    enemy1.isStatic = false;
    enemy1.isPlayer = false;
    enemy1.isEnemy = true;


    //enum gameState gS = GAME;

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
	currentKeyState.exitPressed = true;

    int jumpAudioCounter = 0;
    int curAudio = 0;

    struct gameObject* objects[maxGameObjects] = {
        &player1,
        &enemy1,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &mouseObject,
    };

	//short int line_color = 0x0;
    while (1)
    {
        int switch_value = *switch_ptr & 0x3FF; // read 10-bit switches


        int audioVal = audio_value();
        *LEDR_ptr = 1;
        //int currentPow = switch_value*100;
        if(audioVal > HIGH_AUDIO){
            audioVal *= 1024;
            audioVal /= HIGH_AUDIO;
            *LEDR_ptr = audioVal;
        }

        draw_background(true);
        for(int i = 0; i < maxGameObjects; i++){
            if(objects[i] != NULL){
                draw_sprite(objects[i], true);
            }
        }

        //start logic
        handle_ps2(PS2_BASE, &ps2BytePacket1, &currentKeyState);
        handle_ps2(PS2_BASE2, &ps2BytePacket2, &currentKeyState);

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
        integerToHEX(ledValue);


        if(currentKeyState.exitPressed){
            player1.position.x = SCEEN_BOUNDS;
            player1.position.y = 0;
            player1.velocity.x = 0;
            player1.velocity.y = 0;
            player1.acceleration.x = 0;
            player1.acceleration.y = 1;

            enemy1.position.x = 100;
            enemy1.position.y = 0;
            enemy1.velocity.x = 0;
            enemy1.velocity.y = 0;
            enemy1.acceleration.x = 0;
            enemy1.acceleration.y = 1;

            screenObject.position.x = 0;

            currentKeyState.exitPressed = false;
            clear_screen();
        }

        if(currentKeyState.upPressed){
            player1.velocity.y = -10;
            currentKeyState.upPressed = false;
            curAudio = JUMP_AUDIO_DELAY;
            jumpAudioCounter = 100;
            play_audio(&curAudio, 1000);
        }

        if (!(currentKeyState.rightPressed ^ currentKeyState.leftPressed))
        {
            player1.velocity.x = 0;
        }
        else if(currentKeyState.leftPressed){
            player1.velocity.x = -5;
        }
        else if(currentKeyState.rightPressed){
            player1.velocity.x = 5;
        }

        for(int i = 0; i < maxGameObjects; i++){
            if(objects[i] != NULL && !(objects[i]->isStatic)){
                objects[i]->position.x += objects[i]->velocity.x;
                objects[i]->position.y += objects[i]->velocity.y;
                objects[i]->velocity.x += objects[i]->acceleration.x;
                objects[i]->velocity.y += objects[i]->acceleration.y;
            }
        }

        if(player1.position.x + player1.dimensions.x - screenObject.position.x > SCREEN_WIDTH-SCEEN_BOUNDS){
            screenObject.position.x = player1.position.x + player1.dimensions.x  - SCREEN_WIDTH + SCEEN_BOUNDS;
        }
        else if(player1.position.x - screenObject.position.x < SCEEN_BOUNDS){
            screenObject.position.x = player1.position.x - SCEEN_BOUNDS;
        }

        if(player1.position.y >= 200){
            player1.position.y = 200;
            player1.velocity.y = 0;
        }
        if(enemy1.position.y >= 200){
            enemy1.position.y = 200;
            enemy1.velocity.y = 0;
        }


        //end game logic

        draw_background(false);
        for(int i = maxGameObjects - 1; i >=0; i--){
            if(objects[i] != NULL && !objects[i]->isInvisible){
                draw_sprite(objects[i], false);
            }
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
