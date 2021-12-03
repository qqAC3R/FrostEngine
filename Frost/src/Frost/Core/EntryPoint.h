#pragma once

extern Frost::Application* Frost::CreateApplication();

int main(int argc, char** argv)
{
	Frost::Log::Init();

	auto app = Frost::CreateApplication();
	app->Run();
	delete app;
}