#include <stdio.h>
#include <SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL_video.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;

} sdl_t;

typedef struct {
    uint32_t scale_factor;          // Amount to scale the 64x32 display of CHIP8 
    uint32_t window_width;          // width of the SDL window
    uint32_t window_height;         // height of the SDL window
    uint32_t foreground_color;      // foreground color of sprites
    uint32_t background_color;      // background color of window
    uint32_t instr_rate;            // number of instructions per second to be run 
} config_t;

typedef enum {
    QUIT,
    PAUSED,
    RUNNING,
} emulator_state_t;

typedef struct {
    emulator_state_t state;
    uint8_t RAM[4096];
    bool display[64*32]; // original chip8 resolution
    uint16_t stack[12];
    uint8_t SP;
    uint8_t V[16];
    uint16_t I;
    uint16_t PC;
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool keypad[16];
    char *rom_path;

} chip8_t;

bool set_config(config_t* config, chip8_t* chip8, const int argc, char **argv){
    *config = (config_t){
        .scale_factor = 20,                 // value to scale the display of CHIP8 by
        .window_width = 64,                 // width of SDL window
        .window_height = 32,                // height of SDL window
        .foreground_color = 0xFFFFFFFF,     // white
        .background_color = 0x00000000,     // black 
        .instr_rate = 700,                 // Number of instructions to run in 1 second
    };
    if(argc > 1) {
        for (int i=0; i < argc; i++){
            if(strcmp(argv[i], "-w")==0){
                if( i+1 < argc){
                    char* str = argv[i+1];
                    for(int j=0; str[j] != '\0'; j++){
                        if(str[j]-'0' <0 || str[j] > '9'){
                            perror("Invalid width parameter\n");
                            return false;
                        }
                    }
                    config->window_width = atoi(argv[i+1]);
                } else {
                    perror("Unspecified width flag");
                    return false;
                }
            }

            else if(strcmp(argv[i], "-h")==0){
                if( i+1 < argc){
                    char* str = argv[i+1];
                    for(int j=0; str[j] != '\0'; j++){
                        if(str[j]-'0' <0 || str[j] > '9'){
                            perror("Invalid height parameter\n");
                            return false;
                        }
                    }
                    config->window_height = atoi(argv[i+1]);
                } else {
                    perror("Unspecified height flag");
                    return false;
                }
            }

            else if(strcmp(argv[i], "-p")==0) {
                if( i+1 < argc){
                    
                    chip8->rom_path = argv[i+1];
                } else {
                    perror("Uncpecified ROM path");
                    return false;
                }
            }
            else if (strcmp(argv[i], "-sf") == 0) {
                if( ++i < argc){
                    
                    config->scale_factor = (uint32_t)strtol(argv[i], NULL, 10);
                } else {
                    printf("Unspecified scale factor, default is 20\n");
                }
            }
        }
        
    }
    return true;
}

bool init_chip8(chip8_t *chip8){
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0   
        0x20, 0x60, 0x20, 0x20, 0x70,   // 1  
        0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2 
        0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,   // 4    
        0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,   // F
    };
    //memset(chip8, 0, sizeof(chip8_t));
    memcpy(&(chip8->RAM[0]), font, sizeof(font));

    FILE *rom = fopen(chip8->rom_path, "rb");
    if(rom==NULL){
        perror("Failed to read the specified ROM file");
        return false;
    }
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->RAM - 0x200; 
    rewind(rom);
    if(rom_size > max_size) {
        SDL_Log("ROM file %s is too big; ROM size: %zu; Max size allowed %zu\n", chip8->rom_path, rom_size, max_size);
        return false;
    }

    if (fread(&(chip8->RAM[0x200]), rom_size, 1, rom) !=1) {
        SDL_Log("Could not read ROM file into chip8 memory");
        return false;
    }
    fclose(rom);
    chip8->state = RUNNING;
    chip8->PC = 0x200;
    chip8->SP = 0;
    return true;
}

