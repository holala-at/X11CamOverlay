/*
 * Copyright 2020 Dominik Hochreiter <hochreiter@holala.at>
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <opencv2/opencv.hpp>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <signal.h>
#include <chrono>
#include <thread>

using namespace cv;
using namespace std;

bool quit = false;
void sigintHandler(int s)
{
	quit = true;
}

int main(int argc, char **argv)
{
    setbuf (stdout, NULL);
    signal(SIGINT, sigintHandler);
    printf("ctrl-c to exit, have fun - and visit holala.at ;)\n\n");

    const std::string keys =
        "{cam|0|default cam device index, see OpenCV docs}"
        "{fps|25|limit, upper update rate of overlay}"
        "{fs|true|full screen at start}"
        "{ui|false|show UI}"
        "{alpha|30|default 30% transparent}"
        "{onlyAlphaChan|false|if true only alpha channel is changed}"
        ;
    cv::CommandLineParser parser ( argc, argv, keys );
    parser.printMessage();

    cv::VideoCapture* cap = new cv::VideoCapture();
    cap->open( parser.get<int>("cam") );
    if ( !cap->isOpened() ) {
        printf("No video signal found, terminating\n");
        exit(EXIT_FAILURE);
    }
    
    int wVid = (int)cap->get(cv::CAP_PROP_FRAME_WIDTH);
    int hVid = (int)cap->get(cv::CAP_PROP_FRAME_HEIGHT);
    
    
    Display *d = XOpenDisplay(NULL);
    Window root = DefaultRootWindow(d);
    
    XWindowAttributes wa;
    XGetWindowAttributes(d, root, &wa);
    
    XVisualInfo vinfo;
    if (!XMatchVisualInfo(d, DefaultScreen(d), 32, TrueColor, &vinfo)) {
        printf("No visual found supporting RGBA color, terminating\n");
        exit(EXIT_FAILURE);
    }
    

    XSetWindowAttributes attrs;
    attrs.override_redirect = true;
    attrs.colormap = XCreateColormap(d, root, vinfo.visual, AllocNone);
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    
    int wOver = wa.width/2;
    int hOver = (int)((float)(wa.width/2)/((float)wVid/(float)hVid));
    
    Window overlay = XCreateWindow(
        d, root,
        wa.width/4, wa.height/4, wOver, hOver, 0, 
        vinfo.depth, InputOutput, 
        vinfo.visual,
        CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel, &attrs
    );
    
    if(parser.get<bool>("fs"))
        XMoveResizeWindow(d,overlay,0,0,wa.width, wa.height);


    XRectangle rect;
    XserverRegion region = XFixesCreateRegion(d, &rect, 1);
    XFixesSetWindowShapeRegion(d, overlay, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(d, region);
    

    XMapWindow(d, overlay);
    XWindowAttributes gwa;
    XGetWindowAttributes(d, overlay, &gwa); 
    
    XImage *image = XGetImage(d, overlay, 0, 0 , gwa.width, gwa.height, AllPlanes, ZPixmap);
    GC gc = XCreateGC(d, overlay, 0, 0);
    
    
    cv::Mat img(gwa.height, gwa.width, CV_8UC4, image->data);
    cv::Mat vidFrm(hVid,wVid,CV_8UC3), vidFrmA(hVid,wVid,CV_8UC4);
    
    int alpha = parser.get<int>("alpha");
    int fps = std::max(1,(int)cap->get(cv::CAP_PROP_FPS));
    if(fps > parser.get<int>("fps"))
        fps = parser.get<int>("fps");
    printf("Use FPS: %d\n",fps);
    
    if(parser.get<bool>("ui")) {
        namedWindow("UI Overlay", cv::WINDOW_NORMAL);
        resizeWindow("UI Overlay",gwa.width/4, gwa.height/4);
        cv::createTrackbar("Alpha","UI Overlay", &alpha, 100, [] (int pos, void* userdata) {
            *((int*)userdata) = pos;
        },&alpha);
    }

    while(!quit) {

        if(!cap->read(vidFrm))
            break;
 
        cv::cvtColor(vidFrm,vidFrmA,cv::COLOR_BGR2BGRA);
        resize(vidFrmA, img, img.size(), cv::INTER_NEAREST); 
        if(parser.get<bool>("onlyAlphaChan")) {
            Mat bgra[4];
            cv::split(img,bgra);
            bgra[3] = bgra[3] * (float)alpha/100.0f; 
            cv::merge(bgra,4,img);
        } else
            img *= (float)alpha/100.0f;

        XPutImage(d, overlay, gc, image, 0, 0, 0, 0, gwa.width, gwa.height);

        if(parser.get<bool>("ui")) {
            imshow("UI Overlay", img);
        
            char c = (char)waitKey(1000/fps);  
            if ( c == 27 ) // ESC
                break;
            else if(c == 'r')
                XMoveWindow(d,overlay,wa.width - gwa.width,wa.height - gwa.height);
            else if(c == 'c')
                XMoveWindow(d,overlay,wa.width/4, wa.height/4);
            else if(c == 'f') {
                XResizeWindow(d,overlay,wa.width,wa.height);
                XGetWindowAttributes(d, overlay, &gwa); 
                image = XGetImage(d, overlay, 0, 0 , gwa.width, gwa.height, AllPlanes, ZPixmap);
                img = cv::Mat(gwa.height, gwa.width, CV_8UC4, image->data);
                XMoveWindow(d,overlay,0, 0);
            } else if(c == 's') {
                XResizeWindow(d,overlay,wOver,hOver);
                XGetWindowAttributes(d, overlay, &gwa); 
                image = XGetImage(d, overlay, 0, 0 , gwa.width, gwa.height, AllPlanes, ZPixmap);
                img = cv::Mat(gwa.height, gwa.width, CV_8UC4, image->data);
                XMoveWindow(d,overlay,wa.width/4, wa.height/4);
            }
        } else
            std::this_thread::sleep_for(std::chrono::milliseconds(1000/fps));
    }
    XDestroyImage(image);
    delete cap;
    return 0;
}

