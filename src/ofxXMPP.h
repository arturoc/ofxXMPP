/*
 * ofxXMPP.h
 *
 *  Created on: Aug 24, 2013
 *      Author: arturo
 */

#ifndef OFXXMPP_H_
#define OFXXMPP_H_

#include "ofThread.h"
#include "ofConstants.h"
#include "strophe.h"
#include <map>
#include <queue>
#include "ofEvents.h"


enum ofxXMPPChatState{
	ofxXMPPChatStateActive,
	ofxXMPPChatStateInactive,
	ofxXMPPChatStateGone,
	ofxXMPPChatStateComposing,
	ofxXMPPChatStatePaused
};

enum ofxXMPPShowState{
	ofxXMPPShowAvailable,
	ofxXMPPShowAway,
	ofxXMPPShowDnd,
	ofxXMPPShowXA
};

enum ofxXMPPConnectionState{
	ofxXMPPConnected,
	ofxXMPPConnecting,
	ofxXMPPDisconnected
};

enum ofxXMPPTerminateReason{
	ofxXMPPTerminateSuccess,
	ofxXMPPTerminateBusy,
	ofxXMPPTerminateDecline,
	ofxXMPPTerminateUnkown
};

struct ofxXMPPUser{
	string userName;
	string status;
	ofxXMPPShowState show;
	ofxXMPPChatState chatState;
	int priority;
	string resource;
	vector<string> capabilities;
};

struct ofxXMPPMessage{
	string from;
	string type;
	string body;
};

struct ofxXMPPPayload{
	int id;  //typically 96 - 127
	string name;
	int clockrate;
};

#ifndef USE_OFX_NICE
struct ofxICECandidate{
	int component;
	string foundation;
	int generation;
	string id;
	string ip;
	int network;
	int port;
	int priority;
	string protocol;
	string type;
};
#endif

struct ofxXMPPICETransport{
	string pwd;
	string ufrag;
	vector<ofxICECandidate> candidates;
};

struct ofxXMPPJingleContent{
	string name;
	string media;
	vector<ofxXMPPPayload> payloads;
	ofxXMPPICETransport transport;
};

struct ofxXMPPJingleInitiation{
	string from;
	string sid;
	vector<ofxXMPPJingleContent> contents;
};


struct ofxXMPPJingleFileInitiation{
	string fid;
	string from;
	string sid;
	string name;
	string date;
	string desc;
	size_t size;
	string hash;  // file hash as sha-1
	ofxXMPPICETransport transport;
};

struct ofxXMPPJingleHash{
	string sid;
	string fid;
	string from;
	string hash;
	string algo;
};

class ofxXMPP: public ofThread {
public:
	ofxXMPP();
	virtual ~ofxXMPP();

	void setShow(ofxXMPPShowState showState);
	void setStatus(const string & status);
	void setCapabilities(const string & capabilities);

	void connect(const string & host, const string & jid, const string & pass);
	void stop();

	void sendMessage(const string & to, const string & message);
	vector<ofxXMPPUser> getFriends();

	ofxXMPPConnectionState getConnectionState();

	string getBoundJID();

	/// general events
	ofEvent<ofxXMPPConnectionState> connectionStateChanged;
	ofEvent<ofxXMPPMessage> newMessage;
	ofEvent<ofxXMPPUser> userConnected;
	ofEvent<ofxXMPPUser> userDisconnected;

	/// jingle rtp events
	ofEvent<ofxXMPPJingleInitiation> jingleInitiationReceived;
	ofEvent<string> jingleInitiationACKd;
	ofEvent<ofxXMPPJingleInitiation> jingleInitiationAccepted;
	ofEvent<ofxXMPPTerminateReason> jingleTerminateReceived;
	ofEvent<string> jingleRing;

	/// jingle file transfer events
	ofEvent<ofxXMPPJingleFileInitiation> jingleFileInitiationReceived;
	ofEvent<ofxXMPPJingleFileInitiation> jingleFileInitiationAccepted;
	ofEvent<ofxXMPPJingleHash> hashReceived;
	ofEvent<ofxXMPPJingleHash> hashACKd;



