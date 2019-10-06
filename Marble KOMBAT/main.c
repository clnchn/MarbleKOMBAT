// MARBLE KOMBAT
// Hashem Botma, Hsuan Ling Chen

#include <lpc17xx.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <rtl.h>
#include <math.h>
#include <time.h>
#include "uart.h"
#include "GLCD.h"
#include "led.h"
#include "timer.h"

#define ever ;;

// Macro Functions -----------------------------------------------------------------------------------------------------
#define RADIANS(theta)  (theta * 3.141592654 / 180)     // Degrees to radians
#define SQUARE(x)       ((x) * (x))                     // Squares a number

// Typedefs ------------------------------------------------------------------------------------------------------------
// Colour enum type for marbles
typedef enum {
    BLACK = 0x0000,
    RED,
    BLUE,
    GREEN,
    YELLOW,
    MAGENTA,
    WHITE = 0xFFFF,
} Colour;

// Game state enum type for starting and ending game
typedef enum {
    TITLE_SCREEN,
    GAME_ON,
    FATALITY,
    YOU_DIED,
} Game_State;

// Marble struct type
typedef struct Marble Marble;
struct Marble {
    float x;
    float y;
    Colour colour;
    Marble *next; // For marble linked list usage
};

// Constants -----------------------------------------------------------------------------------------------------------
#define NUM_STARTING_MARBLES    7               // Marbles in train at start
#define MARBLE_DIAMETER         16
#define TRAIN_SPEED             8               // Pixels per second
#define BULLET_SPEED            125             // Pixels per second
#define WINDOW_X                240             // Pixel width
#define WINDOW_Y                320             // Pixel height

#define BACKGROUND_COLOUR       BLACK           // For clearing screen and patching
#define TEXT_COLOUR             WHITE
#define CANNON_COLOUR           WHITE 
#define CANNON_X                25              // X position of cannon
#define CANNON_Y                WINDOW_Y / 2    // Y position of cannon

// Global variables ----------------------------------------------------------------------------------------------------
// Mutexes for data sharing
OS_MUT mut_LED, mut_LCD, mut_pot, mut_joy;

// Inputs
Game_State state;
uint32_t pot_position;

// Global marbles
Marble *train_root; // The root of the marble linked list
Marble *bullet; // The projectile marble

// Game logic
bool shoot_marble, swap_marble, marble_airborne, bullet_collision;
int cannon_angle;
float firing_angle;
Colour chambered_colour, spare_colour;

// Score tracking
uint32_t score, score_multiplier;

// Random number generation
uint16_t seed;
unsigned bit;

// Graphics
bool clear_screen;
float bullet_patch_x, bullet_patch_y;
uint16_t marble_draw_bmp_pri[144];
uint16_t marble_draw_bmp_sec[12];
uint16_t marble_patch_bmp[96];

// Text display
char score_str[3], title_str[] = "MARBLE KOMBAT", inst_str[] = "PRESS BUTTON TO BEGIN", gg_win_str[] = "FATALITY!", gg_lose_str[] = "YOU DIED", gg_score_str[] = "SCORE: ";
  
// Functions -----------------------------------------------------------------------------------------------------------
// Place the marble at a position x,y
void Position_Marble(Marble *marble, float x, float y) {
    marble->x = x;
    marble->y = y;
}

// Move the marble by an amount x,y
void Move_Marble(Marble* marble, float x, float y) {
    marble->x += x;
    marble->y += y;
}

// Obtain the primary colour code given a colour enum
uint16_t Get_Primary_Hex(Colour named_colour) {
    switch (named_colour) {
        case WHITE: return 0xFFFF;
        case RED: return 0xF800;
        case BLUE: return 0x05DF;
        case GREEN: return 0x5DA0;
        case YELLOW: return 0xFE60;
        case BLACK: return 0x0000;
        default: return 0xD01F; // Should never get here
    }
}

// Obtain the secondary colour code given a colour enum
uint16_t Get_Secondary_Hex(Colour named_colour) {
    switch (named_colour) {
        case WHITE: return 0xBDF7;
        case RED: return 0xFC10;
        case BLUE: return 0x51DF;
        case GREEN: return 0xA7E7;
        case YELLOW: return 0xFD00;
        case BLACK: return 0x0000;
        default: return 0xD01F; // Should never get here
    }
}

