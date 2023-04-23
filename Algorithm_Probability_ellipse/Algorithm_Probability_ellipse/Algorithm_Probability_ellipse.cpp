#include <iostream>
#include <windows.h>
#include <ctime> 
#include <list>
#include <cmath>

using namespace std;

void New_Setka(double setka_to_array[][30], int size) {

	for (int i = 0; i < 30; i++)
	{
		for (int j = 0; j < 30; j++)
		{
			if (setka_to_array[i][j] < 1)
			{
				srand(time(NULL));
				if ((1 + rand() % 100) < setka_to_array[i][j] * 100)
				{
					HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
					COORD position = { 3 * j, i };
					SetConsoleCursorPosition(hConsole, position);

					if (setka_to_array[i - 1][j] >= 1) cout << setka_to_array[i - 1][j];
					else if (setka_to_array[i + 1][j] >= 1) cout << setka_to_array[i + 1][j];
					else if (setka_to_array[i][j + 1] >= 1) cout << setka_to_array[i][j + 1];
					else if (setka_to_array[i][j - 1] >= 1) cout << setka_to_array[i][j - 1];
					else if (setka_to_array[i - 1][j - 1] >= 1) cout << setka_to_array[i - 1][j - 1];
					else if (setka_to_array[i - 1][j + 1] >= 1) cout << setka_to_array[i - 1][j + 1];
					else if (setka_to_array[i + 1][j - 1] >= 1) cout << setka_to_array[i + 1][j - 1];
					else if (setka_to_array[i + 1][j + 1] >= 1) cout << setka_to_array[i + 1][j + 1];
				}
			}
		}

	}

	Sleep(1000);
}

void Probability() {

	double setka_to_array[30][30];

	for (int i = 0; i < 30; i++)
	{
		for (int j = 0; j < 30; j++)
		{
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			COORD position = { 3 * j, i };
			SetConsoleCursorPosition(hConsole, position);

			WCHAR ch;
			DWORD read;
			ReadConsoleOutputCharacterW(hConsole, &ch, 1, position, &read);

			if (ch != '_')
			{
				setka_to_array[i][j] = ch - '0';
			}
			else
			{
				setka_to_array[i][j] = 0;
			}
		}
	}

	for (int i = 0; i < 30; i++)
	{
		for (int j = 0; j < 30; j++)
		{
			if (setka_to_array[i][j] >= 1)
			{
				if (i + 1 <= 29 && setka_to_array[i + 1][j] == 0) setka_to_array[i + 1][j] = 0.501;
				if (i - 1 >= 0 && setka_to_array[i - 1][j] == 0) setka_to_array[i - 1][j] = 0.501;
				if (j + 1 <= 29 && setka_to_array[i][j + 1] == 0) setka_to_array[i][j + 1] = 0.501;
				if (j - 1 >= 0 && setka_to_array[i][j - 1] == 0) setka_to_array[i][j - 1] = 0.501;
				if (i + 1 <= 29 && j - 1 >= 0 && setka_to_array[i + 1][j - 1] == 0) setka_to_array[i + 1][j - 1] = 0.448;
				if (i + 1 <= 29 && j + 1 <= 29 && setka_to_array[i + 1][j + 1] == 0) setka_to_array[i + 1][j + 1] = 0.016;
				if (i - 1 >= 0 && j - 1 >= 0 && setka_to_array[i - 1][j - 1] == 0) setka_to_array[i - 1][j - 1] = 0.016;
				if (i - 1 >= 0 && j + 1 <= 29 && setka_to_array[i - 1][j + 1] == 0) setka_to_array[i - 1][j + 1] = 0.448;
			}
		}
	}

	New_Setka(setka_to_array, 30);
}




void Print(int x, int y, int sight) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD position = { 3 * x - 3, y - 1 };
	SetConsoleCursorPosition(hConsole, position);
	cout << sight;
}

void Random() {
	srand(time(NULL));

	int x, y, sight = 0; list<int> myList = list<int>();
	for (int i = 0; i < (1 + rand() % 10); i++)
	{
		x = 1 + rand() % 30;
		y = 1 + rand() % 30;

		Print(x, y, ++sight);
		myList.push_back(x); myList.push_back(y);
	}

	Probability();
}

void Setka() {
	for (int i = 0; i < 30; i++)
	{
		for (int j = 0; j < 30; j++)
		{
			cout << "__" << "|";
		}
		cout << endl;
	}

	Random();
}

int main()
{
	Setka();

	for (int i = 0; i < 30; i++)
	{
		Probability();
	}

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD position = { 0, 45 };
	SetConsoleCursorPosition(hConsole, position);
}