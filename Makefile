minecracker.exe : minecracker.cpp minehook.dll
	g++ -o minecracker.exe -masm=intel -L. .\minecracker.cpp -lminehook

minehook.dll :
	g++ -o minehook.dll -shared -fPIC -masm=intel .\minehook.cpp -lgdi32

clean :
	del .\minehook.o minehook.dll minecracker.exe