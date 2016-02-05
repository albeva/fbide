#include "app_pch.h"

class App : public wxApp
{
public:

	bool OnInit() override
	{
		auto frame = new wxFrame(nullptr, wxID_ANY, "fbide");
		frame->Show();
		return true;
	}
};

DECLARE_APP(App);
IMPLEMENT_APP(App);