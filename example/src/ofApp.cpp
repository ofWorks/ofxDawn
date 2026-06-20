#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxDawn - WebGPU particles");
	ofBackground(0);
	ofSetVerticalSync(true);

	if (!dawn.setup()) {
		ofLogError("ofApp") << "WebGPU setup failed";
		return;
	}
	particles.setup(dawn, 200000, glm::vec2(ofGetWidth(), ofGetHeight()));
}

void ofApp::update() {
	// Particles chase the mouse; when it leaves the window they drift toward
	// the center.
	glm::vec2 attractor(ofGetMouseX(), ofGetMouseY());
	if (!ofGetWindowRect().inside(attractor)) {
		attractor = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f);
	}
	particles.update(ofGetLastFrameTime(), attractor);
}

void ofApp::draw() {
	ofEnableBlendMode(OF_BLENDMODE_ADD);
	ofSetColor(80, 160, 255, 255);
	particles.draw();
	ofDisableBlendMode();

	ofSetColor(255);
	ofDrawBitmapStringHighlight(
		"ofxDawn - " + ofToString(particles.getCount()) + " GPU particles\n"
		"fps: " + ofToString(ofGetFrameRate(), 1),
		20, 30);
}
