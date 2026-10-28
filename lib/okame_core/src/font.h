#pragma once
#include <stdint.h>

// 3x5 glyphs, 15 bits row-major from the top-left (bit 14 = top-left).
inline uint16_t glyph3x5(char c) {
  if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
  switch (c) {
    case '0': return 0b111101101101111;
    case '1': return 0b010110010010111;
    case '2': return 0b111001111100111;
    case '3': return 0b111001111001111;
    case '4': return 0b101101111001001;
    case '5': return 0b111100111001111;
    case '6': return 0b111100111101111;
    case '7': return 0b111001010010010;
    case '8': return 0b111101111101111;
    case '9': return 0b111101111001111;
    case 'A': return 0b010101111101101;
    case 'B': return 0b110101110101110;
    case 'C': return 0b011100100100011;
    case 'D': return 0b110101101101110;
    case 'E': return 0b111100110100111;
    case 'F': return 0b111100110100100;
    case 'G': return 0b011100101101011;
    case 'H': return 0b101101111101101;
    case 'I': return 0b111010010010111;
    case 'J': return 0b001001001101010;
    case 'K': return 0b101101110101101;
    case 'L': return 0b100100100100111;
    case 'M': return 0b101111111101101;
    case 'N': return 0b110101101101101;
    case 'O': return 0b010101101101010;
    case 'P': return 0b110101110100100;
    case 'Q': return 0b010101101110011;
    case 'R': return 0b110101110101101;
    case 'S': return 0b011100010001110;
    case 'T': return 0b111010010010010;
    case 'U': return 0b101101101101111;
    case 'V': return 0b101101101101010;
    case 'W': return 0b101101111111101;
    case 'X': return 0b101101010101101;
    case 'Y': return 0b101101010010010;
    case 'Z': return 0b111001010100111;
    case '-': return 0b000000111000000;
    case ':': return 0b000010000010000;
    case '.': return 0b000000000000010;
    case '!': return 0b010010010000010;
    case '?': return 0b111001011000010;
    case '/': return 0b001001010100100;
    case '+': return 0b000010111010000;
    case '%': return 0b101001010100101;
    default: return 0;
  }
}
