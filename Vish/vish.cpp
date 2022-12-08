// VideoCS.cpp : Defines the entry point for the console application.
//


#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <chrono>
#include <thread>

using namespace cv;
using namespace std;
using namespace std::chrono;

string inFile, outFile, outputAffix, outExtension; //input and output file full paths
VideoCapture vid; //input video object
VideoWriter cs; //output video object
int out4cc; //output file encoder/4character code
int verboseName, timeVertical, playbuild, translate;
steady_clock::time_point start, update; //clock vars for estimating time to completion in UI

int csCols, csRows; //rows and columns of resultant contact sheet
int thumblength; //flag to autocalculate columns and rows for a clip duration in seconds
double vidFPS, vidFrames, vidHeight, vidWidth; //properties read from video file

int tstamps; //flag to include timestamp on each thumb
int vrsbs; //flag to split side-by-side vr source
float fontscaleHeight; //pixel scaling factor for timestamp
int fontR, fontG, fontB, tstampboxR, tstampboxG, tstampboxB; //font and font backing box colors
int tstampbox; //flag for timestamp backing box
Size tstampboxSize; //size of timestamp backing box
Point fontPnt,vrfontPnt; //coordinate for timestamp
int msec, hrs, mins, secs; //timestamp parameters
char buffer[10]; //timestamp char buffer
string timestamp; //constructed timestamp string

string exedir; //full path of .exe
ifstream iniFile; //.ini file for input
string iniLine; //.ini file line
string configVar, configVarValue;

int csCells, csFrames; //number of grid cells and frames of output video
Size outSize; //output resolutoin
int cellWidth, cellHeight; //pixel dimensions of thumbnail video
int csOffset, vidOffset; //translation effect offset in pixels
int invert; //invert time flow

Mat *screenshots; //array of thumbnail frames
Mat grabFrame; //single frame from input video file
Mat thumbFrame; //resized grabFrame
Mat csFrame; //single frame of output video
Mat csRoi, thumbRoi; //region-of-interest of output frame in which to write thumbnail image
int cell_x, cell_y; //coordinates for region-of-interest
Point xyPoint; //Point of cell x/y
Point cellCoord; //resolution of cscell

int files, frame, row, col; //looping vars

void updateUser(string filename, int appTask, double fileProgress, int fileNum, int numFiles, steady_clock::time_point startTime, steady_clock::time_point updateTime){

	system("cls");

	filename = filename.substr(filename.find_last_of("\\") + 1, filename.length() - 1);

	int elapsedSeconds = duration_cast<seconds>(steady_clock::now() - startTime).count();
	int remainingSeconds = (100 - fileProgress) * (elapsedSeconds / fileProgress);

	switch (appTask){
	case 0:
		cout << "Building frames for ";
		cout << filename;
		cout << " (" << fileProgress << "%, " << remainingSeconds << " seconds remaining)" << endl;
		break;
	case 1:
		cout << "Writing output file ";
		cout << filename;
		break;
	}

	string titleBarStr("Video Contact Sheet ");
	titleBarStr += filename;
	titleBarStr += " (";
	titleBarStr += to_string(fileNum);
	titleBarStr += "/";
	titleBarStr += to_string(numFiles);
	titleBarStr += ")";

	SetConsoleTitleA(titleBarStr.c_str());

}

void stampIt(Mat tsscreen) {

	//add  timestamp
		msec = vid.get(CAP_PROP_POS_MSEC);
		hrs = (msec / 3600000);
		mins = (msec % 3600000) / 60000;
		secs = ((msec % 3600000) % 60000) / 1000;
		msec = (((msec % 3600000) % 60000) % 1000);
		sprintf_s(buffer, "%0*d:%0*d:%0*d", 2, hrs, 2, mins, 2, secs); //format timestamp vars at least two characters wide
		timestamp = buffer;
		if (tstampbox > 0){
			tstampboxSize = getTextSize(timestamp, FONT_HERSHEY_PLAIN, cellHeight / fontscaleHeight, 1, 0);
			rectangle(tsscreen, fontPnt, Point(fontPnt.x + tstampboxSize.width, fontPnt.y - tstampboxSize.height), Scalar(tstampboxB, tstampboxG, tstampboxR), FILLED, LINE_AA, 0);
			if (vrsbs) rectangle(tsscreen, vrfontPnt, Point(vrfontPnt.x + tstampboxSize.width, vrfontPnt.y - tstampboxSize.height), Scalar(tstampboxB, tstampboxG, tstampboxR), FILLED, LINE_AA, 0);
		}
		putText(tsscreen, timestamp, fontPnt, FONT_HERSHEY_PLAIN, cellHeight / fontscaleHeight, Scalar(fontB, fontG, fontR), 1, LINE_AA);
		if (vrsbs) putText(tsscreen, timestamp, vrfontPnt, FONT_HERSHEY_PLAIN, cellHeight / fontscaleHeight, Scalar(fontB, fontG, fontR), 1, LINE_AA);
}

