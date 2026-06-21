#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxDawn - WebGPU particles (zero-copy)");
	ofBackground(0);
	ofSetVerticalSync(true);

	if (!dawn.setup()) {
		ofLogError("ofApp") << "WebGPU setup failed";
		return;
	}

	// The particles are simulated and rendered on the GPU straight into this
	// shared IOSurface texture, then drawn by OF with no pixel copy.
	sharedTex.setup(dawn, ofGetWidth(), ofGetHeight());
	particles.setup(dawn, 200000, glm::vec2(ofGetWidth(), ofGetHeight()));
}

void ofApp::update() {
	glm::vec2 attractor(ofGetMouseX(), ofGetMouseY());
	if (!ofGetWindowRect().inside(attractor)) {
		attractor = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f);
	}
	particles.update(ofGetLastFrameTime(), attractor, sharedTex);
}

void ofApp::draw() {
	if (sharedTex.isSetup()) {
		sharedTex.getTexture().draw(0, 0);
	}

	ofSetColor(255);
	ofDrawBitmapStringHighlight(
		"ofxDawn - " + ofToString(particles.getCount()) + " GPU particles (zero-copy IOSurface)\n"
		"fps: " + ofToString(ofGetFrameRate(), 1),
		20, 30);
}
