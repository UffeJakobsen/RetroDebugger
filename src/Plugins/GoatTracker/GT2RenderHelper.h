#ifndef _GT2RenderHelper_H_
#define _GT2RenderHelper_H_

#include "SYS_Defs.h"
#include "imgui.h"
#include "CGT2FontAtlas.h"

// Renoise-layout UI zoom factor. 1.0 = 100%. Range [0.5, 3.5], 12.5% grid.
// Single source of truth; written only through GT2_SetRenoiseUIScale
// (see C64DebuggerPluginGoatTracker.h). Persisted per workspace.
extern float gt2RenoiseUIScale;

// Render-time scale. Legacy GT2 layouts always render at 1.0; only the
// Renoise keyboard/layout mode uses gt2RenoiseUIScale.
float GT2EffectiveUIScale();

// On-screen GT2 character cell size, scaled by the zoom factor.
// GT2_CHAR_W/H stay as the unscaled font-atlas texture-space constants.
inline float GT2CellW() { return (float)GT2_CHAR_W * GT2EffectiveUIScale(); }
inline float GT2CellH() { return (float)GT2_CHAR_H * GT2EffectiveUIScale(); }

// Draw single character from GT2 font atlas at pixel position.
// charCode: 0-255 index into font atlas.
// fgColor/bgColor: ImU32 RGBA colors (use font->palette[index] to look up GT2 palette).
void DrawCharGT2(ImDrawList *dl, CGT2FontAtlas *font,
                 float px, float py, u8 charCode, ImU32 fgColor, ImU32 bgColor);

// Draw a string of characters (replaces printtext).
// colorIndex: packed GT2 color byte - low nibble = fg, high nibble = bg.
// Example: 0x07 = fg=7 (light gray), bg=0 (black).
void DrawTextGT2(ImDrawList *dl, CGT2FontAtlas *font,
                 float px, float py, u8 colorIndex, const char *text);

// Draw a colored background rectangle without changing fg/chars (replaces printbg).
// Used for cursor highlight and selection marks.
void DrawBgGT2(ImDrawList *dl, CGT2FontAtlas *font,
               float px, float py, u8 colorIndex, int lengthChars);

// Fill an area with space characters in the given background color (replaces printblankc).
void DrawBlankGT2(ImDrawList *dl, CGT2FontAtlas *font,
                  float px, float py, u8 colorIndex, int lengthChars);

// Draw a box outline using ASCII box-drawing characters (replaces drawbox).
void DrawBoxGT2(ImDrawList *dl, CGT2FontAtlas *font,
                float px, float py, u8 colorIndex, int widthChars, int heightChars);

// Coordinate conversion helpers - scaled by the zoom factor.
inline float GT2ColToPixel(int col) { return (float)col * GT2CellW(); }
inline float GT2RowToPixel(int row) { return (float)row * GT2CellH(); }
inline int GT2PixelToCol(float px)  { return (int)(px / GT2CellW()); }
inline int GT2PixelToRow(float py)  { return (int)(py / GT2CellH()); }

#endif // _GT2RenderHelper_H_
