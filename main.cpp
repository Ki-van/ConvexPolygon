#include <Windows.h>
#include <gdiplus.h>
#include <tchar.h>
#include <resource.h>
#include <commctrl.h>
#include <windowsx.h>
#include <vector>

using namespace Gdiplus;

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

#define MAX_POLYGON_POINTS 20
#define MAX_CHECK_POINTS 10

// Global Variables:
HINSTANCE hInst;    // current instance

/// <summary>
/// Polygon points
/// </summary>
std::vector<Gdiplus::PointF> PP, 
							CheckedInPoints,
							CheckedOutPoints;

enum {
	CLOCKWISE,
	COUNTERCLOCKWISE
};

int BypassDirection;

BOOL DrawingPolygon;
Gdiplus::CachedBitmap *cbmSmile, *cbmSad;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
VOID				OnPaint(HDC hdc, HWND hwnd);
VOID                OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
BOOL                IsPolygonContains(PointF point);
BOOL				IsPolygonConvex();

// My bad math
double calcDet(PointF v1, PointF v2);
double Norm(PointF v);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	MyRegisterClass(hInstance);
	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

	DrawingPolygon = TRUE;

	ULONG_PTR gdiplusToken;
	GdiplusStartupInput gdiplusStartupInput;

	Gdiplus::Status stat = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	if (Ok == stat) {
		// Perform application initialization:
		if (!InitInstance(hInstance, nCmdShow))
		{
			return FALSE;
		}
		


		MSG msg;
		// Main message loop:
		while (GetMessage(&msg, NULL, 0, 0))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccel, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		GdiplusShutdown(gdiplusToken);
		return (int)msg.wParam;
	}
	else
		return stat;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = TEXT("tryNotToDIE");
	wcex.hIconSm = NULL;

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowEx(0, TEXT("tryNotToDIE"), TEXT("HAPPY"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		CW_USEDEFAULT, 0, 1000, 800, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	/*HDC hdc = GetDC(hWnd);
	Graphics g = Graphics(hdc);
	Bitmap bmSad = Bitmap(LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)));
	Gdiplus::CachedBitmap *bmSad = new Gdiplus::CachedBitmap(&bmSad, &g);
	*/

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDA_ENTER:
		{
			if (IsPolygonConvex())
				DrawingPolygon = FALSE;
			else
			{
				WCHAR mes[] = L"Многоугольник не выпуклый(";
				WCHAR caption[] = L"КАК ТАК!";
				MessageBox(hWnd, mes, caption, MB_ICONWARNING | MB_OK);
				PP.clear();
			}

			InvalidateRect(hWnd, NULL, TRUE);
		} break;
		case IDA_ESCAPE: {
			PP.clear();
			CheckedInPoints.clear();
			CheckedOutPoints.clear();

			DrawingPolygon = TRUE;
			InvalidateRect(hWnd, NULL, TRUE);
		} break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}break;
	case WM_LBUTTONDOWN: {
		HANDLE_WM_LBUTTONDOWN(hWnd, wParam, lParam, OnLButtonDown);
	} break;
	case WM_MOUSEMOVE: {
		if (DrawingPolygon && PP.size()>0) {
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			RECT rect = { (int)PP[PP.size() - 1].X - 20, PP[PP.size() - 1].Y - 20,xPos + 20, yPos + 20};

			InvalidateRect(hWnd, &rect, TRUE);
		}
	} break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		OnPaint(hdc, hWnd);
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

VOID OnPaint(HDC hdc, HWND hwnd) {
	Gdiplus::Graphics graphics(hdc);
	graphics.Clear(Color::White);

	Pen pen(Color::Black);
	pen.SetWidth(3);

	SolidBrush sBrush(Color::Green);

	for (int i = 1; i < PP.size(); i++)
		graphics.DrawLine(&pen, PP[i - 1], PP[i]);
	
	if (DrawingPolygon ) {
		if (PP.size() > 0) {
			POINT cursor;
			GetCursorPos(&cursor);
			ScreenToClient(hwnd, &cursor);
			PointF cursor2(cursor.x, cursor.y);

			graphics.DrawLine(&pen, PP[PP.size() - 1], cursor2);
		}
	}
	else {
		graphics.DrawLine(&pen, PP[PP.size() - 1], PP[0]);
		#define CIP CheckedInPoints
		for (int i = 0; i < CIP.size(); i++)
		{
			graphics.FillEllipse(&sBrush, (int) CIP[i].X - 5, (int) CIP[i].Y - 5, 10, 10);
		}
		sBrush.SetColor(Color::Red);

		#define COP CheckedOutPoints
		for (int i = 0; i < COP.size(); i++)
		{
			graphics.FillEllipse(&sBrush, (int)COP[i].X - 5, (int)COP[i].Y - 5, 10, 10);
		}
	}
}

VOID OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags) {
	if (DrawingPolygon) {
		PP.push_back(PointF(x, y));

		if (PP.size() == MAX_POLYGON_POINTS) {
			if(IsPolygonConvex())
				DrawingPolygon = FALSE;
			else
			{
				WCHAR mes[] = L"Многоугольник не выпуклый(";
				WCHAR caption[] = L"КАК ТАК!";
				MessageBox(hwnd, mes, caption, MB_ICONWARNING | MB_OK);
				PP.clear();
			}

			InvalidateRect(hwnd, NULL, TRUE);
		}
	}
	else
	{
		if (CheckedInPoints.size() < MAX_CHECK_POINTS || CheckedOutPoints.size() < MAX_CHECK_POINTS) {
			if (IsPolygonContains(PointF(x, y)))
				CheckedInPoints.push_back(PointF(x, y));
			else
				CheckedOutPoints.push_back(PointF(x, y));
			InvalidateRect(hwnd, NULL, TRUE);
		}
		else
		{
			WCHAR mes[] = L"Предел точек достинут(";
			WCHAR caption[] = L"ОСТАНОВИСЬ!";
			MessageBox(hwnd, mes, caption, MB_ICONWARNING | MB_OK);
		}
	}
}

BOOL IsPolygonContains(PointF point) {

	PointF v1, v2;
	double det;
	for (int i = 1; i < PP.size() + 1; ++i)
	{
		v1 = PP[i % PP.size()] - PP[i - 1];
		v2 = point - PP[i - 1];
		det = calcDet(v1, v2);
		if (det < 0)
			return FALSE;
	}

	return TRUE;
}


BOOL IsPolygonConvex() {

	if (PP.size() < 3)
		return FALSE;
	
	double wolk = calcDet(PP[1] - PP[0], PP[2] - PP[1]);
	double det;
	PointF v1, v2;
	for (int i = 2; i < PP.size(); ++i)
	{
		v1 = PP[i % PP.size()] - PP[i - 1];
		v2 = PP[(i + 1) % PP.size()] - PP[i];

		det = calcDet(v1, v2);
		if (wolk < 0 && det > 0)
			return FALSE;
		if (wolk > 0 && det < 0)
			return FALSE;
	}

	if(det > 0)
	
	return TRUE;
}

double calcDet(PointF v1, PointF v2) {
	return (double)v1.X * v2.Y - (double)v1.Y * v2.X;
}

double Norm(PointF v) {
	return sqrt((double) v.X * v.X + (double) v.Y * v.Y);
}