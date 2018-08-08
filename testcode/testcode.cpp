// testcode.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <string>

using namespace std;

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	string name_path;
	string name_file, real_name;



	name_file = "test.zip";

	name_path = "c:\\pkgapdn";
	CreateDirectory(name_path.c_str(), NULL);

	name_path += "\\";
	name_path += name_file;
	
	int idx = name_path.find_last_of("\\");
	if(idx > 0)	
	{
		real_name = name_path.substr(0, idx);
		real_name += "\\";
		real_name += "setup.exe";
	}

	return 0;
}



