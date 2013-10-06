/*******************************************************************************
 *                                                                              *
 *   PrimeSense NiTE 2.0 - Hand Viewer Sample                                   *
 *   Copyright (C) 2012 PrimeSense Ltd.                                         *
 *                                                                              *
 *******************************************************************************/

#include <map>
#include "Viewer.h"

#if (ONI_PLATFORM == ONI_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "HistoryBuffer.h"
#include <NiteSampleUtilities.h>

#include "DetectFingers.h"
#include <vector>
#include <sys/wait.h>

#define GL_WIN_SIZE_X	1280
#define GL_WIN_SIZE_Y	1024
#define TEXTURE_SIZE	512

#define FINGER_HIST_SIZE 5
#define VOL_DIST_THRESH 20

#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

SampleViewer* SampleViewer::ms_self = NULL;

std::map<int, HistoryBuffer<20> *> g_histories;

bool g_drawDepth = true;
bool g_smoothing = false;
bool g_drawFrameId = false;

int g_nXRes = 0, g_nYRes = 0;

void SampleViewer::glutIdle()
{
    glutPostRedisplay();
}
void SampleViewer::glutDisplay()
{
    SampleViewer::ms_self->Display();
}
void SampleViewer::glutKeyboard(unsigned char key, int x, int y)
{
    SampleViewer::ms_self->OnKey(key, x, y);
}

SampleViewer::SampleViewer(const char* strSampleName)
{
    ms_self = this;
    strncpy(m_strSampleName, strSampleName, ONI_MAX_STR);
    m_pHandTracker = new nite::HandTracker;
}
SampleViewer::~SampleViewer()
{
    Finalize();

    delete[] m_pTexMap;

    ms_self = NULL;
}

void SampleViewer::Finalize()
{
    delete m_pHandTracker;
    nite::NiTE::shutdown();
    openni::OpenNI::shutdown();
}

openni::Status SampleViewer::Init(int argc, char **argv)
{
    m_pTexMap = NULL;

    openni::OpenNI::initialize();

    const char* deviceUri = openni::ANY_DEVICE;
    for (int i = 1; i < argc-1; ++i)
	{
	    if (strcmp(argv[i], "-device") == 0)
		{
		    deviceUri = argv[i+1];
		    break;
		}
	}

    openni::Status rc = m_device.open(deviceUri);
    if (rc != openni::STATUS_OK)
	{
	    printf("Open Device failed:\n%s\n", openni::OpenNI::getExtendedError());
	    return rc;
	}

    nite::NiTE::initialize();

    if (m_pHandTracker->create(&m_device) != nite::STATUS_OK)
	{
	    return openni::STATUS_ERROR;
	}
    m_pHandTracker->startGestureDetection(nite::GESTURE_HAND_RAISE);
    //m_pHandTracker->startGestureDetection(nite::GESTURE_WAVE);
    //m_pHandTracker->startGestureDetection(nite::GESTURE_CLICK);

    return InitOpenGL(argc, argv);
}

openni::Status SampleViewer::Run()	//Does not return
{
    glutMainLoop();

    return openni::STATUS_OK;
}

