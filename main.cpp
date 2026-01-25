#include "test/test_slides.h"

#if VISAGE_MAC || VISAGE_LINUX || VISAGE_EMSCRIPTEN
int main(int, char**)
{
  return run_slides();
}
#else
#include <windows.h>
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
  return run_slides();
}
#endif
