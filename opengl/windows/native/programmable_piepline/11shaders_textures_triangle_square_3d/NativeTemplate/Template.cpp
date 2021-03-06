#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include <gl/glew.h> // must come before GL.h header
#include <gl/GL.h>

#include "vmath.h"
#include "resource.h"

#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "opengl32.lib")

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

using namespace vmath;

enum
{
	VDG_ATTRIBUTE_VERTEX = 0,
	VDG_ATTRIBUTE_COLOR = 1,
	VDG_ATTRIBUTE_NORMAL,
	VDG_ATTRIBUTE_TEXTURE0,
};

LRESULT CALLBACK WndCallbackProc(HWND, UINT, WPARAM, LPARAM);
void uninitialize(void);
void resize(int, int);
void display(void);

FILE *gpFile = NULL;

HWND gblHwnd = NULL;
HDC gblHdc = NULL;
HGLRC gblHrc = NULL;

DWORD gblDwStyle;
WINDOWPLACEMENT gblWindowPlcaement = { sizeof(WINDOWPLACEMENT) };

bool gblFullScreen = false;
bool gblIsEscPressed = false;
bool gblActiveWindow = false;

GLuint gVertexShaderObject;
GLuint gFragmentShaderObject;
GLuint gShaderProgramObject;

GLuint gVaoPyramid;
GLuint gVboPyramidPosition;
GLuint gVboPyramidTexture;

GLuint gVaoCube;
GLuint gVboCubePosition;
GLuint gVboCubeTexture;

GLfloat pyramidAngle = 0.0f;
GLfloat cubeAngle = 0.0f;

GLuint gMVPUniform;

mat4 gPerspectiveProjectionMatrix;

GLuint gTexture_sampler_uniform;
GLuint gTextureKundali;
GLuint gTextureStone;

int WINAPI WinMain(HINSTANCE currentHInstance, HINSTANCE prevHInstance, LPSTR lpszCmdLune, int iCmdShow)
{
	// function prototype
	void initialize(void);

	// variables declartion
	WNDCLASSEX wndClass;
	HWND hwnd;
	MSG msg;
	int iScreenWidth, iScreenHeight;
	TCHAR szClassName[] = TEXT("RTR OpenGL");
	bool bDone = false;

	if (fopen_s(&gpFile, "Log.txt", "w") != 0)
	{
		MessageBox(NULL, TEXT("Log file cant be created \n existing"), TEXT("ERROR"), MB_OK | MB_TOPMOST | MB_ICONSTOP);
		exit(0);
	}
	else
	{
		fprintf(gpFile, "Log file created succcessfully \n");
	}

	//initialize window object
	wndClass.cbSize = sizeof(WNDCLASSEX);
	/*
	CS_OWNDC : Is required to make sure memory allocated is neither movable or discardable
	in OpenGL.
	*/
	wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = currentHInstance;
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.hIcon = wndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.lpfnWndProc = WndCallbackProc;
	wndClass.lpszClassName = szClassName;
	wndClass.lpszMenuName = NULL;

	// register class
	RegisterClassEx(&wndClass);

	iScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	iScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	// create window
	hwnd = CreateWindowEx(WS_EX_APPWINDOW,
		szClassName,
		TEXT("OpenGL Programmable Pipeline : Texture 3D"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
		(iScreenWidth / 2) - (WIN_WIDTH / 2),
		(iScreenHeight / 2) - (WIN_HEIGHT / 2),
		WIN_WIDTH,
		WIN_HEIGHT,
		NULL,
		NULL,
		currentHInstance,
		NULL);
	gblHwnd = hwnd;

	// initialize
	initialize();

	// render window
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

	// game loop
	while (bDone == false)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				bDone = true;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			if (gblActiveWindow == true)
			{
				display();
				if (gblIsEscPressed == true)
					bDone = true;
			}
		}
	}

	uninitialize();

	return ((int)msg.wParam);
}

