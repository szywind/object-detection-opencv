
#include <iostream>
#include <fstream>
#include <string>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <cmath>
#include "ic.h"

using namespace cv;
using namespace std;

string getImageName(const string& fullname) {
	size_t lastindex = fullname.find_last_of(".");
	if (lastindex == string::npos) {
		return fullname;
	}
	string rawname = fullname.substr(0, lastindex);
	return rawname;
}

string getFileName(const string& s) {
	char sep = '/';
#ifdef _WIN32
	sep = '\\';
#endif

	size_t i = s.rfind(sep, s.length());
	if (i != string::npos) {
		return (s.substr(i + 1, s.length() - i));
	}
	return "";
}

// https://stackoverflow.com/questions/3071665/getting-a-directory-name-from-a-filename
string getParentDir(const string& str)
{
	size_t found;
	//cout << "Splitting: " << str << endl;
	found = str.find_last_of("/\\");
	//cout << " folder: " << str.substr(0, found) << endl;
	//cout << " file: " << str.substr(found + 1) << endl;
	return str.substr(0, found);
}

void sharpen(Mat src, Mat & dst) {
	Mat kernel = (Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
	filter2D(src, dst, -1, kernel);
}

int crop_batch(const string& in_dir, const string& outputDir)
{
	vector<cv::String> fn;
	vector<cv::String> fjpg;

	glob(in_dir + "\\*.jpg", fjpg, false);
	fn.insert(fn.end(), fjpg.begin(), fjpg.end());

	vector<cv::String> fpng;
	glob(in_dir + "\\*.png", fpng, false);
	fn.insert(fn.end(), fpng.begin(), fpng.end());

	for (auto img_path : fn) {
		crop(img_path, outputDir);
	}
	return 0;
}

int crop(const string& inputImageName, const string& outputDir)
{
	// 
	std::cout << "Loading and processing image " << inputImageName << std::endl;
	cv::Mat img_bgr, img_rgb, img_gray2;
	img_bgr = cv::imread(inputImageName);
	cv::cvtColor(img_bgr, img_rgb, CV_BGR2RGB);
	cv::cvtColor(img_bgr, img_gray2, CV_BGR2GRAY);
	int height = img_bgr.rows, width = img_bgr.cols;


	cv::Mat img_gray;

	// v0.4 
	// bilateralFilter(img_gray2, img_gray, 7, 15, 3);
	// Canny(img_gray, img_gray, 40, 60, 3);

	// v0.5
	img_gray = 255 - img_gray2;
	// blur(img_gray, img_gray, Size(3, 3));


	cv::Mat element = getStructuringElement(MORPH_ELLIPSE, Size(25, 25));
	morphologyEx(img_gray, img_gray, MORPH_CROSS, element);

	cv::Mat dst;

	// v0.4
	// cv::threshold(img_gray, dst, 0, 255, CV_THRESH_TRIANGLE);

	// v0.5
	cv::erode(img_gray, img_gray, 4);
	cv::dilate(img_gray, img_gray, 4);
	cv::threshold(img_gray, dst, 0, 255, CV_THRESH_OTSU);


	double maxArea = 0;
	//vector<cv::Point> maxContour = vector<cv::Point>();
	int maxIndex = -1;

	vector<vector<cv::Point> > contours;
	findContours(dst, contours, CV_RETR_TREE, CV_CHAIN_APPROX_NONE); //CV_RETR_EXTERNAL
	for (int i = 0; i < contours.size(); i++) {
		cv::Rect box = cv::boundingRect(contours[i]);
		double area = box.area();
		double midx = (box.x + 0.5*box.width) / width, midy = (box.y + 0.5*box.height) / height;
		double left_ratio = box.x / double(width), right_ratio = (box.x + box.width) / double(width);
		double top_ratio = box.y / double(height), bottom_ratio = (box.y + box.height) / double(height);
		if (area > height * width * 0.8 
			|| midx < 0.1 || midx > 0.9 || midy < 0.1 || midy > 0.9
			|| left_ratio < 0.02 || right_ratio > 0.98 
			|| top_ratio < 0.02 || bottom_ratio > 0.98) {
			continue;
		}
		if (area > maxArea) {
			maxArea = area;
			// maxContour = contours[i];
			maxIndex = i;
		}
	}

	if (-1 == maxIndex) {
		std::cout << "no item found" << std::endl;
		return -1;
	}

	cv::Rect box = cv::boundingRect(contours[maxIndex]);
	cv::rectangle(img_rgb, box, cv::Scalar(0, 255, 0), 8);


	// Find the minimum area enclosing triangle
	cv::Point2f vtx[4];
	cv::RotatedRect rect = cv::minAreaRect(contours[maxIndex]);
	rect.points(vtx);

	// Draw the bounding box
	//for (int i = 0; i < 4; i++) {
	//	cv::line(img_rgb, vtx[i], vtx[(i + 1) % 4], Scalar(0, 0, 255), 16, LINE_AA);
	//}


	// Save results
	string outputImageName = getFileName(inputImageName);
	cv::imwrite(outputDir + "\\" + outputImageName, img_rgb);

	//std::ofstream outFile1(outputDir + "\\" + outputImageName + "_box1.txt");
	string inputDir = getParentDir(inputImageName);

	std::ofstream outFile1(inputDir + "\\" + getImageName(outputImageName) + ".txt");

	vector<double> box1 = vector<double>{ (box.x+0.5*box.width)/width, (box.y+0.5*box.height)/height, double(box.width)/width, double(box.height)/height};
	for (const auto &e : box1) {
		outFile1 << e << "\n";
	}
	outFile1.close();

	//std::ofstream outFile2(outputDir + "\\" + outputImageName + "_box2.txt");
	//for (const auto &e : vtx) {
	//	outFile2 << e.x << "\n";
	//	outFile2 << e.y << "\n";
	//}
	//outFile2.close();
	return 0;
}

int main(int argc, char* argv[])
{
	string inputImageName, outputDir;
	double defect_size, field_width;
	if (argc != 3 && argc != 4 && argc != 5)
	{
		std::cout << "Usage: ItemCrop.exe <src image path> <dst image path> [<-b>]" << endl;

		return -1;
	}
	else {
		inputImageName = argv[1];
		outputDir = argv[2];
		if (argc == 4 && 0 == (argv[3] == "-b")) {
			crop_batch(inputImageName, outputDir);

		} else {
			crop(inputImageName, outputDir);
		}
	}
	return 0;
}