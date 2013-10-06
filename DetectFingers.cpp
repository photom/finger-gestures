#include "DetectFingers.h"

#include <opencv/highgui.h>
#include <opencv2/core/core.hpp>
#include "thinning.h"

static const int UNIT = 80, BASE = 100;

using namespace cv;
using namespace std;

static int debug = 0;

DetectFingers::DetectFingers()
{
}
vector<nite::Point3f> DetectFingers::fingers(openni::VideoFrameRef depthFrame,
				       float pDepthHist[],
				       nite::Point3f hand)
{
    vector<nite::Point3f> fingers3;
    return fingers3;
}
vector<nite::Point3f> DetectFingers::fingersDebug(openni::VideoFrameRef depthFrame,
				       float pDepthHist[],
				       openni::RGB888Pixel* pTexMap,
				       unsigned int nTexMapX,
				       unsigned int nTexMapY,
				       nite::Point3f hand)
{
    Mat image(Size(UNIT, UNIT), CV_8U, Scalar::all(0));
    int cx=0, cy=0;
    cx = (int)hand.x-UNIT/2;
    cy = (int)hand.y-UNIT/2;
    if (debug)
	printf("cx:%d, cy:%d, UNIT:%d hand.x:%lf hand.y:%lf\n",cx, cy, UNIT, hand.x, hand.y);
    toMap(depthFrame, pDepthHist, cx, cy, image);

    Mat gray, gray2;
    int ch[] = {0, 0};
    vector< vector<Point> > contours;
    Mat mask = Mat::zeros(image.rows, image.cols, CV_8UC1);
    unsigned int maxIdx = -1;
    double maxA = -1;
    int avg = 0, count = 0;
    const int num = 1;
    int hist[256];
    const int UNDER_LIMIT = 200;
    const int FINGER_UPPER_LIMIT = 150;
    const int FINGER_COORD_LIMIT = 10;

    medianBlur(image, gray, 3);
    memset(hist, 0, sizeof(hist));	    
    for (int i=0; i<gray.cols; i++){
	for (int j=0; j<gray.rows; j++){
	    int val = gray.data[i*gray.step + j*gray.elemSize()];
	    avg += gray.data[i*gray.step + j*gray.elemSize()];
	    count++;
	    hist[val]++;
	    if (val>0)
		hist[val-1]++;
	    if (val<256)
		hist[val+1]++;
	}
    }
    int maxidx = -1, maxval = -1;
    for (int i=UNDER_LIMIT; i<256; i++){
	if (hist[i]>maxval){
	    maxval = hist[i];
	    maxidx = i;
	}
    }
	    
    avg /= count;
    int thresh1 = (avg-50)<0 ? 0 : (avg-10);
    int thresh2 = (avg+50)>255?255:(avg+10);
    if (debug)
	printf("avg:%d count:%d thresh1:%d thresh2:%d, maxidx:%d, maxval:%d \n", avg, count, thresh1,thresh2, maxidx, maxval);
    threshold(gray, gray, maxidx-20, 255, THRESH_BINARY);
    thinning(gray, gray2);
    vector<nite::Point3f> edges = edge(gray2);
    for (vector<nite::Point3f>::iterator it=edges.begin(); it!=edges.end(); it++)
	circle(gray, Point(it->x, it->y), 5, Scalar(250,250,250), -1);
    debugDisplay(depthFrame, pDepthHist, pTexMap, nTexMapX, nTexMapY, cx, cy, gray2);
    
    mask.release();
    gray.release();
    gray2.release();
    image.release();
    return edges;
}