bool init_sdl(sdl_t* sdl, const config_t config){
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)!= 0){
        SDL_Log("Could not initialize SDL subsytem: %s\n", SDL_GetError());
        return false;
    } 
    printf("bruh\n");
    sdl->window = SDL_CreateWindow("CHIP8-Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config.window_width * config.scale_factor, config.window_height * config.scale_factor, 0);
    if(sdl->window == NULL){
        SDL_Log("Could not create window: %s\n", SDL_GetError());
        return false;
    }
    printf("2\n");
    if((sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED))== NULL){
        SDL_Log("Could not create renderer: %s\n", SDL_GetError());
        return false;
    }
    printf("3\n");
    return true;
}

void finish_sdl(sdl_t* sdl){
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    atexit(SDL_Quit);
}

void clear_screen(const config_t config, sdl_t* sdl){
    uint8_t r = (config.background_color >> 24) & 0xFF;
    uint8_t g = (config.background_color >> 16) & 0xFF;
    uint8_t b = (config.background_color >> 8) & 0xFF;
    uint8_t a = (config.background_color >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl->renderer, r, g, b, a);
    SDL_RenderClear(sdl->renderer);

}

void update_screen(sdl_t* sdl, const config_t config, const chip8_t* chip8){
    SDL_Rect rect = {.x=0, .y=0, .w=config.scale_factor, .h = config.scale_factor};

    // foreground color
    uint8_t fg_r = (config.foreground_color >> 24) & 0xFF;
    uint8_t fg_g = (config.foreground_color >> 16) & 0xFF;
    uint8_t fg_b = (config.foreground_color >> 8) & 0xFF;
    uint8_t fg_a = (config.foreground_color >> 0) & 0xFF;

    //background color
    uint8_t bg_r = (config.background_color >> 24) & 0xFF;
    uint8_t bg_g = (config.background_color >> 16) & 0xFF;
    uint8_t bg_b = (config.background_color >> 8) & 0xFF;
    uint8_t bg_a = (config.background_color >> 0) & 0xFF;
    for(uint32_t i=0; i< sizeof chip8->display; i++){
        rect.x = config.scale_factor* (i % config.window_width);
        rect.y = config.scale_factor*(i / config.window_width);

        if (chip8->display[i]) {
            SDL_SetRenderDrawColor(sdl->renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl->renderer, &rect);
        } else {
            SDL_SetRenderDrawColor(sdl->renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl->renderer, &rect);
        }
    }
    SDL_RenderPresent(sdl->renderer);
}

void handle_input(chip8_t* chip8){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_QUIT: chip8->state = QUIT; break;
            case SDL_KEYDOWN: 
                switch(event.key.keysym.sym){
                    case SDLK_ESCAPE: chip8->state = QUIT; break;
                    case SDLK_SPACE: 
                        if(chip8->state == PAUSED) {
                            chip8->state = RUNNING;
                        } else if(chip8->state == RUNNING) {
                            chip8->state = PAUSED;
                            puts("==Paused==");
                        } else {
                            perror("Somehow state QUIT was reached\n");
                            exit(EXIT_FAILURE);
                        } break;
                    case SDLK_0: chip8->keypad[0x00] = true; break;
                    case SDLK_1: chip8->keypad[0x01] = true; break;
                    case SDLK_2: chip8->keypad[0x02] = true; break;
                    case SDLK_3: chip8->keypad[0x03] = true; break;
                    case SDLK_4: chip8->keypad[0x04] = true; break;
                    case SDLK_5: chip8->keypad[0x05] = true; break;
                    case SDLK_6: chip8->keypad[0x06] = true; break;
                    case SDLK_7: chip8->keypad[0x07] = true; break;
                    case SDLK_8: chip8->keypad[0x08] = true; break;
                    case SDLK_9: chip8->keypad[0x09] = true; break;
                    case SDLK_a: chip8->keypad[0x0A] = true; break;
                    case SDLK_b: chip8->keypad[0x0B] = true; break;
                    case SDLK_c: chip8->keypad[0x0C] = true; break;
                    case SDLK_d: chip8->keypad[0x0D] = true; break;
                    case SDLK_e: chip8->keypad[0x0E] = true; break;
                    case SDLK_f: chip8->keypad[0x0F] = true; break;
                    default: break;
                } break;
            case SDL_KEYUP:
                switch(event.key.keysym.sym){
                    case SDLK_0: chip8->keypad[0x00] = false; break;
                    case SDLK_1: chip8->keypad[0x01] = false; break;
                    case SDLK_2: chip8->keypad[0x02] = false; break;
                    case SDLK_3: chip8->keypad[0x03] = false; break;
                    case SDLK_4: chip8->keypad[0x04] = false; break;
                    case SDLK_5: chip8->keypad[0x05] = false; break;
                    case SDLK_6: chip8->keypad[0x06] = false; break;
                    case SDLK_7: chip8->keypad[0x07] = false; break;
                    case SDLK_8: chip8->keypad[0x08] = false; break;
                    case SDLK_9: chip8->keypad[0x09] = false; break;
                    case SDLK_a: chip8->keypad[0x0A] = false; break;
                    case SDLK_b: chip8->keypad[0x0B] = false; break;
                    case SDLK_c: chip8->keypad[0x0C] = false; break;
                    case SDLK_d: chip8->keypad[0x0D] = false; break;
                    case SDLK_e: chip8->keypad[0x0E] = false; break;
                    case SDLK_f: chip8->keypad[0x0F] = false; break;
                    default: break;
                } break;
            default: break;
                
        }
    }
}

