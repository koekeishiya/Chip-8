#include "chip8.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#define internal static
#define DISPLAY_LENGTH (DISPLAY_WIDTH * DISPLAY_HEIGHT)

static unsigned char Chip8Font[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void Chip8Initialize(chip8 *Processor)
{
    /* NOTE(koekeishiya): Clear entire processor struct to zero. */
    memset(Processor, 0, sizeof(chip8));

    /* NOTE(koekeishiya): Reset program counter to starting location and load font-data into memory. */
    Processor->Pc = 0x200;
    memcpy(Processor->Memory, Chip8Font, sizeof(Chip8Font));

    srand(time(NULL));
}

bool Chip8LoadRom(chip8 *Processor, const char *Rom)
{
    FILE *FileHandle = fopen(Rom, "rb");
    unsigned int Length = 0;

    if(FileHandle)
    {
        fseek(FileHandle, 0, SEEK_END);
        Length = ftell(FileHandle);
        fseek(FileHandle, 0, SEEK_SET);
        fread(Processor->Memory + 0x200, Length, 1, FileHandle);
        fclose(FileHandle);
        return true;
    }
    else
    {
        return false;
    }
}

void Chip8DoCycle(chip8 *Processor)
{
    Processor->Opcode = Processor->Memory[Processor->Pc] << 8 |
                        Processor->Memory[Processor->Pc + 1];

    Processor->Pc += 2;
    unsigned short X = (Processor->Opcode & 0x0F00) >> 8;
    unsigned short Y = (Processor->Opcode & 0x00F0) >> 4;

    /* NOTE(koekeishiya): The first 4-bits decide the operation. */
    switch(Processor->Opcode & 0xF000)
    {
        case 0x0000: // Behaviour depends on least-significant-bits.
        {
            switch(Processor->Opcode)
            {
                case 0x00E0: // 00E0: Clears the screen.
                {
                    memset(Processor->Graphics, 0, DISPLAY_LENGTH);
                    Processor->Draw = true;
                } break;
                case 0x00EE: // 00EE: Returns from subroutine.
                {
                    Processor->Pc = Processor->Stack[--Processor->Sp];
                } break;
                default: // 0NNN: Calls RCA 1802 program at address NNN. Not necessary for most ROMs.
                {
                } break;
            }
        } break;
        case 0x1000: // 1NNN: Jumps to address NNN.
        {
            Processor->Pc = Processor->Opcode & 0x0FFF;
        } break;
        case 0x2000: // 2NNN: Calls subroutine at address NNN.
        {
            Processor->Stack[Processor->Sp++] = Processor->Pc;
            Processor->Pc = Processor->Opcode & 0x0FFF;
        } break;
        case 0x3000: // 3XNN: Skips the next instruction if VX equals NN.
        {
            if(Processor->V[X] == (Processor->Opcode & 0x00FF))
                Processor->Pc += 2;
        } break;
        case 0x4000: // 4XNN: Skips the next instruction if VX does not equal NN.
        {
            if(Processor->V[X] != (Processor->Opcode & 0x00FF))
                Processor->Pc += 2;
        } break;
        case 0x5000: // 5XY0: Skips the next instruction if VX equals VY.
        {
            if(Processor->V[X] == Processor->V[Y])
                Processor->Pc += 2;
        } break;
        case 0x6000: // 6XNN: Sets VX to NN.
        {
            Processor->V[X] = Processor->Opcode & 0x00FF;
        } break;
        case 0x7000: // 7XNN: Adds NN to VX.
        {
            Processor->V[X] += Processor->Opcode & 0x00FF;
        } break;
        case 0x8000:
        {
            switch(Processor->Opcode & 0x000F)
            {
                case 0x0000: // 8XY0: Sets VX to the value of VY.
                {
                    Processor->V[X] = Processor->V[Y];
                } break;
                case 0x0001: // 8XY1: Sets VX to VX or VY.
                {
                    Processor->V[X] |= Processor->V[Y];
                } break;
                case 0x0002: // 8XY2: Sets VX to VX and VY.
                {
                    Processor->V[X] &= Processor->V[Y];
                } break;
                case 0x0003: // 8XY3: Sets VX to VX xor VY.
                {
                    Processor->V[X] ^= Processor->V[Y];
                } break;
                case 0x0004: // 8XY4: Adds VY to VX. VF is set to 1 when there is a carry.
                {
                    Processor->V[0xF] = 0;
                    unsigned short Sum = Processor->V[X] + Processor->V[Y];

                    if(Sum > 255)
                    {
                        Processor->V[0xF] = 1;
                        Sum -= 256;
                    }

                    Processor->V[X] = Sum;
                } break;
                case 0x0005: // 8XY5: VY is subtracted from VX. VF is set to 0 when borrow.
                {
                    Processor->V[0xF] = 1;
                    short Diff = Processor->V[X] - Processor->V[Y];

                    if(Diff < 0)
                    {
                        Processor->V[0xF] = 0;
                        Diff += 256;
                    }

                    Processor->V[X] = Diff;
                } break;
                case 0x0006: // 8XY6: Shifts VX right by one. VF is set to value of least sig bit before shift.
                {
                    Processor->V[0xF] = Processor->V[X] & 0x1;
                    Processor->V[X] >>= 1;
                } break;
                case 0x0007: // 8XY7: Sets VX to VY minus VX. VF is set to 0 when borrow.
                {
                    Processor->V[0xF] = 1;
                    short Diff = Processor->V[Y] - Processor->V[X];

                    if(Diff < 0)
                    {
                        Processor->V[0xF] = 0;
                        Diff += 256;
                    }

                    Processor->V[X] = Diff;
                } break;
                case 0x000E: // 8XYE: Shifts VX left by one. VF is set to value of most sig bit before shift.
                {
                    Processor->V[0xF] = Processor->V[X] & 0x80;
                    Processor->V[X] <<= 1;
                } break;
            }
        } break;
        case 0x9000: // 5XY0: Skips the next instruction if VX does not equal VY.
        {
            if(Processor->V[X] != Processor->V[Y])
                Processor->Pc += 2;
        } break;
        case 0xA000: // ANNN: Sets I to the address NNN.
        {
            Processor->I = Processor->Opcode & 0x0FFF;
        } break;
        case 0xB000: // BNNN: Jump to address NNN plus V0.
        {
            Processor->Pc = (Processor->Opcode & 0x0FFF) + Processor->V[0];
        } break;
        case 0xC000: // CXNN: Sets VX to the result of bitwise and on a random number and NN.
        {
            Processor->V[X] = floor((rand() % 0xFF) & (Processor->Opcode & 0x00FF));
        } break;
        case 0xD000: // DXYN: Display sprite starting at memory location I, set VF equal to collision.
        {
            Processor->V[0xF] = 0;
            unsigned short RegisterX = Processor->V[X];
            unsigned short RegisterY = Processor->V[Y];
            unsigned short Height = Processor->Opcode & 0x000F;

            for(int Col = 0; Col < Height; ++Col)
            {
                unsigned short Pixel = Processor->Memory[Processor->I + Col];
                for(int Row = 0; Row < 8; ++Row)
                {
                    if((Pixel & (0x80 >> Row)) != 0)
                    {
                        unsigned short Location = RegisterX + Row + ((RegisterY + Col) * DISPLAY_WIDTH);
                        if(Processor->Graphics[Location] == 1)
                            Processor->V[0xF] = 1;

                        Processor->Graphics[Location] ^= 1;
                    }
                }
            }

            Processor->Draw = true;
        } break;
        case 0xE000:
        {
            switch(Processor->Opcode & 0x00FF)
            {
                case 0x009E: // EX9E: Skips the next instruction if the key stored in VX is pressed.
                {
                    if(Processor->Key[Processor->V[X]] == 1)
                        Processor->Pc += 2;
                } break;
                case 0x00A1: // EXA1: Skips the next instruction if the key stored in VX is not pressed.
                {
                    if(Processor->Key[Processor->V[X]] != 1)
                        Processor->Pc += 2;
                } break;
            }
        } break;
        case 0xF000:
        {
            switch(Processor->Opcode & 0x00FF)
            {
                case 0x0007: // FX07: Sets VX to the value of the delay timer.
                {
                    Processor->V[X] = Processor->DelayTimer;
                } break;
                case 0x000A: // FX0A: A keypress is awaited and stored in VX.
                {
                    bool Keypress = false;
                    for(int Index = 0; Index < 16; ++Index)
                    {
                        if(Processor->Key[Index] == 1)
                        {
                            Processor->V[X] = Index;
                            Keypress = true;
                        }
                    }

                    if(!Keypress)
                    {
                        Processor->Pc -= 2;
                        return;
                    }
                } break;
                case 0x0015: // FX18: Sets the delay timer to VX.
                {
                    Processor->DelayTimer = Processor->V[X];
                } break;
                case 0x0018: // FX18: Sets the sound timer to VX.
                {
                    Processor->SoundTimer = Processor->V[X];
                } break;
                case 0x001E: // FX1E: Adds VX to I.
                {
                    Processor->I += Processor->V[X];
                } break;
                case 0x0029: // FX29: Sets I to the location of the sprite in character VX (4x5 font).
                {
                    Processor->I = Processor->V[X] * 5;
                } break;
                case 0x0033: // FX33: Stores BCD representation of VX, with 3 digits, in memory at location I.
                {
                    unsigned char Digit = Processor->V[X];
                    for(int Index = 3; Index > 0; --Index)
                    {
                        Processor->Memory[Processor->I + Index - 1] = Digit % 10;
                        Digit /= 10;
                    }
                } break;
                case 0x0055: // FX55: Store registers V0 through VX in memory at location I.
                {
                    for(int Index = 0; Index <= X; ++Index)
                        Processor->Memory[Processor->I + Index] = Processor->V[Index];
                } break;
                case 0x0065: // FX65: Read registers V0 through VX from memory at location I.
                {
                    for(int Index = 0; Index <= X; ++Index)
                        Processor->V[Index] = Processor->Memory[Processor->I + Index];
                } break;
            }
        } break;
        default:
        {
            printf("Not yet implemented: 0x%X\n", Processor->Opcode);
        } break;
    }
}
