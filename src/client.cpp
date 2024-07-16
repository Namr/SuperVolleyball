#include <iostream>
#include <raylib.h>

int main() {
  InitWindow(800, 450, "SuperVolleyball");

  while (!WindowShouldClose()) {
    BeginDrawing();
      ClearBackground(RAYWHITE);
      DrawText("there should be a game here.... eventually", 190, 200, 20, LIGHTGRAY);
    EndDrawing();
  }

  CloseWindow();

  return 0;
}