// Render the marble at its position
void Draw_Marble(Marble *marble) {
    int i;
    unsigned int x = marble->x - 8;
    unsigned int y = marble->y - 8;
   
    // Set the rendering bitmaps' colours to the marble's colour
    for (i = 0; i < 144; i++) {
        marble_draw_bmp_pri[i] = Get_Primary_Hex(marble->colour);
    }
    
    for (i = 0; i < 12; i++) {
        marble_draw_bmp_sec[i] = Get_Secondary_Hex(marble->colour);
    }
        
    // Render the circle using a series of bitmaps
    GLCD_Bitmap(x + 2,  y + 2,  12, 12, (unsigned char *)marble_draw_bmp_pri);
    
    GLCD_Bitmap(x + 5,  y,      6,  2,  (unsigned char *)marble_draw_bmp_sec);
    GLCD_Bitmap(x + 5,  y + 14, 6,  2,  (unsigned char *)marble_draw_bmp_sec);
    GLCD_Bitmap(x,      y + 5,  2,  6,  (unsigned char *)marble_draw_bmp_sec);
    GLCD_Bitmap(x + 14, y + 5,  2,  6,  (unsigned char *)marble_draw_bmp_sec);
    
    GLCD_Bitmap(x + 3,  y + 1,  2,  1,  (unsigned char *)marble_draw_bmp_pri);
    GLCD_Bitmap(x + 11, y + 1,  2,  1,  (unsigned char *)marble_draw_bmp_pri);
    GLCD_Bitmap(x + 3,  y + 14, 2,  1,  (unsigned char *)marble_draw_bmp_pri);
    GLCD_Bitmap(x + 11, y + 14, 2,  1,  (unsigned char *)marble_draw_bmp_pri);
    GLCD_Bitmap(x + 1,  y + 3,  1,  2,  (unsigned char *)marble_draw_bmp_pri);
    GLCD_Bitmap(x + 1,  y + 11, 1,  2,  (unsigned char *)marble_draw_bmp_pri);
    GLCD_Bitmap(x + 14, y + 3,  1,  2,  (unsigned char *)marble_draw_bmp_pri);
    GLCD_Bitmap(x + 14, y + 11, 1,  2,  (unsigned char *)marble_draw_bmp_pri);
}

// Render the bullet marble patcher for a marble at position x,y
void Draw_Bullet_Patch(float marble_x, float marble_y) {
    unsigned int x = marble_x - 12;
    unsigned int y = marble_y - 12;
    
    // Render the patcher using a series of bitmaps
    GLCD_Bitmap(x,      y + 4,  4, 16,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 3,  y,      14, 4,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 3,  y + 20, 14, 4,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 4,  y + 4,  1,  5,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 4,  y + 15, 1,  5,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 5,  y + 4,  1,  3,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 5,  y + 17, 1,  3,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 6,  y + 4,  1,  2,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 6,  y + 18, 1,  2,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 7,  y + 4,  2,  1,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 7,  y + 19, 2,  1,  (unsigned char *)marble_patch_bmp);
} 

// Render the train marble patcher for a marble at position x,y
void Draw_Train_Patch(float marble_x, float marble_y) {
    unsigned int x = marble_x - 8;
    unsigned int y = marble_y - 9;
    
    // Render the patcher using a series of bitmaps
    GLCD_Bitmap(x + 5,  y,      6,  1,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x,      y + 1,  5,  1,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 11, y + 1,  5,  1,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x,      y + 2,  3,  1,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 13, y + 2,  3,  1,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x,      y + 3,  2,  1,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 14, y + 3,  2,  1,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x,      y + 4,  1,  2,  (unsigned char *)marble_patch_bmp);
    GLCD_Bitmap(x + 15, y + 4,  1,  2,  (unsigned char *)marble_patch_bmp);
}   

// Render the cannon given its angle and the chambered and spare colours
void Draw_Cannon(float angle, Colour cannon_colour, Colour chambered_colour, Colour spare_colour) {
    // Used to draw the cannon
    Marble renderer = { CANNON_X, CANNON_Y, WHITE};

    // Set the drawing marble to the chambered colour and draws it
    renderer.colour = chambered_colour;
    Draw_Marble(&renderer);

    // Repeats for the spare colour, setting its position according to rotation
    Move_Marble(&renderer, -16 * cos(angle), -16 * sin(angle));
    renderer.colour = spare_colour;
    Draw_Marble(&renderer);

    // Draws the arm of the cannon based on its rotation
    renderer.colour = cannon_colour;
    Move_Marble(&renderer, 32 * cos(angle), 32 * sin(angle));
    Draw_Marble(&renderer);
    Move_Marble(&renderer, 16 * cos(angle), 16 * sin(angle));
    Draw_Marble(&renderer);
}

