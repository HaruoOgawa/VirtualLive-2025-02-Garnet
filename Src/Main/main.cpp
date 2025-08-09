#include <AppCore/CAppRunner.h>
#include "../App/ScriptApp/CLive1135App.h"

int main()
{
	app::SAppSettings Settings = {};
	Settings.FullScreen = false;
	Settings.ShowFPS = true;

	if (!app::CAppRunner::Run(std::make_shared<app::CLive1135App>(), Settings)) return 1;

	return 0;
}