#include <slides.h>

#if CHOWDSP_SLIDES_POSIX
int main(int, char**)
{
  return run_slides();
}
#elif CHOWDSP_SLIDES_WINDOWS
#include <windows.h>
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
  return run_slides();
}
#endif