float Colors[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
int colorCount = 3;

void DrawHistory(nite::HandTracker* pHandTracker, int id, HistoryBuffer<20>* pHistory)
{
    glColor3f(Colors[id % colorCount][0], Colors[id % colorCount][1], Colors[id % colorCount][2]);
    float coordinates[60] = {0};
    float factorX = GL_WIN_SIZE_X / (float)g_nXRes;
    float factorY = GL_WIN_SIZE_Y / (float)g_nYRes;

    for (int i = 0; i < pHistory->GetSize(); ++i)
	{
	    const nite::Point3f& position = pHistory->operator[](i);
		
	    pHandTracker->convertHandCoordinatesToDepth(position.x, position.y, position.z, &coordinates[i*3], &coordinates[i*3+1]);

	    coordinates[i*3]   *= factorX;
	    coordinates[i*3+1] *= factorY;
	}

    glPointSize(8);
    glVertexPointer(3, GL_FLOAT, 0, coordinates);
    glDrawArrays(GL_LINE_STRIP, 0, pHistory->GetSize());


    glPointSize(12);
    glVertexPointer(3, GL_FLOAT, 0, coordinates);
    glDrawArrays(GL_POINTS, 0, 1);

}

#ifndef USE_GLES
void glPrintString(void *font, const char *str)
{
    int i,l = (int)strlen(str);

    for(i=0; i<l; i++)
	{   
	    glutBitmapCharacter(font,*str++);
	}   
}
#endif
void DrawFrameId(int frameId)
{
    char buffer[80] = "";
    sprintf(buffer, "%d", frameId);
    glColor3f(1.0f, 0.0f, 0.0f);
    glRasterPos2i(20, 20);
    glPrintString(GLUT_BITMAP_HELVETICA_18, buffer);
}


void SampleViewer::Display()
{
    nite::HandTrackerFrameRef handFrame;
    openni::VideoFrameRef depthFrame;
    nite::Status rc = m_pHandTracker->readFrame(&handFrame);
    if (rc != nite::STATUS_OK) {
	printf("GetNextData failed\n");
	return;
    }

    depthFrame = handFrame.getDepthFrame();

    if (m_pTexMap == NULL) {
	// Texture map init
	m_nTexMapX = MIN_CHUNKS_SIZE(depthFrame.getVideoMode().getResolutionX(), TEXTURE_SIZE);
	m_nTexMapY = MIN_CHUNKS_SIZE(depthFrame.getVideoMode().getResolutionY(), TEXTURE_SIZE);
	m_pTexMap = new openni::RGB888Pixel[m_nTexMapX * m_nTexMapY];
    }


    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y, 0, -1.0, 1.0);

    if (depthFrame.isValid()) {
	calculateHistogram(m_pDepthHist, MAX_DEPTH, depthFrame);
    }
    int fingerNum = 0;
    float fx=-1, fy=-1;
    memset(m_pTexMap, 0, m_nTexMapX*m_nTexMapY*sizeof(openni::RGB888Pixel));
    float factor[3] = {1, 1, 1};
    // check if we need to draw depth frame to texture
    if (depthFrame.isValid() && g_drawDepth) {
	const nite::Array<nite::HandData>& hands= handFrame.getHands();
	for (int i = 0; i < hands.getSize(); ++i) {
	    const nite::HandData& user = hands[i];
	    if (user.isTracking())
		if (i==0) {
		    m_pHandTracker->convertHandCoordinatesToDepth(user.getPosition().x, user.getPosition().y, user.getPosition().z, &fx, &fy);
		    break;
		}
	}

	const openni::DepthPixel* pDepthRow = (const openni::DepthPixel*)depthFrame.getData();
	openni::RGB888Pixel* pTexRow = m_pTexMap + depthFrame.getCropOriginY() * m_nTexMapX;
	int rowSize = depthFrame.getStrideInBytes() / sizeof(openni::DepthPixel);
	for (int y = 0; y < depthFrame.getHeight(); ++y) {
	    const openni::DepthPixel* pDepth = pDepthRow;
	    openni::RGB888Pixel* pTex = pTexRow + depthFrame.getCropOriginX();
	    
	    for (int x = 0; x < depthFrame.getWidth(); ++x, ++pDepth, ++pTex) {
		if (*pDepth != 0) {
		    factor[0] = Colors[colorCount][0];
		    factor[1] = Colors[colorCount][1];
		    factor[2] = Colors[colorCount][2];

		    int nHistValue = m_pDepthHist[*pDepth];
		    pTex->r = nHistValue*factor[0];
		    pTex->g = nHistValue*factor[1];
		    pTex->b = nHistValue*factor[2];
		    factor[0] = factor[1] = factor[2] = 1;
		}
	    }

	    pDepthRow += rowSize;
	    pTexRow += m_nTexMapX;
	}

	nite::Point3f hand(fx, fy, 0);
	DetectFingers df;
	//vector<nite::Point3f> fingers = df.fingerDebug(depthFrame, m_pDepthHist, m_pTexMap, m_nTexMapX, m_nTexMapY, hand);
	std::vector<nite::Point3f> fingers = df.fingersDebug(depthFrame, m_pDepthHist, m_pTexMap, m_nTexMapX, m_nTexMapY, hand);
	fingerNum = (int)fingers.size();
	while (fingersHistory.size()>FINGER_HIST_SIZE)
	    fingersHistory.erase(fingersHistory.begin());
	fingersHistory.push_back(fingers);
	if (fingers.size()>1){
	    //nite::Point3f p = fingers[0];
	    //printf("handy:%lf retx:%lf rety:%lf\n", fy, p.x, p.y);
	    char d[2];
	    d[1] = '\0';
	    if(fingerNum==2)
		d[0] = 'd';
	    else if(fingerNum==3)
		d[0] = 'u';
	    else
		goto nomatch;
	    
	    pid_t pid = vfork();
	    char *homev = getenv("HOME");
	    char *pathv = getenv("PATH");
	    char home[60];
	    char path[256];
	    sprintf(home, "HOME=%s", homev);
	    sprintf(path, "PATH=%s", pathv);
	    if (pid < 0) {
		perror("fork");
		exit(-1);
	    } else if (pid == 0) {
		//child
		char *args[] = {(char*)"./vol", (char*)d, (char*)NULL};
		//char *envs[] = {(char*)"PATH=/bin:/usr/bin:/usr/local/bin", home, (char*)NULL};
		char *envs[] = {(char*)path, (char*)home, (char*)NULL};
		execve("./vol", args, envs);
		exit(-1);
	    }
	    //parent
	    //printf("parent process start\n");

	    int status;
	    pid_t r = waitpid(pid, &status, 0);
	    if (r < 0) {
		perror("waitpid");
		exit(-1);
	    }
	    if (WIFEXITED(status)) {
		int exit_code = WEXITSTATUS(status);
		//printf("child exit-code=%d\n", exit_code);
	    } else {
		printf("child status=%04x\n", status);
	    }
	}
    }
 nomatch:
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_nTexMapX, m_nTexMapY, 0, GL_RGB, GL_UNSIGNED_BYTE, m_pTexMap);

    // Display the OpenGL texture map
    glColor4f(1,1,1,1);

    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);

    g_nXRes = depthFrame.getVideoMode().getResolutionX();
    g_nYRes = depthFrame.getVideoMode().getResolutionY();

    // upper left
    glTexCoord2f(0, 0);
    glVertex2f(0, 0);
    // upper right
    glTexCoord2f((float)g_nXRes/(float)m_nTexMapX, 0);
    glVertex2f(GL_WIN_SIZE_X, 0);
    // bottom right
    glTexCoord2f((float)g_nXRes/(float)m_nTexMapX, (float)g_nYRes/(float)m_nTexMapY);
    glVertex2f(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
    // bottom left
    glTexCoord2f(0, (float)g_nYRes/(float)m_nTexMapY);
    glVertex2f(0, GL_WIN_SIZE_Y);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    char str[20];
    sprintf(str, "finger:%d", fingerNum);
    glColor3f(1.0, 0.0, 0.0);
    glRasterPos2i(20, 20);
    glPrintString(GLUT_BITMAP_HELVETICA_18, str);

    const nite::Array<nite::GestureData>& gestures = handFrame.getGestures();
    for (int i = 0; i < gestures.getSize(); ++i) {
	if (gestures[i].isComplete()) {
	    const nite::Point3f& position = gestures[i].getCurrentPosition();
	    printf("Gesture %d at (%f,%f,%f)\n", gestures[i].getType(), position.x, position.y, position.z);
	    
	    nite::HandId newId;
	    m_pHandTracker->startHandTracking(gestures[i].getCurrentPosition(), &newId);
	}
    }

    const nite::Array<nite::HandData>& hands= handFrame.getHands();
    for (int i = 0; i < hands.getSize(); ++i) {
	const nite::HandData& user = hands[i];
	
	if (!user.isTracking()) {
	    printf("Lost hand %d\n", user.getId());
	    nite::HandId id = user.getId();
	    HistoryBuffer<20>* pHistory = g_histories[id];
	    g_histories.erase(g_histories.find(id));
	    delete pHistory;
	} else {
	    if (user.isNew()) {
		printf("Found hand %d\n", user.getId());
		g_histories[user.getId()] = new HistoryBuffer<20>;
	    }
	    // Add to history
	    HistoryBuffer<20>* pHistory = g_histories[user.getId()];
	    pHistory->AddPoint(user.getPosition());
	    // Draw history
	    DrawHistory(m_pHandTracker, user.getId(), pHistory);
	}
    }

    if (g_drawFrameId)
	{
	    DrawFrameId(handFrame.getFrameIndex());
	}

    // Swap the OpenGL display buffers
    glutSwapBuffers();

}