LRESULT CALLBACK WndCallbackProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	void display(void);
	void toggleFullScreen(void);

	switch (iMsg)
	{
	case WM_ACTIVATE:
		gblActiveWindow = (HIWORD(wParam) == 0);
		break;

	case WM_PAINT:
		/*
		it's single buffered rendering/painting
		single threaded
		can't save the state on stack
		so that's why tearing and flicekring happens
		*/
		break;

		//TO-DO: No need due double buffer
	case WM_ERASEBKGND:
		/*
		telling windows, dont paint window background, this program
		has ability to paint window background by itself.*/
		return(0);

	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
		case 'q':
		case 'Q':
			gblIsEscPressed = true;
			break;

		case 0x46: // 'f' or 'F'
			gblFullScreen = !gblFullScreen;
			toggleFullScreen();
			break;

		default:
			break;
		}
		break;

	case WM_SIZE:
		resize(LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_LBUTTONDOWN:
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		break;
	}

	return (DefWindowProc(hwnd, iMsg, wParam, lParam));
}

void initialize(void)
{
	void ReleaseDeviceContext(void);
	int LoadGLTexture(GLuint *, TCHAR[]);

	/*It has 26 members*/
	PIXELFORMATDESCRIPTOR pfd;
	int iPixelFormatIndex;

	// zero out 
	ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));

	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;

	// added double buffer now
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cRedBits = 8;
	pfd.cGreenBits = 8;
	pfd.cBlueBits = 8;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 32;

	gblHdc = GetDC(gblHwnd);

	iPixelFormatIndex = ChoosePixelFormat(gblHdc, &pfd);
	if (iPixelFormatIndex == 0)
	{
		ReleaseDeviceContext();
	}

	if (SetPixelFormat(gblHdc, iPixelFormatIndex, &pfd) == FALSE)
	{
		ReleaseDeviceContext();
	}

	/*
	1) every window has individual rendering context (In maya/phosohop every window has own context)
	if we have multiple child window and want to render same things accross all then
	main window knows to with it has shared the context but child/other window doesnt know about it
	2) usually 1 viewport has 1 rendering context. we can split 1 window in multiple viewports and can 1 rendering
	context into multiple viewports

	Transition from window to OpenGL rendering context
	Copies Windows context property to OpenGL context properties
	Windows: WGL (Windows GL)

	This is called Bridging
	*/
	gblHrc = wglCreateContext(gblHdc);
	if (gblHrc == NULL)
	{
		ReleaseDeviceContext();
	}

	if (wglMakeCurrent(gblHdc, gblHrc) == FALSE)
	{
		wglDeleteContext(gblHrc);
		gblHrc = NULL;
		ReleaseDeviceContext();
	}

	//GLEW initialization
	GLenum glew_error = glewInit();
	if (glew_error != GLEW_OK)
	{
		wglDeleteContext(gblHrc);
		gblHrc = NULL;
		ReleaseDeviceContext();
	}

	// VERTEX SHADER
	gVertexShaderObject = glCreateShader(GL_VERTEX_SHADER);
	const GLchar *vertexShaderSourceCode =
		"#version 130" \
		"\n" \
		"in vec4 vPosition;" \
		"in vec2 vTexture0_coords;" \
		"out vec2 out_texture0_coords;" \
		"uniform mat4 u_mvp_matrix;" \
		"void main(void)" \
		"{" \
		"gl_Position = u_mvp_matrix * vPosition;" \
		"out_texture0_coords = vTexture0_coords;" \
		"}";
	glShaderSource(gVertexShaderObject, 1, (const GLchar **)&vertexShaderSourceCode, NULL);
	glCompileShader(gVertexShaderObject);

	// Error checking
	GLint iInfoLogLength = 0;
	GLint iShaderCompiledStatus = 0;
	char *szInfoLog = NULL;
	glGetShaderiv(gVertexShaderObject, GL_COMPILE_STATUS, &iShaderCompiledStatus);
	if (iShaderCompiledStatus == GL_FALSE)
	{
		glGetShaderiv(gVertexShaderObject, GL_INFO_LOG_LENGTH, &iInfoLogLength);
		if (iInfoLogLength > 0)
		{
			szInfoLog = (char *)malloc(iInfoLogLength);
			if (szInfoLog != NULL)
			{
				GLsizei written;
				glGetShaderInfoLog(gVertexShaderObject, iInfoLogLength, &written, szInfoLog);
				fprintf(gpFile, "Vertex Shader Compilation Log : %s\n", szInfoLog);
				free(szInfoLog);
				uninitialize();
				//fclose(gpFile);  //if any error occurs write immidiately 
				exit(0);
			}
		}
	}

	// FRAGMENT SHADER
	gFragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar *fragmentShaderSourceCode =
		"#version 130" \
		"\n" \
		"in vec2 out_texture0_coords;" \
		"out vec4 FragColor;" \
		"uniform sampler2D u_texture0_sampler;" \
		"void main(void)" \
		"{" \
		"FragColor = texture(u_texture0_sampler, out_texture0_coords);" \
		"}";

	glShaderSource(gFragmentShaderObject, 1, (const GLchar **)&fragmentShaderSourceCode, NULL);
	glCompileShader(gFragmentShaderObject);

	// Error checking
	glGetShaderiv(gFragmentShaderObject, GL_COMPILE_STATUS, &iShaderCompiledStatus);
	if (iShaderCompiledStatus == GL_FALSE)
	{
		glGetShaderiv(gFragmentShaderObject, GL_INFO_LOG_LENGTH, &iInfoLogLength);
		if (iInfoLogLength > 0)
		{
			szInfoLog = (char *)malloc(iInfoLogLength);
			if (szInfoLog != NULL)
			{
				GLsizei written;
				glGetShaderInfoLog(gFragmentShaderObject, iInfoLogLength, &written, szInfoLog);
				fprintf(gpFile, "Fragment Shader Compilation Log : %s\n", szInfoLog);
				free(szInfoLog);
				uninitialize();
				//fclose(gpFile);  //if any error occurs write immidiately 
				exit(0);
			}
		}
	}

	// SHADER PROGRAM
	gShaderProgramObject = glCreateProgram();

	// attach vertex shader to shader program
	glAttachShader(gShaderProgramObject, gVertexShaderObject);

	// attach vertex shader to shader program
	glAttachShader(gShaderProgramObject, gFragmentShaderObject);

	// pre-link binding of shader program object with vertex shader position attribute
	glBindAttribLocation(gShaderProgramObject, VDG_ATTRIBUTE_VERTEX, "vPosition");

	// pre-link binding of shader program object with fragment shader texture attribute
	glBindAttribLocation(gShaderProgramObject, VDG_ATTRIBUTE_TEXTURE0, "vTexture0_coords");

	// link shader
	glLinkProgram(gShaderProgramObject);
	GLint iShaderProgramLinkStatus = 0;
	glGetProgramiv(gShaderProgramObject, GL_LINK_STATUS, &iShaderProgramLinkStatus);
	if (iShaderProgramLinkStatus == GL_FALSE)
	{
		glGetProgramiv(gShaderProgramObject, GL_INFO_LOG_LENGTH, &iInfoLogLength);
		if (iInfoLogLength > 0)
		{
			szInfoLog = (char *)malloc(iInfoLogLength);
			if (szInfoLog != NULL)
			{
				GLsizei written;
				glGetProgramInfoLog(gShaderProgramObject, iInfoLogLength, &written, szInfoLog);
				fprintf(gpFile, "Shader Program Link Log : %s\n", szInfoLog);
				free(szInfoLog);
				uninitialize();
				//fclose(gpFile);  //if any error occurs write immidiately
				exit(0);
			}
		}
	}

	// get MVP uniform location
	gMVPUniform = glGetUniformLocation(gShaderProgramObject, "u_mvp_matrix");
	gTexture_sampler_uniform = glGetUniformLocation(gShaderProgramObject, "u_texture0_sampler");

	// vertices, colors, shader attribs, vbo, vao initializations
	const GLfloat pyramidVertices[] =
	{
		0.0f, 1.0f, 0.0f, // front 
		-1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		0.0f, 1.0f, 0.0f, // right
		1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,
		0.0f, 1.0f, 0.0f, // back
		1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		0.0f, 1.0f, 0.0f, // left
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, 1.0f
	};
		
	const GLfloat pyramidTexturecoords[] =
	{
		0.5, 1.0,
		0.0, 0.0,
		1.0, 0.0,

		0.5, 1.0,
		1.0, 0.0,
		0.0, 0.0,

		0.5, 1.0,
		1.0, 0.0,
		0.0, 0.0,

		0.5, 1.0,
		0.0, 0.0,
		1.0, 0.0
	};

	// Pyramid VAO
	glGenVertexArrays(1, &gVaoPyramid);
	glBindVertexArray(gVaoPyramid);

	// Position
	glGenBuffers(1, &gVboPyramidPosition);
	glBindBuffer(GL_ARRAY_BUFFER, gVboPyramidPosition);
	
	// move data from main memory to graphics memory
	// GL_STATIC_DRAW or GL_DYNAMIC_DRAW : How you want load data run or preloading. example game shows loadingbar and "loading" messages
	glBufferData(GL_ARRAY_BUFFER, sizeof(pyramidVertices), pyramidVertices, GL_STATIC_DRAW);

	glVertexAttribPointer(VDG_ATTRIBUTE_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(VDG_ATTRIBUTE_VERTEX);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//texture
	glGenBuffers(1, &gVboPyramidTexture);
	glBindBuffer(GL_ARRAY_BUFFER, gVboPyramidTexture);
	// move data from main memory to graphics memory
	// GL_STATIC_DRAW or GL_DYNAMIC_DRAW : How you want load data run or preloading. example game shows loadingbar and "loading" messages
	glBufferData(GL_ARRAY_BUFFER, sizeof(pyramidTexturecoords), pyramidTexturecoords, GL_STATIC_DRAW);

	glVertexAttribPointer(VDG_ATTRIBUTE_TEXTURE0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(VDG_ATTRIBUTE_TEXTURE0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	// vertices, colors, shader attribs, vbo, vao initializations
	const GLfloat cubeVertices[] =
	{
		1.0f, 1.0f, -1.0f, // TOP
		-1.0f, 1.0f, -1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, -1.0f, // bootom 
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, // front
		-1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		1.0f, 1.0f, -1.0f, // back
		-1.0f, 1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, 1.0f, -1.0f, // right
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,
		-1.0f, 1.0f, -1.0f, // left
		-1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f
	};

	// pretty bullie way
	const GLfloat cubeTextureCords[] =
	{
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};	

	// Cube VAO
	glGenVertexArrays(1, &gVaoCube);
	glBindVertexArray(gVaoCube);

	// Position
	glGenBuffers(1, &gVboCubePosition);
	glBindBuffer(GL_ARRAY_BUFFER, gVboCubePosition);
	// move data from main memory to graphics memory
	// GL_STATIC_DRAW or GL_DYNAMIC_DRAW : How you want load data run or preloading. example game shows loadingbar and "loading" messages
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(VDG_ATTRIBUTE_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(VDG_ATTRIBUTE_VERTEX);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Texture
	glGenBuffers(1, &gVboCubeTexture);
	glBindBuffer(GL_ARRAY_BUFFER, gVboCubeTexture);
	// move data from main memory to graphics memory
	// GL_STATIC_DRAW or GL_DYNAMIC_DRAW : How you want load data run or preloading. example game shows loadingbar and "loading" messages
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeTextureCords), cubeTextureCords, GL_STATIC_DRAW);
	glVertexAttribPointer(VDG_ATTRIBUTE_TEXTURE0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(VDG_ATTRIBUTE_TEXTURE0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glShadeModel(GL_SMOOTH);

	glClearDepth(1.0f); // set depth buffer
	glEnable(GL_DEPTH_TEST); // enable depth testing
	glDepthFunc(GL_LEQUAL); // type of depth testing (may be Direxct3D, Vulkcan doesnt requires this test)

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	LoadGLTexture(&gTextureStone, MAKEINTRESOURCE(IDB_BITMAP_STONE));
	LoadGLTexture(&gTextureKundali, MAKEINTRESOURCE(IDB_BITMAP_KUNDALI));

	//glEnable(GL_CULL_FACE);

	/*state function*/
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // BLACK

	// Perspective Matrix to identity matrix
	gPerspectiveProjectionMatrix = mat4::identity();

	// warmup
	resize(WIN_WIDTH, WIN_HEIGHT);
}

int LoadGLTexture(GLuint *texture, TCHAR resource[])
{
	HBITMAP hBitmap;
	BITMAP bmp;
	int iStatus = FALSE;

	glGenTextures(1, texture);
	hBitmap = (HBITMAP) LoadImage(GetModuleHandle(NULL), resource, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	
	if (hBitmap) 
	{
		iStatus = TRUE;

		GetObject(hBitmap, sizeof(bmp), &bmp);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glBindTexture(GL_TEXTURE_2D, *texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bmp.bmWidth, bmp.bmHeight, 0, GL_BGR, GL_UNSIGNED_BYTE, bmp.bmBits);

		glGenerateMipmap(GL_TEXTURE_2D);

		DeleteObject(hBitmap);
	}

	return (iStatus);
}

void ReleaseDeviceContext(void)
{
	ReleaseDC(gblHwnd, gblHdc);
	gblHdc = NULL;
}

void uninitialize(void)
{
	if (gblFullScreen == false)
	{
		gblDwStyle = GetWindowLong(gblHwnd, GWL_STYLE);
		SetWindowLong(gblHwnd, GWL_STYLE, gblDwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(gblHwnd, &gblWindowPlcaement);
		SetWindowPos(gblHwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);

		ShowCursor(TRUE);
	}

	// destroy vao
	if (gVaoPyramid)
	{
		glDeleteVertexArrays(1, &gVaoPyramid);
		gVaoPyramid = 0;
	}
	if (gVaoCube)
	{
		glDeleteVertexArrays(1, &gVaoCube);
		gVaoCube = 0;
	}

	// destroy vbo
	if (gVboPyramidPosition)
	{
		glDeleteBuffers(1, &gVboPyramidPosition);
		gVboPyramidPosition = 0;
	}
	if (gVboPyramidTexture)
	{
		glDeleteBuffers(1, &gVboPyramidTexture);
		gVboPyramidTexture = 0;
	}
	if (gVboCubePosition)
	{
		glDeleteBuffers(1, &gVboCubePosition);
		gVboCubePosition = 0;
	}
	if (gVboCubeTexture)
	{
		glDeleteBuffers(1, &gVboCubeTexture);
		gVboCubeTexture = 0;
	}

	// detach shader from shader program
	glDetachShader(gShaderProgramObject, gVertexShaderObject);
	glDetachShader(gShaderProgramObject, gFragmentShaderObject);

	// delete vertex shader object
	glDeleteShader(gVertexShaderObject);
	gVertexShaderObject = 0;

	// delete fragment shader object
	glDeleteShader(gFragmentShaderObject);
	gFragmentShaderObject = 0;

	// delete shader program object
	glDeleteProgram(gShaderProgramObject);
	gShaderProgramObject = 0;

	// unlink shader program
	glUseProgram(0);

	// delete texture objects
	if (gTextureKundali) {
		glDeleteTextures(1, &gTextureKundali);
		gTextureKundali = 0;
	}

	if (gTextureStone) {
		glDeleteTextures(1, &gTextureStone);
		gTextureStone = 0;
	}

	wglMakeCurrent(NULL, NULL);

	wglDeleteContext(gblHrc);
	gblHrc = NULL;

	ReleaseDeviceContext();

	DestroyWindow(gblHwnd);
	gblHwnd = NULL;

	if (gpFile)
	{
		fprintf(gpFile, "Log file is succefully closed\n");
		fclose(gpFile);
		gpFile = NULL;
	}
}

void display(void)
{
	void update(void);

	//code
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// start using OpenGL program object
	glUseProgram(gShaderProgramObject);

	// OpenGL Drawing for Traingle
	// set modelview & modelviewprojection matrices to identity
	mat4 modelViewMatrix = mat4::identity();
	mat4 modelViewProjectionMatrix = mat4::identity();

	// tranlsate to negative x, z axis
	modelViewMatrix = modelViewMatrix * vmath::translate(-1.5f, 0.0f, -6.0f);
	// scale
	modelViewMatrix = modelViewMatrix * vmath::scale(0.5f, 0.5f, 0.5f);
	// rotate on y axis
	modelViewMatrix = modelViewMatrix * vmath::rotate(pyramidAngle, 0.0f, 1.0f, 0.0f);

	// multiply the modelview and orthographic matrix to get modelviewprojection matri
	// order is important
	modelViewProjectionMatrix = gPerspectiveProjectionMatrix * modelViewMatrix;

	// pass above modelviewprojection matrix to the vertex shader in 'u_mvp_matrix' shader variable
	// whose position value we already calculated in initWithFrame() by using glGetUniformLocation()
	glUniformMatrix4fv(gMVPUniform, 1, GL_FALSE, modelViewProjectionMatrix);

	// bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTextureStone);
	glUniform1i(gTexture_sampler_uniform, 0);

	// bind vao
	glBindVertexArray(gVaoPyramid);

	// draw, either by glDrawTriangles() or glDrawArrays() or glDrawElements()
	glDrawArrays(GL_TRIANGLES, 0, 12); // 3 (each with its x,y,z ) vertices in pyramidVertices array for 4 sides
									   // unbind vao
	glBindVertexArray(0);

	// OpenGL drwaing for square
	// set modelview & modelviewprojection matrices to identity
	modelViewMatrix = mat4::identity();
	modelViewProjectionMatrix = mat4::identity();

	// tranlsate to negative x, z axis
	modelViewMatrix = vmath::translate(1.5f, 0.0f, -6.0f);
	// scale
	modelViewMatrix = modelViewMatrix * vmath::scale(0.5f, 0.5f, 0.5f);
	// rotate on all axis
	modelViewMatrix = modelViewMatrix * vmath::rotate(cubeAngle, cubeAngle, cubeAngle);
	// multiply the modelview and orthographic matrix to get modelviewprojection matri
	// order is important
	modelViewProjectionMatrix = gPerspectiveProjectionMatrix * modelViewMatrix;

	// pass above modelviewprojection matrix to the vertex shader in 'u_mvp_matrix' shader variable
	// whose position value we already calculated in initWithFrame() by using glGetUniformLocation()
	glUniformMatrix4fv(gMVPUniform, 1, GL_FALSE, modelViewProjectionMatrix);

	// bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTextureKundali);
	glUniform1i(gTexture_sampler_uniform, 0);

	// bind vao
	glBindVertexArray(gVaoCube);

	// draw, either by glDrawTriangles() or glDrawArrays() or glDrawElements()
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // 3 (each with its x,y,z ) vertices in pyramidVertices array for 6 sides one by one
	glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
	glDrawArrays(GL_TRIANGLE_FAN, 8, 4);
	glDrawArrays(GL_TRIANGLE_FAN, 12, 4);
	glDrawArrays(GL_TRIANGLE_FAN, 16, 4);
	glDrawArrays(GL_TRIANGLE_FAN, 20, 4);
	// unbind vao
	glBindVertexArray(0);

	// stop using OpenGL program object
	glUseProgram(0);

	update();

	SwapBuffers(gblHdc);
}

void update()
{
	pyramidAngle = pyramidAngle >= 360 ? 0 : pyramidAngle + 1.0f;
	cubeAngle = cubeAngle > 360 ? 0 : cubeAngle + 1.0f;
}
/*
Very important for Dirext X not for OpenGL
becuase DirextX is not state machine and change in windows resize empose
re-rendering of Direct X (even for Vulcan) scenes.
*/
void resize(int width, int height)
{
	if (height == 0)
		height = 1;

	glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	gPerspectiveProjectionMatrix = vmath::perspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);

}

void toggleFullScreen(void)
{
	MONITORINFO monInfo;
	HMONITOR hMonitor;
	monInfo = { sizeof(MONITORINFO) };
	hMonitor = MonitorFromWindow(gblHwnd, MONITORINFOF_PRIMARY);

	if (gblFullScreen == true) {
		gblDwStyle = GetWindowLong(gblHwnd, GWL_STYLE);
		if (gblDwStyle & WS_OVERLAPPEDWINDOW)
		{
			gblWindowPlcaement = { sizeof(WINDOWPLACEMENT) };
			if (GetWindowPlacement(gblHwnd, &gblWindowPlcaement) && GetMonitorInfo(hMonitor, &monInfo))
			{
				SetWindowLong(gblHwnd, GWL_STYLE, gblDwStyle & ~WS_OVERLAPPEDWINDOW);
				SetWindowPos(gblHwnd, HWND_TOP, monInfo.rcMonitor.left, monInfo.rcMonitor.top, (monInfo.rcMonitor.right - monInfo.rcMonitor.left), (monInfo.rcMonitor.bottom - monInfo.rcMonitor.top), SWP_NOZORDER | SWP_FRAMECHANGED);
			}

			ShowCursor(FALSE);
		}
	}
	else
	{
		SetWindowLong(gblHwnd, GWL_STYLE, gblDwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(gblHwnd, &gblWindowPlcaement);
		SetWindowPos(gblHwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

		ShowCursor(TRUE);
	}
}