// Check two marbles for collision
bool Marble_Collision(Marble *m1, Marble *m2) {
    return (sqrt(SQUARE(m2->x - m1->x) + SQUARE(m2->y - m1->y)) < MARBLE_DIAMETER);
}

// Move the train root marble forward, and push all marbles in collision
void Move_Marble_Train(Marble *root, float time_elapsed) {
    // Start at root
    Marble *current = root;        
   
    // Move the root marble
    Move_Marble(current, 0, TRAIN_SPEED * time_elapsed);

    // If the next marble is in contact, move it ahead of the current marble
    // Stop when no marbles are left or in contact
    while (current->next != NULL && Marble_Collision(current, current->next)) {
        Position_Marble(current->next, current->x, current->y + MARBLE_DIAMETER - 1); // -1 to ensure collisions
        current = current->next;
    }
}

// Move the bullet marble, given the flight angle and time elapsed
// Also places the marble into the train if it makes a collision
// Returns true if flight ended, otherwise returns false
bool Move_Bullet(Marble *bullet_ptr, Marble **root, float angle, float time_elapsed) {
    Marble *current = *root, *prev = *root;
    
    // Move the marble in its flight vector
    Move_Marble(bullet_ptr, time_elapsed * BULLET_SPEED * cos(angle), time_elapsed * BULLET_SPEED * sin(angle));

    // Ensure marble is within screen boundaries
    if (bullet_ptr->x >= WINDOW_X - MARBLE_DIAMETER || bullet_ptr->x <= 0 + MARBLE_DIAMETER ||
        bullet_ptr->y >= WINDOW_Y - MARBLE_DIAMETER || bullet_ptr->y <= 0 + MARBLE_DIAMETER) {
        free(bullet_ptr);
        return true;
    }

    // Iterate through the marble train checking for collisions
    while (current != NULL) {
        if (Marble_Collision(bullet_ptr, current)) {
            // Collision found
            if (bullet_ptr->y < current->y) {
                // Bullet made a collision from the top
                // Update the linked list nodes to add the bullet
                if (current == *root) {
                    *root = bullet_ptr;
                }
                else {
                    prev->next = bullet_ptr;
                }
                bullet_ptr->next = current;
                
                // Move the bullet into position and shift the train
                Position_Marble(bullet_ptr, current->x, current->y);
                Move_Marble_Train(bullet_ptr, 0);
            }
            else {
                // Bullet made a collision from the bottom
                // Update the linked list nodes to add the bullet
                bullet_ptr->next = current->next;
                current->next = bullet_ptr;
                
                // Move the bullet into position and shift the train
                Position_Marble(bullet_ptr, current->x, current->y);
                Move_Marble_Train(current, 0);
            }

            return true;

        }
        else {
            prev = current;
            current = current->next;
        }
    }

    return false;
}

// Check the marble train for bullet matches and eliminate them
// Return true if marbles were collapsed, else returns false
bool Collapse_Marbles(Marble **root, Marble *bullet) {
    Colour collapse_colour = bullet->colour;
    Marble *current = *root, *pre_collapse = NULL,
           *temp_1, *temp_2;
    int num_to_collapse = 0, i;
    bool bullet_found = false, end_sequence = false;

    // Iterate through the marble train
    while (current != NULL) {
        // Check if the bullet has been found yet
        if (current == bullet) {
            bullet_found = true;
        }
        // Check if there is a disconnection (end of train, change in colour, or no collision)
        if (current->colour != collapse_colour || current->next == NULL ||
            current->next->colour != collapse_colour || !Marble_Collision(current, current->next)) {
            end_sequence = true;
        }
        // Check if the current marble should be collapsed
        if (current->colour == collapse_colour) {
            num_to_collapse++;
        }
        
        if (end_sequence) {
            // There is a disconnection
            if (bullet_found) {
                // Bullet is within the last sequence. Must collapse or exit
                if (num_to_collapse >= 3) {
                    // Collapse
                    // Handle the linked list nodes
                    if (pre_collapse == NULL) {
                        temp_1 = *root;
                        *root = current->next;
                    }
                    else {
                        temp_1 = pre_collapse->next;
                        pre_collapse->next = current->next;
                    }
                            
                    // Delete the marbles
                    for (i = 0; i < num_to_collapse; i++) {
                        temp_2 = temp_1->next;
                        free(temp_1);
                        temp_1 = temp_2;
                    }
                            
                    return true;
                }                  
                else {
                    // Exit
                    return false;
                }
            }
            else {
                // Bullet not found yet, go to the next sequence
                num_to_collapse = 0;
                pre_collapse = current;
                end_sequence = false;
            }
        }
        
        // Move to the next marble
        current = current->next;
    }
    
    return false;
}