void DetectFingers::toMap(openni::VideoFrameRef depthFrame,
			  float pDepthHist[],
			  int cornerX,
			  int cornerY,
			  Mat &image)
{
    const openni::DepthPixel* pDepthRow = (const openni::DepthPixel*)depthFrame.getData();
    int rowSize = depthFrame.getStrideInBytes() / sizeof(openni::DepthPixel);
    /*
    printf("rowSize:%d depthFrame.getStrideInBytes():%d \
depthFrame.getCropOriginX():%d depthFrame.getCropOriginY():%d \
depthFrame.getWidth():%d  depthFrame.getHeight():%d \
depthFrame.getVideoMode().getResolutionX():%d depthFrame.getVideoMode().getResolutionY():%d \
imagechannels:%d\n", 
	   rowSize, depthFrame.getStrideInBytes(), 
	   depthFrame.getCropOriginX(), depthFrame.getCropOriginY(), 
	   depthFrame.getWidth(), depthFrame.getHeight(),
	   depthFrame.getVideoMode().getResolutionX(), depthFrame.getVideoMode().getResolutionY(),
	   image.channels());
    */
    for (int y = 0; y < depthFrame.getHeight(); ++y) {
	const openni::DepthPixel* pDepth = pDepthRow;
	
	for (int x = 0; x < depthFrame.getWidth(); ++x, ++pDepth) {
	    if (*pDepth != 0) {
		int nHistValue = pDepthHist[*pDepth];
		if ((x>=cornerX) && (x<(cornerX+UNIT)) && (y>=cornerY) && (y<(cornerY+UNIT)))
		    for (int c=0; c<image.channels(); c++)
			image.data[(y-cornerY)*image.step + (x-cornerX)*image.elemSize() + c] += nHistValue;
	    }
	}

	pDepthRow += rowSize;
    }
}
void DetectFingers::debugDisplay(openni::VideoFrameRef depthFrame,
				 float pDepthHist[],
				 openni::RGB888Pixel* pTexMap,
				 unsigned int nTexMapX,
				 unsigned int nTexMapY,
				 int cornerX,
				 int cornerY,
				 Mat image)
{
    const openni::DepthPixel* pDepthRow = (const openni::DepthPixel*)depthFrame.getData();
    openni::RGB888Pixel* pTexRow = pTexMap + depthFrame.getCropOriginY() * nTexMapX;
    int rowSize = depthFrame.getStrideInBytes() / sizeof(openni::DepthPixel);
    for (int y = 0; y < depthFrame.getHeight(); ++y) {
	const openni::DepthPixel* pDepth = pDepthRow;
	openni::RGB888Pixel* pTex = pTexRow + depthFrame.getCropOriginX();
		
	for (int x = 0; x < depthFrame.getWidth(); ++x, ++pDepth, ++pTex) {
	    if (*pDepth != 0) {
		if ((x>=cornerX) && (x<(cornerX+UNIT)) && (y>=cornerY) && (y<(cornerY+UNIT))){
		    int val = image.data[(y-cornerY)*image.step + (x-cornerX)*image.elemSize() + 0];
		    /*
		      int val = 0;
		      for (int c=0; c<mask.channels(); c++)
		      val += mask.data[(y-cy)*mask.step + (x-cx)*mask.elemSize() + c];*/
		    pTex->r = val;
		    if (val!=250)
			pTex->g = val;
		    if (val!=250)
			pTex->b = val;
		}
	    }
	}
	pDepthRow += rowSize;
	pTexRow += nTexMapX;
    }
}


vector<nite::Point3f> DetectFingers::edge(Mat &image)
{
    CV_Assert(image.channels() == 1);
    CV_Assert(image.depth() != sizeof(uchar));
    CV_Assert(image.rows > 3 && image.cols > 3);
    Mat img = image.clone();
    img /= 255;         // convert to binary image

    vector<nite::Point3f> edge;
    int x, y;
    uchar *pAbove;
    uchar *pCurr;
    uchar *pBelow;
    uchar *nw, *no, *ne;    // north (pAbove)
    uchar *we, *me, *ea;
    uchar *sw, *so, *se;    // south (pBelow)

    uchar *pDst;

    // initialize row pointers
    pAbove = NULL;
    pCurr  = img.ptr<uchar>(0);
    pBelow = img.ptr<uchar>(1);

    for (y = 1; y < img.rows-1; ++y) {
        // shift the rows up by one
        pAbove = pCurr;
        pCurr  = pBelow;
        pBelow = img.ptr<uchar>(y+1);

        // initialize col pointers
        no = &(pAbove[0]);
        ne = &(pAbove[1]);
        me = &(pCurr[0]);
        ea = &(pCurr[1]);
        so = &(pBelow[0]);
        se = &(pBelow[1]);

        for (x = 1; x < img.cols-1; ++x) {
            // shift col pointers left by one (scan left to right)
            nw = no;
            no = ne;
            ne = &(pAbove[x+1]);
            we = me;
            me = ea;
            ea = &(pCurr[x+1]);
            sw = so;
            so = se;
            se = &(pBelow[x+1]);
	    
	    int neighbors = 0;
	    if (*me==1){
		if (*nw==1)
		    neighbors++;
		if (*no==1)
		    neighbors++;
		if (*ne==1)
		    neighbors++;
		if (*we==1)
		    neighbors++;
		if (*ea==1)
		    neighbors++;
		if (*sw==1)
		    neighbors++;
		if (*so==1)
		    neighbors++;
		if (*se==1)
		    neighbors++;
	    }
	    if (neighbors==1)
		edge.push_back(nite::Point3f(x, y, 0));
        }
    }
    img.release();
    return edge;
}
