#pragma once

#include "ofMain.h"
#include "ofxDawn.h"
#include "ofxDawnParticles.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;

	ofxDawn dawn;
	ofxDawnParticles particles;
};
