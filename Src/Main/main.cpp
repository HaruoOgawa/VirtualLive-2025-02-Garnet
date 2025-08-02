#include <AppCore/CAppRunner.h>
#include "../App/ScriptApp/CScriptApp.h"

int main()
{
	app::SAppSettings Settings = {};
	Settings.FullScreen = false;
	Settings.ShowFPS = true;

	if (!app::CAppRunner::Run(std::make_shared<app::CScriptApp>(), Settings)) return 1;

	return 0;
}