// Generate a random number given a seed
uint16_t Random_Number() {
    bit  = ((seed >> 0) ^ (seed >> 2) ^ (seed >> 3) ^ (seed >> 5) ) & 1;
    return seed = (seed >> 1) | (bit << 15);
}

// Generate a random colour
Colour Generate_Colour() {
    switch (Random_Number() % 4) {
        case 0: return RED;
        case 1: return BLUE;
        case 2: return GREEN;
        case 3: return YELLOW;
        default: return MAGENTA; // Should never get here
    }
}

// ISRs ----------------------------------------------------------------------------------------------------------------
// Press button ISR
void EINT3_IRQHandler() {
    if (state == TITLE_SCREEN) {
        // Start game
        state = GAME_ON;
        clear_screen = true;
    }
    else {
        // Fire marble
        shoot_marble = true;
    }
    
    // Clear pending interrupt
    LPC_GPIOINT->IO2IntClr |= (1 << 10);
}

// Tasks ---------------------------------------------------------------------------------------------------------------
// Potentiometer read task
__task void Potentiometer_Read() {
    uint32_t reading = 0;
    
    // Initialize potentiometer and ADC converter
    LPC_PINCON->PINSEL1 &= ~(0x03 << 18);
    LPC_PINCON->PINSEL1 |= (0x01 << 18);
    LPC_SC->PCONP |= (1 << 12);
    LPC_ADC->ADCR = (1 << 2) |
                    (4 << 8) |
                    (1 << 21);
    
    // Repeatedly read and store the potentiometer value
    for(ever) {
        // Start new reading
        LPC_ADC->ADCR |= (1 << 24);
    
        // Await reading
        do {
            reading = LPC_ADC->ADGDR;
        } while (!(reading & 0x80000000));
        
        // Store in global variable
        os_mut_wait(&mut_pot, 0xFFFF); // --------------------------------------
        pot_position = (reading & 0x0000FFFF) >> 4;
        os_mut_release(&mut_pot); // -------------------------------------------
        
        os_tsk_pass();
    }
}

// Joystick read task
__task void Joystick_Read() {
    bool joystick_prev_held = false;
    
    // Initialize joystick
    LPC_GPIO1->FIODIR &= ~0x07900000;
    
    // Repeatedly check the joystick status and set a flag if it is moved in
    for(ever) {
        // If any of the joystick bits are 0, the joystick is held
        if (~LPC_GPIO1->FIOPIN & 0x07900000) {
            if (!joystick_prev_held) {
                // Store in global variables
                os_mut_wait(&mut_joy, 0xFFFF); // ------------------------------
                swap_marble = true;
                os_mut_release(&mut_joy); // -----------------------------------
            }
            joystick_prev_held = true;
        }
        else {
            joystick_prev_held = false;
        }
        
        os_tsk_pass();
    }
}

