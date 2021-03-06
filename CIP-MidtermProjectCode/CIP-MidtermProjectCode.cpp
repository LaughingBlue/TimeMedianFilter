// CIP-MidtermProjectCode.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <omp.h>
#include <opencv2/opencv.hpp>

int main(int argc, char *argv[])
{
	constexpr unsigned int NORMALIZED_WIDTH = 427;
	constexpr unsigned int NORMALIZED_HEIGHT = 240;
	cv::VideoCapture videoCap(argv[1]);
	if (!videoCap.isOpened())
	{
		std::cerr << "CAN NOT OPEN CAMERA OR VIDEO.\n";
		return -1;
	}
	videoCap.set(cv::CAP_PROP_FRAME_WIDTH, NORMALIZED_WIDTH);
	videoCap.set(cv::CAP_PROP_FRAME_HEIGHT, NORMALIZED_HEIGHT);

	cv::Mat srcVideoFrame, grayFrame;
	std::vector<cv::Mat> srcFrames, grayFrames, motionMaskFrames, movingObjFrames;

	//========== Load video ==========
	while (true)
	{
		videoCap >> srcVideoFrame;
		if (srcVideoFrame.empty())
			break;

		cv::resize(srcVideoFrame, srcVideoFrame, cv::Size(NORMALIZED_WIDTH, NORMALIZED_HEIGHT));
		cv::blur(srcVideoFrame, srcVideoFrame, cv::Size(5, 5));
		cv::cvtColor(srcVideoFrame, grayFrame, CV_BGR2GRAY);
		cv::equalizeHist(grayFrame, grayFrame);

		srcFrames.push_back(srcVideoFrame.clone());
		grayFrames.push_back(grayFrame.clone());
	}

	//========== Time Median Filter background extraction process ==========
	cv::Mat gray_bgImg = cv::Mat::zeros(NORMALIZED_HEIGHT, NORMALIZED_WIDTH, CV_8UC1);
	
	#pragma omp parallel for num_threads(omp_get_num_procs())
	for (int imgRowIndex = 0; imgRowIndex < gray_bgImg.rows; ++imgRowIndex)
	{
		for (int imgColIndex = 0; imgColIndex < gray_bgImg.cols; ++imgColIndex)
		{
			std::vector<uchar> pixel2sort;
			for (std::vector<cv::Mat>::iterator frameItr = grayFrames.begin(); frameItr != grayFrames.end(); ++frameItr)
			{
				pixel2sort.push_back(frameItr->at<uchar>(imgRowIndex, imgColIndex));
			}
			std::sort(pixel2sort.begin(), pixel2sort.end());
			gray_bgImg.at<uchar>(imgRowIndex, imgColIndex) = pixel2sort.at(pixel2sort.size()/2);
		}
	}

	//========== frame differences calculating(foregroud masking) ==========
	cv::Mat diffImage;
	
	const float threshold = 30.0f;
	float dist;

	for (std::vector<cv::Mat>::iterator frameItr = grayFrames.begin(); frameItr != grayFrames.end(); ++frameItr)
	{
		cv::absdiff(gray_bgImg, *frameItr, diffImage);
		cv::Mat foregroundMask = cv::Mat::zeros(diffImage.rows, diffImage.cols, CV_8UC1);		
		#pragma omp parallel for num_threads(omp_get_num_procs())
		for (int rowIndex = 0; rowIndex < diffImage.rows; ++rowIndex)
		{
			for (int colIndex = 0; colIndex < diffImage.cols; ++colIndex)
			{
				uchar dist = diffImage.at<uchar>(rowIndex, colIndex);

				if (dist > threshold)
				{
					foregroundMask.at<uchar>(rowIndex, colIndex) = 255;
				}
			}
		}
		motionMaskFrames.push_back(foregroundMask.clone());
		cv::cvtColor(diffImage, diffImage, CV_GRAY2BGR);
		movingObjFrames.push_back(diffImage);
	}

	//========== Result display and output ========	
	cv::VideoWriter writer;
	cv::Size videoSize = cv::Size(NORMALIZED_WIDTH * 2, NORMALIZED_HEIGHT * 2);
	writer.open("./result.mp4", CV_FOURCC('M', 'J', 'P', 'G'), 20, videoSize);

	cv::Mat blockClear = cv::Mat::zeros(NORMALIZED_HEIGHT, NORMALIZED_WIDTH, CV_8UC3);
	cv::Mat resultCanvas = cv::Mat::zeros(NORMALIZED_HEIGHT*2, NORMALIZED_WIDTH*2, CV_8UC3);

	cv::cvtColor(gray_bgImg, gray_bgImg, CV_GRAY2BGR);
	cv::putText(gray_bgImg, 
				"Grayscale Background Image", 
				cv::Point(20, 30), 
				cv::FONT_HERSHEY_SIMPLEX, 
				0.7, 
				cv::Scalar(0, 0, 255), 2);
	gray_bgImg.copyTo(resultCanvas(cv::Rect(NORMALIZED_WIDTH, 0, NORMALIZED_WIDTH, NORMALIZED_HEIGHT))); // top right corner. Extracted background.
	

	for (int index = 0; index < srcFrames.size(); ++index)
	{
		srcFrames.at(index).copyTo(resultCanvas(cv::Rect(0, 0, NORMALIZED_WIDTH, NORMALIZED_HEIGHT))); // top left corner. Original video frames.
		cv::putText(resultCanvas(cv::Rect(0, 0, NORMALIZED_WIDTH, NORMALIZED_HEIGHT)),
					"Source Video", 
					cv::Point(20, 30), 
					cv::FONT_HERSHEY_SIMPLEX, 
					0.7, 
					cv::Scalar(0, 0, 255), 2);

		movingObjFrames.at(index).copyTo(resultCanvas(cv::Rect(0, NORMALIZED_HEIGHT, NORMALIZED_WIDTH, NORMALIZED_HEIGHT))); // bottom left corner. Show img difference distance.
		cv::putText(resultCanvas(cv::Rect(0, NORMALIZED_HEIGHT, NORMALIZED_WIDTH, NORMALIZED_HEIGHT)), 
					"Difference HeatMap", 
					cv::Point(20, 30), 
					cv::FONT_HERSHEY_SIMPLEX,
					0.7,
					cv::Scalar(0, 0, 255), 2);

		blockClear.copyTo(resultCanvas(cv::Rect(NORMALIZED_WIDTH, NORMALIZED_HEIGHT, NORMALIZED_WIDTH, NORMALIZED_HEIGHT))); // bottom right corner. Clear block to display correct result.
		srcFrames.at(index).copyTo(resultCanvas(cv::Rect(NORMALIZED_WIDTH, NORMALIZED_HEIGHT, NORMALIZED_WIDTH, NORMALIZED_HEIGHT)), motionMaskFrames.at(index)); // bottom right corner. masked obj frame.
		cv::putText(resultCanvas(cv::Rect(NORMALIZED_WIDTH, NORMALIZED_HEIGHT, NORMALIZED_WIDTH, NORMALIZED_HEIGHT)),
					"Foreground Object",
					cv::Point(20, 30),
					cv::FONT_HERSHEY_SIMPLEX, 
					0.7, 
					cv::Scalar(0, 0, 255), 2);

		cv::imshow("Result", resultCanvas);
		writer << resultCanvas;
		cv::waitKey(30);
	}
}
