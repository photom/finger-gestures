#ifndef _DETECT_FINGER_H_
#define _DETECT_FINGER_H_

#include "NiTE.h"
#include <opencv/cv.h>

class DetectFingers
{
 public:
    DetectFingers();
    std::vector<nite::Point3f> fingers(openni::VideoFrameRef depthFrame,
				  float pDepthHist[],
				  nite::Point3f hand);
    std::vector<nite::Point3f> fingersDebug(openni::VideoFrameRef depthFrame,
				       float pDepthHist[],
				       openni::RGB888Pixel* pTexMap,
				       unsigned int nTexMapX,
				       unsigned int nTexMapY,
				       nite::Point3f hand);
 protected:
 private:
    int winSizeX;
    int winSizeY;
    void toMap(openni::VideoFrameRef depthFrame,
	       float pDepthHist[],
	       int cornerX,
	       int cornerY,
	       cv::Mat &image);
    void debugDisplay(openni::VideoFrameRef depthFrame,
		      float pDepthHist[],
		      openni::RGB888Pixel* pTexMap,
		      unsigned int nTexMapX,
		      unsigned int nTexMapY,
		      int cornerX,
		      int cornerY,
		      cv::Mat image);
    std::vector<nite::Point3f> edge(cv::Mat &image);
};

#endif//_DETECT_FINGER_H_
