/**
 * easy debugger
 */
#include <opencv2/opencv.hpp>
#include "show.h"

// show(video_data.pdata, video_data.width, video_data.height, CAMERA_CAPTURE_PIX_FORMAT);
// TODO: halfway, this is too raw.
void show(void *yuv422_ptr, int width, int height, int pixel_format)
{
    cv::Mat bgr_img;
    cv::Mat yuv_img(height, width, CV_8UC2, yuv422_ptr);
    cv::cvtColor(yuv_img, bgr_img, cv::COLOR_YUV2BGR_YUYV);

    printf("width=%d/height=%d\n", width, height);
    cv::imshow("YUV Image", bgr_img);
    cv::waitKey(0); // 等待按鍵按下
    return;
}