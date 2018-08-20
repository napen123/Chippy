#include <stdio.h>
#include <stdint.h>

#include <time.h>

#include <SDL.h>
#undef main

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

#define INSTRUCTIONS_PER_FRAME 10

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef double f64;

static u8 builtinFont[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0,
    0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0,
    0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10,
    0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0,
    0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0,
    0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90,
    0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0,
    0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0,
    0xF0, 0x80, 0xF0, 0x80, 0x80
};

union
{
    u8 memory[0x1000];

    struct
    {
        u16 i;
        u16 pc;

        u8 sp;
        u8 waitingKey;
        u8 delayTimer;
        u8 soundTimer;

        u8 reg[16];
        u8 keys[16];
        u16 stack[12];
        u8 font[80];
        u8 display[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];
    };
} chippy;

static u8 calculate_draw(int index, u8 value)
{
    return ((chippy.display[index] ^= value) ^ value) & value;
}

static int get_random()
{
    int ret = 0;

    do
    {
        ret = rand();
    } while(ret >= 256 * (int) ((RAND_MAX + 1) / 256));

    ret /= (int) ((RAND_MAX + 1) / 256);

    return ret;
}

static int get_key(int scancode)
{
    switch(scancode)
    {
        case SDL_SCANCODE_1: return 0x1;
        case SDL_SCANCODE_2: return 0x2;
        case SDL_SCANCODE_3: return 0x3;
        case SDL_SCANCODE_4: return 0xC;
        case SDL_SCANCODE_Q: return 0x4;
        case SDL_SCANCODE_W: return 0x5;
        case SDL_SCANCODE_E: return 0x6;
        case SDL_SCANCODE_R: return 0xD;
        case SDL_SCANCODE_A: return 0x7;
        case SDL_SCANCODE_S: return 0x8;
        case SDL_SCANCODE_D: return 0x9;
        case SDL_SCANCODE_F: return 0xE;
        case SDL_SCANCODE_Z: return 0xA;
        case SDL_SCANCODE_X: return 0x0;
        case SDL_SCANCODE_C: return 0xB;
        case SDL_SCANCODE_V: return 0xF;
        default: return -1;
    }
}

static void load(const char* filename)
{
    FILE* file = fopen(filename, "rb");

    fseek(file, 0, SEEK_END);
    int length = ftell(file);
    rewind(file);
    fread(&chippy.memory[0x200], sizeof(u8), length, file);
    fclose(file);

    for(int i = 0; i < 80; i++)
        chippy.font[i] = builtinFont[i];

    chippy.pc = 0x200;

    srand(time(NULL));
}

