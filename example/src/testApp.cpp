#include "testApp.h"
#include "ofConstants.h"

//--------------------------------------------------------------
void testApp::setup(){
	ofXml settings;
	missingSettings = !settings.load("settings.xml");
	if(!missingSettings){

		string server = settings.getValue("server");
		string user = settings.getValue("user");
		string pwd = settings.getValue("pwd");


		xmpp.setShow(ofxXMPPShowAvailable);
		xmpp.setStatus("connnected from ofxXMPP");
		xmpp.connect(server,user,pwd);

		ofAddListener(xmpp.newMessage,this,&testApp::onNewMessage);
	}

	selectedFriend = -1;

	ofBackground(255);
}

void testApp::onNewMessage(ofxXMPPMessage & msg){
	messages.push_back(msg);
	if(messages.size()>12) messages.pop_front();
}

//--------------------------------------------------------------
void testApp::update(){
}

//--------------------------------------------------------------
void testApp::draw(){
	ofSetColor(0);
	if(xmpp.getConnectionState()==ofxXMPPConnected){
		int i=0;
		for (;i<(int)messages.size();i++){
			ofDrawBitmapString(messages[i].from +": " + messages[i].body,20,20+i*20);
		}

		if(currentMessage!=""){
			ofDrawBitmapString("me: " + currentMessage, 20, 20 + i++ *20);
		}

		vector<ofxXMPPUser> friends = xmpp.getFriends();

		if(selectedFriend>=0 && selectedFriend<friends.size()){
			if(friends[selectedFriend].chatState==ofxXMPPChatStateComposing){
				ofDrawBitmapString(friends[selectedFriend].userName + ": ...", 20, 20 + i*20);
			}
		}

		for(int i=0;i<(int)friends.size();i++){
			ofSetColor(0);
			if(selectedFriend==i){
				ofSetColor(127);
				ofRect(ofGetWidth()-260,20+20*i-15,250,20);
				ofSetColor(255);
			}
			ofDrawBitmapString(friends[i].userName,ofGetWidth()-250,20+20*i);
			if(friends[i].show==ofxXMPPShowAvailable){
				ofSetColor(ofColor::green);
			}else{
				ofSetColor(ofColor::orange);
			}
			ofCircle(ofGetWidth()-270,20+20*i-5,3);
			//cout << friends[i].userName << endl;
			for(int j=0;j<friends[i].capabilities.size();j++){
				if(friends[i].capabilities[j]=="telekinect"){
					ofNoFill();
					ofCircle(ofGetWidth()-270,20+20*i-5,5);
					ofFill();
					break;
				}
			}
		}
	}else if(xmpp.getConnectionState()==ofxXMPPConnecting){
		ofDrawBitmapString("connecting...",ofGetWidth()/2-7*8,ofGetHeight()/2-4);
	}else if(missingSettings){
		ofDrawBitmapString("missing settings",ofGetWidth()/2-7*8,ofGetHeight()/2-4);
	}else{
		ofDrawBitmapString("disconnected",ofGetWidth()/2-7*8,ofGetHeight()/2-4);
	}
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
	if(key==OF_KEY_UP){
		selectedFriend--;
		selectedFriend %= xmpp.getFriends().size();
	}
	else if(key==OF_KEY_DOWN){
		selectedFriend++;
		selectedFriend %= xmpp.getFriends().size();
	}
	else if(key==OF_KEY_LEFT_CONTROL){
		xmpp.stop();
		ofXml settings;
		missingSettings = !settings.load("settings.xml");
		if(!missingSettings){
			string server = settings.getValue("server");
			string user = settings.getValue("user");
			string pwd = settings.getValue("pwd");
			xmpp.connect(server,user,pwd);
		}
	}
	else if(key==OF_KEY_CONTROL){

	}
	else if(key!=OF_KEY_RETURN){
		currentMessage += (char)key;
	}else{
		xmpp.sendMessage(xmpp.getFriends()[selectedFriend].userName,currentMessage);
		ofxXMPPMessage msg;
		msg.from = "me";
		msg.body = currentMessage;
		messages.push_back(msg);
		if(messages.size()>12) messages.pop_front();

		currentMessage = "";
	}
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 

}
