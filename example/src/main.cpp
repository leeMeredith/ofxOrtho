#include "ofMain.h"
#include "ofApp.h"

int main()
{
    ofGLWindowSettings settings;
    settings.setSize(1100, 760);
    settings.setGLVersion(3, 2);
    ofCreateWindow(settings);

    ofRunApp(new ofApp());
}