static void step()
{
    u16 opcode = chippy.memory[chippy.pc] << 8 | chippy.memory[chippy.pc + 1];
    chippy.pc += 2;

    u16 nnn   = opcode & 0xFFF;
    u8 nibble = opcode & 0xF;
    u8 x      = (opcode >> 8) & 0xF;
    u8 y      = (opcode >> 4) & 0xF;
    u8 kk     = opcode & 0xFF;
    u8 top    = (opcode >> 12) & 0xF;

    switch(top)
    {
        case 0x0:
            if(nnn == 0xE0)
            {
                for(int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT / 8; i++)
                    chippy.display[i] = 0;
            }
            else if(nnn == 0xEE)
                chippy.pc = chippy.stack[chippy.sp-- % 12];
            break;
        case 0x1:
            chippy.pc = nnn;
            break;
        case 0x2:
            chippy.stack[++chippy.sp % 12] = chippy.pc;
            chippy.pc = nnn;
            break;
        case 0x3:
            if(kk == chippy.reg[x])
                chippy.pc += 2;
            break;
        case 0x4:
            if(kk != chippy.reg[x])
                chippy.pc += 2;
            break;
        case 0x5:
            if(chippy.reg[y] == chippy.reg[x])
                chippy.pc += 2;
            break;
        case 0x6:
            chippy.reg[x] = kk;
            break;
        case 0x7:
            chippy.reg[x] += kk;
            break;
        case 0x8:
            switch(nibble)
            {
                case 0x0:
                    chippy.reg[x] = chippy.reg[y];
                    break;
                case 0x1:
                    chippy.reg[x] |= chippy.reg[y];
                    break;
                case 0x2:
                    chippy.reg[x] &= chippy.reg[y];
                    break;
                case 0x3:
                    chippy.reg[x] ^= chippy.reg[y];
                    break;
                case 0x4:
                {
                    u16 ret = chippy.reg[x] + chippy.reg[y];

                    chippy.reg[x] = ret;
                    chippy.reg[0xF] = (ret >> 8);
                }
                    break;
                case 0x5:
                {
                    u16 ret = chippy.reg[x] - chippy.reg[y];

                    chippy.reg[x] = ret;
                    chippy.reg[0xF] = !(ret >> 8);
                }
                    break;
                case 0x6:
                    chippy.reg[0xF] = chippy.reg[y] & 1;
                    chippy.reg[x] = chippy.reg[y] >> 1;
                    break;
                case 0x7:
                {
                    u16 ret = chippy.reg[y] - chippy.reg[x];

                    chippy.reg[x] = ret;
                    chippy.reg[0xF] = !(ret >> 8);
                }
                    break;
                case 0xE:
                    chippy.reg[0xF] = chippy.reg[y] >> 7;
                    chippy.reg[x] = chippy.reg[y] << 1;
                    break;
            }
            break;
        case 0x9:
            if(chippy.reg[y] != chippy.reg[x])
                chippy.pc += 2;
            break;
        case 0xA:
            chippy.i = nnn;
            break;
        case 0xB:
            chippy.pc = nnn + chippy.reg[0];
            break;
        case 0xC:
            chippy.reg[x] = get_random() & kk;
            break;
        case 0xD:
        {
            unsigned changed = 0;

            for(unsigned xx = chippy.reg[x], yy = chippy.reg[y]; nibble--; )
                changed |= calculate_draw((xx % DISPLAY_WIDTH + (yy + nibble) % DISPLAY_HEIGHT * DISPLAY_WIDTH) / 8, chippy.memory[(chippy.i + nibble) & 0xFFFF] >> (xx % 8))
                        |  calculate_draw(((xx + 7) % DISPLAY_WIDTH + (yy + nibble) % DISPLAY_HEIGHT * DISPLAY_WIDTH) / 8, chippy.memory[(chippy.i + nibble) & 0xFFFF] << (8 - xx % 8));

            chippy.reg[0xF] = (changed != 0);
        }
            break;
        case 0xE:
            if(kk == 0x9E)
            {
                if(chippy.keys[chippy.reg[x] & 15] == SDL_PRESSED)
                    chippy.pc += 2;
            }
            else if(kk == 0xA1)
            {
                if(chippy.keys[chippy.reg[x] & 15] == SDL_RELEASED)
                    chippy.pc += 2;
            }
            break;
        case 0xF:
            switch(kk)
            {
                case 0x07:
                    chippy.reg[x] = chippy.delayTimer;
                    break;
                case 0x0A:
                    chippy.waitingKey = 0x80 | x;
                    break;
                case 0x15:
                    chippy.delayTimer = chippy.reg[x];
                    break;
                case 0x18:
                    chippy.soundTimer = chippy.reg[x];
                    break;
                case 0x1E:
                {
                    u32 ret = (chippy.i & 0xFFFF) + chippy.reg[x];

                    chippy.reg[0xF] = ret >> 24;
                    chippy.i = ret;
                }
                    break;
                case 0x29:
                    chippy.i = &chippy.font[(chippy.reg[x] & 15) * 5] - chippy.memory;
                    break;
                case 0x33:
                    chippy.memory[chippy.i & 0xFFFF] = (chippy.reg[x] / 100) % 10;
                    chippy.memory[(chippy.i + 1) & 0xFFFF] = (chippy.reg[x] / 10) % 10;
                    chippy.memory[(chippy.i + 2) & 0xFFFF] = chippy.reg[x] % 10;
                    break;
                case 0x55:
                    for(int i = 0; i <= x; i++)
                        chippy.memory[chippy.i++ & 0xFFFF] = chippy.reg[i];
                    break;
                case 0x65:
                    for(int i = 0; i <= x; i++)
                        chippy.reg[i] = chippy.memory[chippy.i++ & 0xFFFF];
                    break;
            }
            break;
    }
}

int main(int argc, char** argv)
{
    load(argv[1]);

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(argv[1], SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DISPLAY_WIDTH * 8, DISPLAY_HEIGHT * 8, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* screen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH,DISPLAY_HEIGHT);

    int done = 0;
    int running = 1;
    unsigned instToDo = 0;

    u64 frequency = SDL_GetPerformanceFrequency();
    u64 last = SDL_GetPerformanceCounter();

    while(running)
    {
        for(unsigned i = 0; i < instToDo && !(chippy.waitingKey & 0x80); i++)
            step();

        for(SDL_Event e; SDL_PollEvent(&e);)
        {
            switch(e.type)
            {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                {
                    int key = get_key(e.key.keysym.scancode);

                    if(key == -1)
                        break;

                    int state = e.key.state;
                    chippy.keys[key] = state;

                    if(state == SDL_KEYDOWN && (chippy.waitingKey & 0x80))
                    {
                        chippy.waitingKey             &= 0x7F;
                        chippy.reg[chippy.waitingKey]  = key;
                    }
                }
                    break;
            }
        }

        u64 now = SDL_GetPerformanceCounter();
        f64 elapsed = (double) (now - last) / frequency;
        int frames = (int) (elapsed * 60) - done;

        if(frames > 0)
        {
            done += frames;

            chippy.delayTimer -= MIN(frames, chippy.delayTimer);

            if(chippy.soundTimer > 0)
            {
                chippy.soundTimer -= MIN(frames, chippy.soundTimer);

                if(chippy.soundTimer == 0)
                    puts("BEEP!");
            }

            u32 pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT];

            for(int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
                pixels[i] = 0xFFFFFF * ((chippy.display[i / 8] >> (7 - i % 8)) & 1);

            SDL_UpdateTexture(screen, NULL, pixels, 4 * DISPLAY_WIDTH);
            SDL_RenderCopy(renderer, screen, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        instToDo = MAX(frames, 1) * INSTRUCTIONS_PER_FRAME;

        if((chippy.waitingKey & 0x80) || !frames)
            SDL_Delay(1000 / 60);
    }

    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}
