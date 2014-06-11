#pragma once

#include "ofMain.h"
#include "ofxXMPP.h"
#include <queue>

class testApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void onNewMessage(ofxXMPPMessage & msg);

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
		
		ofxXMPP xmpp;

		deque<ofxXMPPMessage> messages;
		string currentMessage;
		int selectedFriend;
		bool missingSettings;
};