int main(int argc, char* argv[])
{
	int frmnum;

	//loop through command line arguments
	for (files = 1; files < argc; files++) {
	//for (files = 1; files < 2; files++) { //debug loop
		//playbuild = 1;

		//default to static 10x10 grid
		thumblength = 0;
		csCols = 10;
		csRows = 10;

		//timestamp default variables
		tstamps = 1; //add timestamp
		fontscaleHeight = 108.0; //in pixels, fontscale=1 looks good with 10 columns in 1080 resolutoin
		tstampbox = 0; //no backing box
		fontR = fontG = fontB = 255;
		tstampbox = 1;
		tstampboxR = tstampboxG = tstampboxB = 0;

		inFile = argv[files];
		//inFile = "C:/Users/tylon/Videos/Halt And Catch Fire S01E01.mp4"; //Debug file

		//default necessary output parameters that may not be set by ini file
		out4cc = VideoWriter::fourcc('D', 'I', 'V', 'X');
		verboseName = 0;
		timeVertical = 0;
		translate = 0;
		outExtension = "mp4";

		//load input video
		vid = VideoCapture(inFile);
		if (!vid.isOpened()) continue; //continue to next file if a file cannot be loaded

		//default output video properties to those of input file
		vidFPS = vid.get(CAP_PROP_FPS);
		vidFrames = vid.get(CAP_PROP_FRAME_COUNT);
		vidHeight = vid.get(CAP_PROP_FRAME_HEIGHT);
		vidWidth = vid.get(CAP_PROP_FRAME_WIDTH);

		//open ini file and load settings
		exedir = argv[0];
		iniFile.open(exedir.substr(0, exedir.find_last_of("\\") + 1) + "vish.ini");
		if (iniFile.is_open()) {
			while (iniFile.good()) { //loop through .ini lines

				getline(iniFile, iniLine);
				iniLine = iniLine.substr(0, iniLine.find_first_of(";"));
				configVar = iniLine.substr(0, iniLine.find_last_of("="));
				configVarValue = iniLine.substr(iniLine.find_last_of("=") + 1, iniLine.length());

				if (configVar == "4cc") {
					out4cc = VideoWriter::fourcc(configVarValue.at(0), configVarValue.at(1), configVarValue.at(2), configVarValue.at(3));
				}
				if (configVar == "out_extension") {
					outExtension = configVarValue;
				}
				if (configVar == "grid_width") {
					csCols = atoi(configVarValue.c_str());
				}
				if (configVar == "grid_height") {
					csRows = atoi(configVarValue.c_str());
				}
				if (configVar == "res_width") {
					vidWidth = atoi(configVarValue.c_str());
				}
				if (configVar == "res_height") {
					vidHeight = atoi(configVarValue.c_str());
				}
				if (configVar == "thumb_duration") {
					thumblength = atoi(configVarValue.c_str());
				}
				if (configVar == "timestamps") {
					tstamps = atoi(configVarValue.c_str());
				}
				if (configVar == "font_red") {
					fontR = atoi(configVarValue.c_str());
				}
				if (configVar == "font_green") {
					fontG = atoi(configVarValue.c_str());
				}
				if (configVar == "font_blue") {
					fontB = atoi(configVarValue.c_str());
				}
				if (configVar == "timestamp_box_red") {
					tstampboxR = atoi(configVarValue.c_str());
				}
				if (configVar == "timestamp_box_green") {
					tstampboxG = atoi(configVarValue.c_str());
				}
				if (configVar == "timestamp_box_blue") {
					tstampboxB = atoi(configVarValue.c_str());
				}
				if (configVar == "timestamp_backing_box") {
					tstampbox = atoi(configVarValue.c_str());
				}
				if (configVar == "codec_code") {
					out4cc = atoi(configVarValue.c_str());
				}
				if (configVar == "verbose_name") {
					verboseName = atoi(configVarValue.c_str());
				}
				if (configVar == "invert") {
					timeVertical = atoi(configVarValue.c_str());
				}
				if (configVar == "playbuild") {
					playbuild = atoi(configVarValue.c_str());
				}
				if (configVar == "translate") {
					translate = atoi(configVarValue.c_str());
				}
				if (configVar == "VRsbs") {
					vrsbs = atoi(configVarValue.c_str());
				}
			}
			//close ini file after loading values
			iniFile.close();
		}
		else {
			//tell user couldn't load .ini
			system("cls");
			cout << exedir.substr(0, exedir.find_last_of("\\") + 1) << "vish.ini could not be loaded. Using default settings." << endl;
			system("pause");
		}

		//read first frame
		vid >> grabFrame;

		//build output file name
		outFile = inFile;  //modify input filename
		outputAffix = "_vish";
		if (verboseName > 0) {
			outputAffix = outputAffix + "_" + to_string(int(vidWidth)) + "x" + to_string(int(vidHeight));
			if (thumblength > 0)
				outputAffix = outputAffix + "_" + to_string(thumblength) + "s";
			else
				outputAffix = outputAffix + "_" + to_string(csRows) + "x" + to_string(csCols);
			if (tstamps > 0)
				outputAffix = outputAffix + "_ts";
		}
		outFile.insert(outFile.find_last_of("."), outputAffix); //output file in same location as input w/ modified name
		outFile = outFile.substr(0, outFile.find_last_of(".") + 1).append(outExtension); //change file type to .avi
		//if a duration is given for thumbnails, calculate rows/cols of closest square grid fitting
		if (thumblength > 0){
			csRows = sqrt((vidFrames / vidFPS) / thumblength);
			csCols = csRows + 1;
		} 

		//infer useful variables
		csCells = csCols * csRows;
		csFrames = (int)vidFrames / csCells;
		outSize = Size(vidWidth, vidHeight);
		cellWidth = (int)vidWidth / csCols;
		cellHeight = (int)vidHeight / csRows;
		cellCoord = Point(cellWidth, cellHeight);
		fontPnt = Point(2, cellHeight - 4); //position of timestamp
		vrfontPnt = Point(2 + cellWidth/2, cellHeight - 4);

		//test output file before sizing thumbs
		if (out4cc == 0) {
			cs.open(outFile, -1, vidFPS, outSize, true); //open output vidoe file, prompting user for encoder/4cc
		}
		else {
			cs.open(outFile, out4cc, vidFPS, outSize, true); //open output vidoe file for writting
		}
		if (!cs.isOpened()) //check whether the video is loaded or not
		{
			cout << "Error : " << outFile << " cannot be loaded with " << out4cc << "." << endl;
			system("pause"); //wait for a key press
			return -1;
		}

		//Define thumbnail image array
		screenshots = new Mat[csFrames];
		for (frame = 0; frame < csFrames; frame++)
			screenshots[frame] = Mat(vidHeight, vidWidth, grabFrame.type());

		//window for preview playback
		if (playbuild != 0) namedWindow("build", WINDOW_NORMAL);

		//Read video frames into thumbnail array
		start = steady_clock::now(); //start clock for ui
		update = steady_clock::now();
		for (row = 0; row < csRows; row++){
			for (col = 0; col < csCols; col++){
				for (frame = 0; frame < csFrames; frame++){
					
					//calculate thumbnail coordinate within grid
					if (timeVertical == 0) {
						cell_y = cellHeight * row;
						if (vrsbs == 0) 
							cell_x = cellWidth * col;						
						else 
							cell_x = cellWidth * col / 2;						
					}
					else {
						//convert horizontal time position to vertical time position
						frmnum = ((row * csCols) + col);
						cell_y = cellHeight * (frmnum % csRows);
						if (vrsbs == 0) 
							cell_x = cellWidth * (frmnum / csRows);
						else 
							cell_x = cellWidth * (frmnum / csRows) / 2;
					}
					xyPoint = Point(cell_x, cell_y);

					//resize input frame into output thumbnail
					resize(grabFrame, thumbFrame, cellCoord, 0, 0, INTER_AREA);
					//add optional timestamp
					if (tstamps) stampIt(thumbFrame);
					
					//if translation is off, copy thumb into grid
					if (translate == 0) {
						if (!vrsbs) {
							csRoi = Mat(screenshots[frame], Rect(xyPoint, xyPoint + cellCoord));
							thumbFrame.copyTo(csRoi);
						}
						else {
							csRoi = Mat(screenshots[frame], Rect(xyPoint, xyPoint + Point(cellWidth/2, cellHeight)));
							thumbRoi = Mat(thumbFrame, Rect(Point(0, 0), csRoi.size()));
							thumbRoi.copyTo(csRoi);
							csRoi = Mat(screenshots[frame], Rect(xyPoint + Point(vidWidth/2,0), xyPoint + Point(vidWidth/2, 0) + Point(cellWidth / 2, cellHeight)));
							thumbRoi = Mat(thumbFrame, Rect(Point(cellWidth/2, 0), csRoi.size()));
							thumbRoi.copyTo(csRoi);
						}
					}
					//if translation is on, split thumb if necessary
					else {

						//shift cell region-of-insterest proportional to time
						if (timeVertical == 0) {
							csOffset = cellWidth * (frame / (float)csFrames);
							xyPoint = xyPoint + Point(csOffset, 0);
						}
						else {
							csOffset = cellHeight * (frame / (float)csFrames);
							xyPoint = xyPoint + Point(0, csOffset);
						}

						//special case: horizontal time progression and thumb extends past right of grid
						if (timeVertical == 0 && (xyPoint.x + cellWidth) > vidWidth){

							csRoi = Mat(screenshots[frame], Rect(xyPoint, Point(vidWidth - 1, xyPoint.y + cellHeight)));
							thumbRoi = Mat(thumbFrame, Rect(Point(0, 0), csRoi.size()));
							thumbRoi.copyTo(csRoi);

							//if not final row, wrap to left of next row
							if (row + 1 < csRows) {

								csRoi = Mat(screenshots[frame], Rect(Point(0, xyPoint.y + cellHeight), Point(csOffset, xyPoint.y + (2 * cellHeight))));
								thumbRoi = Mat(thumbFrame, Rect(Point(vidWidth - xyPoint.x, 0), csRoi.size()));
								thumbRoi.copyTo(csRoi);

						}	
							//if final row, wrap to top left
							else {

								csRoi = Mat(screenshots[frame], Rect(Point(0, 0), Point(csOffset, cellHeight)));
								thumbRoi = Mat(thumbFrame, Rect(Point(vidWidth - xyPoint.x, 0), csRoi.size()));
								thumbRoi.copyTo(csRoi);

							}

						}
						//special case: vertical time progression and thumb extends past bottom of grid
						else if (timeVertical != 0 && (xyPoint.y + cellHeight) > vidHeight){
							
							csRoi = Mat(screenshots[frame], Rect(xyPoint, Point(xyPoint.x + cellWidth, vidHeight - 1)));
							thumbRoi = Mat(thumbFrame, Rect(Point(0, 0), csRoi.size()));
							thumbRoi.copyTo(csRoi);

							//if not final column, wrap to top of next column
							if (col + 1 < csCols) {

								csRoi = Mat(screenshots[frame], Rect(Point(xyPoint.x + cellWidth, 0), Point(xyPoint.x + (2 * cellWidth), csOffset)));
								thumbRoi = Mat(thumbFrame, Rect(Point(0, vidHeight - xyPoint.y), csRoi.size()));
								thumbRoi.copyTo(csRoi);

							}
							//if final column, wrap to top left
							else {

								csRoi = Mat(screenshots[frame], Rect(Point(0, 0), Size(cellWidth, csOffset)));
								thumbRoi = Mat(thumbFrame, Rect(Point(0, vidHeight - xyPoint.y), csRoi.size()));
								thumbRoi.copyTo(csRoi);

							}

						}
						//general case: thumbnail won't extend past edge of grid
						else {
							csRoi = Mat(screenshots[frame], Rect(xyPoint, xyPoint + cellCoord));
							thumbFrame.copyTo(csRoi);
						}
					}
					
					//optional build playback
					if (playbuild != 0) {

						imshow("build", screenshots[frame]);
						waitKey(1);

					}

					//read next frame, exit if done
					vid >> grabFrame;
					if (grabFrame.empty()) break;

				}

				//UI update
				updateUser(inFile, 0, 100.0 * (row * csCols + col + 1) / csCells, files, argc - 1, start, update);
				update = steady_clock::now();

				if (grabFrame.empty()) break; //exit looping if end of video
			}
			if (grabFrame.empty()) break; //exit looping if end of video
		}
		vid.release(); //close input file

		//write out file
		for (frame = 0; frame < csFrames; frame++){
			
			//write frame
			cs.write(screenshots[frame]);
			
		}

		//close output file
		cs.release();
		
		//delete thumbnail array to free memory
		delete[] screenshots;
		
	}

	return 0;

}