	// RTP-ICE using Jingle  xmpp.org/extensions/xep-0167.html

	/// returns sid for the initiated session
	void initiateRTP(const string & to, ofxXMPPJingleInitiation & jingle);
	void ack(const ofxXMPPJingleInitiation & jingle);
	void ring(const ofxXMPPJingleInitiation & jingle);
	void ackRing(const string & to, const string & sid);
	void acceptRTPSession(const string & to, ofxXMPPJingleInitiation & jingle);
	void terminateRTPSession(ofxXMPPJingleInitiation & jingle, ofxXMPPTerminateReason reason);

	// File transfer based on Jingle file transfer but using libnice tcp over udp
	void initiateFileTransfer(const string & to, ofxXMPPJingleFileInitiation & jingle);
	void ack(const ofxXMPPJingleFileInitiation & jingle);
	void acceptFileTransfer(ofxXMPPJingleFileInitiation & jingle);
	void sendFileHash(const string & to, const ofxXMPPJingleHash & hash);
	void ack(const ofxXMPPJingleHash & hash);

	static string toString(ofxXMPPShowState showState);
	static ofxXMPPShowState fromString(string showState);


    enum JingleState{
    	Disconnected=0,
    	SessionAccepted,

    	//initiator
    	InitiatingRTP,
    	InitiationACKd,
    	WaitingSessionAccept,

    	// responder
    	GotInitiate,
    	Ringing,
    	RingACKd,
    	AcceptingRTP
    };

    /*enum JingleFileTransferState{
    	FileDisconnected=0,
    	FileSessionAccepted,

    	//initiator
    	FileInitiatingRTP,
    	FileInitiationACKd,
    	FileWaitingSessionAccept,

    	// responder
    	FileWaitingFile,
    	FileGotInitiate,
    	FileRinging,
    	FileRingACKd,
    	FileAcceptingRTP
    };*/

    JingleState getJingleState();


    static string LOG_NAME;

private:
	void threadedFunction();

	void sendPressence();

	void update(ofEventArgs & args);
	void fileInitiationReceived(xmpp_stanza_t * iq);
	void rtpInitiationReceived(xmpp_stanza_t * iq);
	void fileInitiationAccepted(xmpp_stanza_t * iq);
	void rtpInitiationAccepted(xmpp_stanza_t * iq);

	ofxXMPPJingleInitiation jingleInititationFromStanza(xmpp_stanza_t * jingle);
	ofxXMPPJingleFileInitiation jingleFileInititationFromStanza(xmpp_stanza_t * iq);

	xmpp_stanza_t * stanzaFromICETransport(const ofxXMPPICETransport & transport);

	static void conn_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status,
            const int error, xmpp_stream_error_t * const stream_error,
            void * const userdata);

	static int presence_handler(xmpp_conn_t * const conn,
				     xmpp_stanza_t * const stanza,
				     void * const userdata);

	static int message_handler(xmpp_conn_t * const conn,
				     xmpp_stanza_t * const stanza,
				     void * const userdata);

	static int iq_handler(xmpp_conn_t * const conn,
				     xmpp_stanza_t * const stanza,
				     void * const userdata);

    static xmpp_ctx_t *ctx;
    xmpp_conn_t *conn;

    ofxXMPPShowState currentShow;
    string currentStatus;
    string capabilities;

    map<string, ofxXMPPUser> friends;
    queue<ofxXMPPMessage> messageQueue;

    ofxXMPPConnectionState connectionState;
    string userName;

    JingleState jingleState;
    //JingleFileTransferState jingleFileTransferState;

    static string toString(JingleState state);
    //static string toString(JingleFileTransferState state);
    void addTextChild(xmpp_stanza_t * stanza, const string & textstr);

};

#endif /* OFXXMPP_H_ */
