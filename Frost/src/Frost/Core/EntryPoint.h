#pragma once

extern Frost::Application* Frost::CreateApplication();

int main(int argc, char** argv)
{
	Frost::Log::Init();
	//FROST_CORE_INFO("Created log!");

	auto app = Frost::CreateApplication();
	app->Run();
	delete app;

}