void SampleViewer::OnKey(unsigned char key, int /*x*/, int /*y*/)
{
    printf("onkey key:%d\n", key);
    switch (key)
	{
	case 27:
	    Finalize();
	    exit (1);
	    /*
	case GLUT_KEY_PAGE_UP:
	case GLUT_KEY_UP:
	case '[':
	    if (thresh<245)
		thresh += 10;
	    else 
		thresh = 255;
	    printf("UP thresh:%d\n", thresh);
	    break;
	case ']':
	case GLUT_KEY_PAGE_DOWN:
	case GLUT_KEY_DOWN:
	    if (thresh>10)
		thresh -= 10;
	    else 
		thresh = 0;
	    printf("DOWN thresh:%d\n", thresh);
	    break;
	    */
	case 'd':
	    g_drawDepth = !g_drawDepth;
	    break;
	case 's':
	    if (g_smoothing)
		{
		    // Turn off smoothing
		    m_pHandTracker->setSmoothingFactor(0);
		    g_smoothing = FALSE;
		}
	    else
		{
		    m_pHandTracker->setSmoothingFactor(0.1);
		    g_smoothing = TRUE;
		}
	    break;
	case 'f':
	    // Draw frame ID
	    g_drawFrameId = !g_drawFrameId;
	    break;
	}

}

openni::Status SampleViewer::InitOpenGL(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
    glutCreateWindow (m_strSampleName);
    // 	glutFullScreen();
    glutSetCursor(GLUT_CURSOR_NONE);

    InitOpenGLHooks();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    return openni::STATUS_OK;

}
void SampleViewer::InitOpenGLHooks()
{
    glutKeyboardFunc(glutKeyboard);
    glutDisplayFunc(glutDisplay);
    glutIdleFunc(glutIdle);
}
