#include "hello_triangle.h"

int main()
{
	HelloTriangle app;
	//app.CreateInstance();

	std::cout << "Revving up the N-Gin.." << std::endl;
	
	try
	{
		app.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}