void emulate_instruction(chip8_t* chip8, config_t config){
    // get the instruction to be executed
    // I have a little endian machine, so the program would first get the larger 8 bits
    // then they need to be shifted and bitwise ORed with the lower 8 bits stored at the next memory address
    uint16_t instr = (chip8->RAM[chip8->PC] << 8) | chip8->RAM[(chip8->PC) + 1];
    
    // increment program counter for the next 16 bit instruction
    chip8->PC += 2;

    uint16_t first4bits = (instr & 0xF000) >> 12;
    uint16_t NNN = (instr & 0x0FFF);
    uint16_t NN = (instr & 0x00FF);
    uint16_t X = (instr & 0x0F00) >> 8;
    uint16_t Y = (instr & 0x00F0) >> 4;
    uint16_t N = (instr & 0x000F);
    switch(first4bits) {
        case 0x00: 
            if (NN == 0xE0) {
                memset(&chip8->display[0], false, sizeof chip8->display);
            } else if(NN == 0xEE) {
                chip8->SP--;
                chip8->PC = chip8->stack[chip8->SP];
                
            }
            break;
        case 0x01: 
            // Opcode is 1NNN: Jumps to address NNN
            chip8->PC = NNN;
            break;
        case 0x02: 
            // Opcode is 2NNN: Calls subroutin at NNN
            
            // only allow a maximum recurive detpth of 12
            if(chip8->SP < 12){
                chip8->stack[chip8->SP] = chip8->PC;
                chip8->SP ++;
                chip8->PC = NNN;
                return;
            } else {
                perror("Stack overflow");
                exit(EXIT_FAILURE);
            }
            
        case 0x03: 
            // Opcode is 3XNN: skips next instruction if VX == NN
            if(chip8->V[X] == NN) {
                chip8->PC += 2;
            }
            return;
        case 0x04: 
            // Opcode is 4XNN: skips next instruction if VX != NN
            if(chip8->V[X] != NN) {
                chip8->PC += 2;
            }
            return;
        case 0x05:
            // Opcode is 5XY0: skips next instruction if VX == VY
            if(chip8->V[X] == chip8->V[Y]){
                chip8->PC += 2;
            }
            return;
        case 0x06: 
            // Opcode is 6XNN: sets value of register VX to NN
            chip8->V[X] = NN;
            return;
        case 0x07: 
            // Opcode is 7XNN: increments value of register VX to NN
            chip8->V[X] += NN;
            return;
        case 0x08: 
            switch(N) {
                //Opcode is 8XY0: set register VX to the value of VY
                case 0x00: chip8->V[X] = chip8->V[Y]; break;
                // Opcode is 8XY1: set register VX to VX OR VY
                case 0x01: chip8->V[X] |= chip8->V[Y]; break;
                // Opcode is 8XY2: set register VX to VX AND VY
                case 0x02: chip8->V[X] &= chip8->V[Y]; break;
                // Opcode is 8XY3: set register VX to VX XOR VY
                case 0x03: chip8->V[X] ^= chip8->V[Y]; break;
                // Opcode is 8XY4: set register VX to VX + VY, if overflow, set VF to 1, otherwise to 0
                case 0x04: 
                    if(chip8->V[X] + chip8->V[Y] > 0xFF) {
                        chip8->V[0x0F] = 0x01;
                    } else {
                        chip8->V[0x0F] = 0x00;
                    } 
                    chip8->V[X] += chip8->V[Y];
                    break;
                // Opcode is 8XY5: set register VX to VX - VY, if underflow, set VF to 0, otherwise to 1
                case 0x05: 
                    if(chip8->V[X] >= chip8->V[Y]) {
                        chip8->V[0x0F] = 0x01;
                        
                    } else {
                        chip8->V[0x0F] = 0x00;
                    }    
                    chip8->V[X] -= chip8->V[Y];
                    break;
                // Opcode is 8XY6: set register VX to VX >>1 (shift to right by 1); set VF to the last bit of VX
                case 0x06: chip8->V[0x0F] = chip8->V[X] & 0x01; chip8->V[X] >>= 1; break;
                // Opcode is 8XY7: set register VX to VY - VX, if underflow, set VF to 0, otherwise to 1
                case 0x07: 
                    if(chip8->V[Y] >= chip8->V[X]) {
                        chip8->V[0x0F] = 0x01;
                    } else {
                        
                        chip8->V[0x0F] = 0x00;
                    } 
                    chip8->V[X] = chip8->V[Y] - chip8->V[X];
                    break;

                case 0x0E: chip8->V[0x0F] = (chip8->V[X] & 0x80)>>7; chip8->V[X] <<= 1; break;
                default: printf("Invalid opcode 0x8%d%d%d\n", X, Y, N);
            }    
            return;
        case 0x09: 
            // Opcode is 9XY0: skips next instruction if VX != VY
            if(chip8->V[X] != chip8->V[Y]){
                chip8->PC += 2;
            }
            return;
        case 0xA: 
            // Opcode is ANNN: sets the register I to value NNN
            chip8->I = NNN;
            return;
        case 0xB: 
            // Opcode is BNNN: sets the value of the program counter to V0 plus NNN
            chip8->PC = chip8->V[0] + NNN;
            return;
        case 0xC: 
            // Opcode is CXNN: sets value of register VX to the bitwise and of NN and a random number between 0 and 255
            chip8->V[X] = NN & (rand() % 256 + 1);
            return;
        case 0xD: 
            // Opcode is DXYN: reads N bytes from memory, starting at position I and XORs them with the display bits starting at coordinate
            // X, Y. If any pixel is erase/ set off, VF is set to 1, otherwise, it is set to 0. If part of the sprite is outside the display, it 
            // wraps around the display.
            uint8_t X_coord = chip8->V[X] % config.window_width;
            uint8_t Y_coord = chip8->V[Y] % config.window_height;
            uint8_t X_original = X_coord;
            chip8->V[0x0F] = 0;
            for (uint8_t i =0; i< N; i++) {
                const uint8_t sprite = chip8->RAM[chip8->I + i];
                X_coord = X_original;
                for (int8_t j=7; j>=0; j--) {
                    // get the bit of the sprite and the corresponding one on the display 
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord];
                    bool sprite_bit = sprite & (1 << j);
                    if (sprite_bit && *pixel) {
                        chip8->V[0x0F] = 1;
                    }
                    *pixel ^= sprite_bit;

                    if(++X_coord >= config.window_width) break;
                }
                if(++Y_coord >= config.window_height) break;
            }
            break;
        case 0xE:
            // Opcode is EX9E: skip next instruction if key stored in VX is pressed
            if(NN == 0x9E) {
                if(chip8->keypad[chip8->V[X]]){
                    chip8->PC += 2;
                }
            // Opcode is EXA1: skip next instruction if key stored in VX is not pressed
            } else if (NN == 0xA1){
                if(!chip8->keypad[chip8->V[X]]){
                    chip8->PC += 2;
                }
            }
            break;
        case 0xF: 
             
            switch(NN) {
                // Opcode is FX07: sets value of ragister VX to the value of the delay timer
                case 0x07:
                    chip8->V[X] = chip8->delay_timer;
                    break;
                // Opcode is FX0A: awaits a keypress and blocks, then stores key value in VX
                case 0x0A:
                    // static variables to handle the keypress
                    static bool is_key_pressed = false;
                    static uint8_t key = 0xFF;
                    // check if any key on the keypad was pressed
                    for(uint8_t i =0; i < 16 && key == 0xFF; i++) {
                        if(chip8->keypad[i]){
                            key = i;
                            is_key_pressed = true;
                            break;
                        }
                    }
                    // if no key was pressed, repeat this instruction 
                    if (!is_key_pressed){
                        chip8->PC -= 2;
                    } else {
                        
                        // if key is still pressed, repeat instruction
                        if(chip8->keypad[key]){
                            chip8->PC -= 2;
                        } else {
                            chip8->V[X] = key;
                            key = 0xFF;
                            is_key_pressed = false;
                        }
                    }
                    break;
                
                // Opcode is FX15: set the delay timer to VX
                case 0x15:
                    chip8->delay_timer = chip8->V[X];
                    break;
                // Opcode is FX18: set the sound timer to VX
                case 0x18:
                    chip8->sound_timer = chip8->V[X];
                    break;
                // Opcode is FX1E: set I = I + VX
                case 0x1E:
                    chip8->I += chip8->V[X];
                    break;
                // Opcode is FX29: set I to the location in memory of the sprite for the character in VX
                case 0x29:
                    chip8->I = 5 * chip8->V[X];
                    break;
                // Opcode is FX33: stores the binary coded decimal representation of VX with hundreds digit at I, tens at I+1, ones digit at I+2
                case 0x33:
                    chip8->RAM[chip8->I] = chip8->V[X] / 100;
                    chip8->RAM[chip8->I +1] = (chip8->V[X] / 10) % 10;
                    chip8->RAM[chip8->I +2] = chip8->V[X] % 10;
                    break;
                // Opcode is FX55: stores V0 - VX in memory starting at address I
                case 0x55:
                    for(uint8_t i =0; i<= X; i++) {
                        chip8->RAM[chip8->I + i] = chip8->V[i];
                    }
                    break;
                // Opcode is FX65: fills V0-VX with values from memory starting at I
                case 0x65:
                    for(uint8_t i =0; i<= X; i++) {
                        chip8->V[i] = chip8->RAM[chip8->I + i];
                    }
                    break;
            }   
            break;

    }
}

