#include "application.h"

int main(void)
{
    auto &app = Application::Get();
    app.Run();
}