// Game logic handling task
__task void Game_Logic() {
    int i;
    float temp_angle = 0;
    float prev_time, time_elapsed = 0;
    Colour temp_colour;
    Marble *temp;
    
    // Initialize timer
    timer_setup();
    
    // Wait while the game starts
    while (state == TITLE_SCREEN) {
        os_tsk_pass();
    }
    
    // Generate random seed given the start time
    // Should be random due to the human factor
    seed = timer_read();
    
    // Set the score in the display string
    sprintf(score_str, "%03d", score);
    
    // Initialize the bullet marble
    bullet = malloc(sizeof(Marble));
    bullet->colour = Generate_Colour();
    Position_Marble(bullet, CANNON_X, CANNON_Y);
    
    // Generate the spare colour
    chambered_colour = bullet->colour;
    spare_colour = Generate_Colour();
    
    // Initialize the marble train root
    train_root = malloc(sizeof(Marble));
    Position_Marble(train_root, WINDOW_X - 40, MARBLE_DIAMETER);
    train_root->colour = Generate_Colour();
    temp = train_root;
    
    // Generate the marble train
    for (i = 1; i < NUM_STARTING_MARBLES; i++) {
        temp->next = malloc(sizeof(Marble));
        temp->next->colour = Generate_Colour();
        Position_Marble(temp->next, WINDOW_X - 40, i * MARBLE_DIAMETER - 1);
        temp = temp->next;
    }
    
    // Clear a button press bug and reset the timer
    shoot_marble = false;
    prev_time = timer_read() / 1000000.f;
        
    // Game loop
    for(ever) {
        // Elapsed time since last iteration
        time_elapsed = timer_read() / 1000000.f - prev_time;
        prev_time += time_elapsed;
        
        // Read the potentiometer position and convert to angle
        os_mut_wait(&mut_pot, 0xFFFF); // --------------------------------------
        temp_angle =  (4095 - pot_position) / 34.125 - 60;
        os_mut_release(&mut_pot); // -------------------------------------------
        
        // Read the joystick flag and swap the spare and chambered marbles
        os_mut_wait(&mut_joy, 0xFFFF); // --------------------------------------
        if (swap_marble) {
            swap_marble = false;
            temp_colour = spare_colour;
            spare_colour = chambered_colour;
            chambered_colour = temp_colour;
            
            if (!marble_airborne) {
                bullet->colour = chambered_colour;
            }
        }
        os_mut_release(&mut_joy); // -------------------------------------------
        
        os_mut_wait(&mut_LCD, 0xFFFF); // --------------------------------------
        // Move the marble train based on time elapsed
        Move_Marble_Train(train_root, time_elapsed);
        
        // Rotate the cannon based on the potentiometer angle
        cannon_angle = (int)temp_angle;
        
        // Move the bullet if it is airborne
        if (marble_airborne) {
            // Prevent bugs
            shoot_marble = false;
            
            // Move the bullet and check for bullet collision with the train
            bullet_collision = Move_Bullet(bullet, &train_root, firing_angle, time_elapsed);
            
            if (bullet_collision) {
                // Bullet joined the train
                // Check and conditionally collapse the marbles around the bullet
                if (Collapse_Marbles(&train_root, bullet)) {
                    // Gain ponits upon successful collapse
                    score += 10 * (score_multiplier + 1);
                    score_multiplier++;
                    sprintf(score_str, "%03d", score);
                }
                else {
                    // Lose multiplier
                    score_multiplier = 0;
                }
                
                // Generate a new marble
                marble_airborne = false;
                bullet = malloc(sizeof(Marble));
                bullet->colour = chambered_colour;
                Position_Marble(bullet, CANNON_X, CANNON_Y);
            }
            else {
                // Position the bullet patcher
                bullet_patch_x = bullet->x;
                bullet_patch_y = bullet->y;
            }
        }
        else if (shoot_marble) {
            // Marble fired
            // Set the flags and generate a new spare colour
            shoot_marble = false;
            marble_airborne = true;
            firing_angle = RADIANS(cannon_angle);
            chambered_colour = spare_colour;
            spare_colour = Generate_Colour();
        }
        
        // Win condition
        if (train_root == NULL) {
            // Train cleared, game won
            // Advance to the win screen
            state = FATALITY;
            clear_screen = true;
            os_mut_release(&mut_LCD); // ---------------------------------------
            break; // Out of the for(ever) loop
        }
        else {
            // Iterate through train to the front marble
            temp = train_root;
            while (temp->next != NULL) {
                temp = temp->next;
            }
            
            // Check front marble's position
            if (temp->y >= WINDOW_Y) {
                // Front marble passed the finish line, game lost
                // Advance to the loss screen
                state = YOU_DIED;
                clear_screen = true;
                os_mut_release(&mut_LCD); // -----------------------------------
                break; // Out of the for(ever) loop
            }
        }
        
        os_mut_release(&mut_LCD); // -------------------------------------------
        
        os_tsk_pass();
    }
    
    // Idle through the end screen
    while (true) {
        os_tsk_pass();
    }
}

