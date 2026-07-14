#ifndef COLORS_H
#define COLORS_H

#define COLOR(fore, back) (((back) << 4) | (fore))

#define BLACK   0x0
#define BLUE    0x1
#define GREEN   0x2
#define CYAN    0x3
#define RED     0x4
#define MAGENTA 0x5
#define BROWN   0x6
#define GRAY    0x7
#define DARK_GRAY    0x8
#define LIGHT_BLUE   0x9
#define LIGHT_GREEN  0xA
#define LIGHT_CYAN   0xB
#define LIGHT_RED    0xC
#define LIGHT_MAGENTA 0xD
#define YELLOW   0xE
#define WHITE    0xF

#define C_PROMPT     COLOR(WHITE, BLACK)
#define C_PATH       COLOR(CYAN, BLACK)
#define C_COMMAND    COLOR(LIGHT_GREEN, BLACK)
#define C_OUTPUT     COLOR(GRAY, BLACK)
#define C_ERROR      COLOR(LIGHT_RED, BLACK)
#define C_SUCCESS    COLOR(GREEN, BLACK)
#define C_DIR        COLOR(LIGHT_CYAN, BLACK)
#define C_FILE       COLOR(WHITE, BLACK)
#define C_HEADER     COLOR(YELLOW, BLACK)

#endif
