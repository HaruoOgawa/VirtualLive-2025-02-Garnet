#include <AppCore/CAppRunner.h>
#include "../App/ScriptApp/CLive1135App.h"

int main()
{
	app::SAppSettings Settings = {};
#ifdef _DEBUG
	Settings.FullScreen = false;
	Settings.ShowFPS = true;
#endif // _DEBUG

	if (!app::CAppRunner::Run(std::make_shared<app::CLive1135App>(), Settings)) return 1;

	return 0;
}