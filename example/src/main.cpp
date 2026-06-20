#include "ofApp.h"
#include "ofMain.h"

int main() {
	ofGLWindowSettings settings;
	settings.setSize(1280, 720);
	settings.setGLVersion(3, 2);
	ofCreateWindow(settings);
	ofRunApp(new ofApp());
}