// LCD graphics rendering task
__task void LCD_Display() {
    int k = 0;
    int new_angle = 0, old_angle = 0;
    Marble *temp;
    
    // Initialize LCD
    GLCD_Init();
    GLCD_Clear(BACKGROUND_COLOUR);
    GLCD_SetBackColor(BACKGROUND_COLOUR);
    GLCD_SetTextColor(TEXT_COLOUR);
    
    // Initialize marble patcher bitmap
    for (k = 0; k < 8; k++) {
        marble_patch_bmp[k] = BACKGROUND_COLOUR;
    }
        
    // Graphics loop
    for(ever) {
        os_mut_wait(&mut_LCD, 0xFFFF); // --------------------------------------
        
        // Clear screen if prompted
        if (clear_screen) {
            clear_screen = false;
            GLCD_Clear(BACKGROUND_COLOUR);
        }
        
        // Render title screen if in that state
        if (state == TITLE_SCREEN) {
            GLCD_DisplayString(5, 1, 1, (unsigned char*)title_str);
            GLCD_DisplayString(20, 10, 0, (unsigned char*)inst_str);
        }
        else if (state == GAME_ON) {
            // Clear the screen upon collision to fix graphics bugs
            if (bullet_collision) {
                GLCD_Clear(BACKGROUND_COLOUR);
                bullet_collision = false;
            }
        
            // Draw the cannon patcher
            new_angle = cannon_angle;
            if (new_angle != old_angle) {
                Draw_Cannon(RADIANS(old_angle), BACKGROUND_COLOUR, BACKGROUND_COLOUR, BACKGROUND_COLOUR);
            }
            old_angle = new_angle;
            
            // Draw the bullet patcher
            Draw_Bullet_Patch(bullet_patch_x, bullet_patch_y);
            
            // Draw the train patcher for each marble in the train
            temp = train_root;
            while (temp != NULL) {
                Draw_Train_Patch(temp->x, temp->y);
                temp = temp->next;
            }
            
            // Draw each marble in the train
            temp = train_root;
            while (temp != NULL) {
                Draw_Marble(temp);
                temp = temp->next;
            }
            
            // Draw the cannon
            Draw_Cannon(RADIANS(new_angle), CANNON_COLOUR, chambered_colour, spare_colour);
            
            // Draw the bullet
            if (marble_airborne) {
                Draw_Marble(bullet);
            }
            
            // Display the score
            GLCD_DisplayString(12, 0, 1, (unsigned char*)score_str);
            
        }
        else {
            // End screen
            if (state == FATALITY) {
                // Display victory screen
                GLCD_DisplayString(5, 4, 1, (unsigned char*)gg_win_str);
            }
            else {
                // Display loss screen
                GLCD_DisplayString(5, 3, 1, (unsigned char*)gg_lose_str);
            }
            
            // Display score
            GLCD_DisplayString(7, 3, 1, (unsigned char*)gg_score_str);
            GLCD_DisplayString(7, 10, 1, (unsigned char*)score_str);
        }
        
        os_mut_release(&mut_LCD); // -------------------------------------------
       
        os_tsk_pass();
    }
}

// LED display task
// Switches on two LEDs, from right to left, for each multiplier
__task void LED_Output() {
    // Initialize LEDs
    LED_setup();
    
    // Repeatedly display the score multiplier on the LEDs
    for(ever) {
        // Clear the LEDs
        LPC_GPIO1->FIOCLR |= 0xB0000000;
        LPC_GPIO2->FIOCLR |= 0x7C;
        
        os_mut_wait(&mut_LED, 0xFFFF); // --------------------------------------
        switch (score_multiplier) {
            case 0: LPC_GPIO2->FIOSET |= 0x60;
                    break;
            
            case 1: LPC_GPIO2->FIOSET |= 0x78;
                    break;
            
            case 2: LPC_GPIO1->FIOSET |= 0x80000000;
                    LPC_GPIO2->FIOSET |= 0x7C;
                    break;
            
            case 3: LPC_GPIO1->FIOSET |= 0xB0000000;
                    LPC_GPIO2->FIOSET |= 0x7C;
                    break;
            
            default: break;
        }
        os_mut_release(&mut_LED); // -------------------------------------------
        
        os_tsk_pass();
    }
}
    
// Parent task for initialization and start-up
__task void Startup_Task() {
    // Initialize semaphores
    os_mut_init(&mut_LED);
    os_mut_init(&mut_LCD);
    os_mut_init(&mut_pot);
    
    // Start all tasks
    os_tsk_create(Potentiometer_Read, 1);
    os_tsk_create(Joystick_Read, 1);
    os_tsk_create(Game_Logic, 1);
    os_tsk_create(LCD_Display, 1);
    os_tsk_create(LED_Output, 1);
    
    // Delete self
    os_tsk_delete_self();
}

// Main ----------------------------------------------------------------------------------------------------------------
int main() {
    // Initialize interrupts
    LPC_GPIOINT->IO2IntEnF |= (1 << 10);
    LPC_GPIOINT->IO2IntEnR &= ~(1 << 10);
    NVIC_EnableIRQ(EINT3_IRQn);
    
    // Start the start-up task
    os_sys_init(Startup_Task);
    return 0;
}