void update_timers(chip8_t* chip8){
    if(chip8->delay_timer > 0){
        chip8->delay_timer--;
    }
    if(chip8->sound_timer > 0) {
        chip8->sound_timer--;
    } else {
        
    }
}

int main(int argc, char **argv){
    sdl_t sdl= {0};
    config_t config = {0};
    chip8_t chip8 = {0};
    srand(time(NULL));

    if(!set_config(&config, &chip8 ,argc, argv)){
        exit(EXIT_FAILURE);
    }

    if(!init_chip8(&chip8)){
        exit(EXIT_FAILURE);
    }
    
    // Initialize SDL subsystem 
    if(!init_sdl(&sdl, config)){
        exit(EXIT_FAILURE);
    }
    
    clear_screen(config, &sdl);
    while(chip8.state != QUIT){
        // handle any input the user might have done first
        handle_input(&chip8);
        // if paused, then simply halt all execution
        if(chip8.state == PAUSED) continue;
        // get the time before running the instructions
        uint64_t before_frame = SDL_GetPerformanceCounter();
        // execute config.instr_rate instructions per second
        for(uint32_t i=0; i < config.instr_rate / 60; i++){
            emulate_instruction(&chip8, config);
        }
        // get the time after running the instructions
        uint64_t after_frame = SDL_GetPerformanceCounter();
        // delay for 60Hz or actual elapsed time
        const double time_elapsed = (double)((after_frame - before_frame) * 1000) / SDL_GetPerformanceFrequency();
        // delay the display
        SDL_Delay(16.67f > time_elapsed ? 16.67f - time_elapsed: 0);
        // update the display
        update_screen(&sdl, config, &chip8);
        // update the delay and sound timers
        update_timers(&chip8);
    }
    // properly close all SDL initalizers and end the program
    finish_sdl(&sdl);

    return 0;
}

