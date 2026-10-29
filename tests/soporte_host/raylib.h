/* Sustituto exclusivo de pruebas: el host de produccion usa Raylib real. */
#ifndef LATINOUI_RAYLIB_SIMULADO_H
#define LATINOUI_RAYLIB_SIMULADO_H
#include <stdbool.h>
typedef struct Vector2 { float x, y; } Vector2;
typedef struct Rectangle { float x, y, width, height; } Rectangle;
typedef struct Color { unsigned char r, g, b, a; } Color;
#define FLAG_WINDOW_RESIZABLE 4u
#define MOUSE_BUTTON_LEFT 0
#define KEY_NULL 0
#define KEY_ESCAPE 256
#define KEY_ENTER 257
#define KEY_TAB 258
#define KEY_BACKSPACE 259
#define KEY_LEFT_SHIFT 340
#define KEY_RIGHT_SHIFT 344
void SetExitKey(int key);
bool IsKeyPressed(int key);
bool IsKeyDown(int key);
int GetCharPressed(void);
#define RAYWHITE ((Color){245, 245, 245, 255})
#define DARKBLUE ((Color){0, 82, 172, 255})
#define MAROON ((Color){190, 33, 55, 255})
#define SKYBLUE ((Color){102, 191, 255, 255})
#define GRAY ((Color){130, 130, 130, 255})
#define LIGHTGRAY ((Color){200, 200, 200, 255})
#define DARKGRAY ((Color){80, 80, 80, 255})
void SetConfigFlags(unsigned int flags);
void InitWindow(int width, int height, const char *title);
bool IsWindowReady(void);
void CloseWindow(void);
void SetWindowMinSize(int width, int height);
void SetTargetFPS(int fps);
bool WindowShouldClose(void);
int GetScreenWidth(void);
int GetScreenHeight(void);
bool IsMouseButtonPressed(int button);
Vector2 GetMousePosition(void);
bool CheckCollisionPointRec(Vector2 point, Rectangle rec);
void BeginDrawing(void);
void EndDrawing(void);
void ClearBackground(Color color);
void DrawText(const char *text, int x, int y, int fontSize, Color color);
void DrawRectangleRec(Rectangle rec, Color color);
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color);
void BeginScissorMode(int x, int y, int width, int height);
void EndScissorMode(void);
const char *TextFormat(const char *text, ...);
#endif
