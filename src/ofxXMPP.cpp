/*
 * ofxXMPP.cpp
 *
 *  Created on: Aug 24, 2013
 *      Author: arturo
 */

#include "ofxXMPP.h"
#include "ofUtils.h"

xmpp_ctx_t *ofxXMPP::ctx=NULL;


string ofxXMPP::toString(JingleState state){
	switch(state){
	case Disconnected:
		return "Disconnected";
		//initiator
	case InitiatingRTP:
		return "InitiatingRTP";
	case InitiationACKd:
		return "InitiationACKd";
	case WaitingSessionAccept:
		return "WaitingSessionAccept";
	case SessionAccepted:
		return "SessionAccepted";
		// responder
	case GotInitiate:
		return "GotInitiate";
	case Ringing:
		return "Ringing";
	case RingACKd:
		return "RingACKd";
	case AcceptingRTP:
		return "AcceptingRTP";
	}
}

void ofxXMPP::conn_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status,
                  const int error, xmpp_stream_error_t * const stream_error,
                  void * const userdata)
{
	ofxXMPP * xmpp = (ofxXMPP*) userdata;

    if (status == XMPP_CONN_CONNECT) {
        fprintf(stderr, "DEBUG: connected\n");
        xmpp->sendPressence();
        xmpp->connectionState = ofxXMPPConnected;
    }
    else {
        fprintf(stderr, "DEBUG: disconnected\n");
        xmpp->stop();
        xmpp->connectionState = ofxXMPPDisconnected;
    }

    ofNotifyEvent(xmpp->connectionStateChanged,xmpp->connectionState,xmpp);
}

string getTextFromStanzasChild(const string & childName, xmpp_stanza_t * stanza){
	string text;
	xmpp_stanza_t * child = xmpp_stanza_get_child_by_name(stanza,childName.c_str());
	if(child){
		const char * child_text = xmpp_stanza_get_text(child);
		if(child_text)
			text = child_text;

		//xmpp_stanza_release(child);
	}
	return text;
}

int ofxXMPP::presence_handler(xmpp_conn_t * const conn,
			     xmpp_stanza_t * const stanza,
			     void * const userdata){

	ofxXMPP * xmpp = (ofxXMPP *)userdata;

	//cout << "presence from " << xmpp_stanza_get_attribute(stanza,"from") << endl;

	ofxXMPPUser user;
	string fullUserName = xmpp_stanza_get_attribute(stanza,"from");
	vector<string> nameParts = ofSplitString(xmpp_stanza_get_attribute(stanza,"from"),"/");
	if(!nameParts.empty())
		user.userName = nameParts[0];
	if(nameParts.size()>1)
		user.resource = nameParts[1];

	user.show = ofxXMPP::fromString(getTextFromStanzasChild("show",stanza));
	user.status = getTextFromStanzasChild("status",stanza);

	xmpp_stanza_t * caps = xmpp_stanza_get_child_by_name(stanza,"caps:c");

	if(caps){
		const char * capsExt = xmpp_stanza_get_attribute(caps,"ext");
		if(capsExt){
			user.capabilities = ofSplitString(capsExt," ");
		}
	}

	xmpp_stanza_t * priority = xmpp_stanza_get_child_by_name(stanza,"priority");
	if(priority){
		user.priority = ofToInt(xmpp_stanza_get_text(priority));
	}else{
		user.priority = 0;
	}

	map<string,ofxXMPPUser>::iterator existingUser=xmpp->friends.find(fullUserName);
	const char * presence_type = xmpp_stanza_get_type(stanza);
	if(presence_type && string(presence_type)=="unavailable" && existingUser!=xmpp->friends.end()){
		xmpp->friends.erase(existingUser);
		ofNotifyEvent(userDisconnected,user,this);
	}else{
		if(existingUser!=xmpp->friends.end()){
			user.chatState = existingUser->second.chatState;
			existingUser->second = user;
		}else{
			xmpp->friends[fullUserName] = user;
			ofNotifyEvent(userConnected,user,this);
		}
	}
	return 1;
}

int ofxXMPP::message_handler(xmpp_conn_t * const conn,
			     xmpp_stanza_t * const stanza,
			     void * const userdata){
	ofxXMPP * xmpp = (ofxXMPP *)userdata;

	ofxXMPPMessage message;
	message.from = xmpp_stanza_get_attribute(stanza,"from");
	message.type = xmpp_stanza_get_attribute(stanza,"type");
	message.body = getTextFromStanzasChild("body",stanza);
	if(message.body!="" && message.type!="error"){
		xmpp->lock();
		xmpp->messageQueue.push(message);
		xmpp->unlock();
	}else if(message.type=="error"){
		ofLogError() << "received error message" << message.body;
	}else if(message.body==""){
		xmpp->lock();
		map<string,ofxXMPPUser>::iterator it = xmpp->friends.find(message.from);
		if(it==xmpp->friends.end()){
			for(it=xmpp->friends.begin();it!=xmpp->friends.end();it++){
				if(it->second.userName==message.from){
					break;
				}
			}
		}
		if(it!=xmpp->friends.end()){
			ofxXMPPUser & user = it->second;
			if(xmpp_stanza_get_child_by_name(stanza,"cha:inactive")){
				user.chatState = ofxXMPPChatStateInactive;
			}else if(xmpp_stanza_get_child_by_name(stanza,"cha:active")){
				user.chatState = ofxXMPPChatStateActive;
			}else if(xmpp_stanza_get_child_by_name(stanza,"cha:gone")){
				user.chatState = ofxXMPPChatStateGone;
			}else if(xmpp_stanza_get_child_by_name(stanza,"cha:composing")){
				user.chatState = ofxXMPPChatStateComposing;
			}else if(xmpp_stanza_get_child_by_name(stanza,"cha:paused")){
				user.chatState = ofxXMPPChatStatePaused;
			}
		}
		xmpp->unlock();

	}

	return 1;
}


int ofxXMPP::iq_handler(xmpp_conn_t * const conn,
			     xmpp_stanza_t * const stanza,
			     void * const userdata){
	ofxXMPP * xmpp = (ofxXMPP *)userdata;

	const char * iq_type = xmpp_stanza_get_type(stanza);
	xmpp_stanza_t * jingle = xmpp_stanza_get_child_by_name(stanza,"jingle");

	if(iq_type && string(iq_type)=="error" && jingle){
		xmpp->jingleState = Disconnected;
	}else if(jingle){
		// jingle iq
		const char * jingle_action_cstr = xmpp_stanza_get_attribute(jingle,"action");
		string jingle_action;
		if(jingle_action_cstr) jingle_action = jingle_action_cstr;
		cout << xmpp_conn_get_bound_jid(xmpp->conn) << ": has jingle, state: " << xmpp->jingleState << "==" << Disconnected << " jingle_action: " << jingle_action << endl;

		if((jingle_action == "session-initiate" && xmpp->jingleState==Disconnected) || (jingle_action == "session-accept" && xmpp->jingleState==WaitingSessionAccept) ){
			cout << "session initiate" << endl;
			ofxXMPPJingleInitiation jingleInitiation;

			jingleInitiation.from = xmpp_stanza_get_attribute(stanza,"from");

			xmpp_stanza_t * content = xmpp_stanza_get_child_by_name(jingle,"content");
			while(content){
				ofxXMPPJingleContent xmppContent;

				// content description and payloads
				xmpp_stanza_t * description = xmpp_stanza_get_child_by_name(content,"description");
				if(description){
					xmppContent.media = xmpp_stanza_get_attribute(description,"media");
					cout << "xmpp " << "received content with media" << xmppContent.media << endl;
					xmpp_stanza_t * payload = xmpp_stanza_get_child_by_name(description,"payload");
					while(payload){
						ofxXMPPPayload xmppPayload;
						xmppPayload.id = ofToInt(xmpp_stanza_get_id(payload));
						xmppPayload.clockrate = ofToInt(xmpp_stanza_get_attribute(payload,"clockrate"));
						xmppPayload.name = xmpp_stanza_get_attribute(payload,"name");
						xmppContent.payloads.push_back(xmppPayload);

						payload = xmpp_stanza_get_next(payload);
					}
				}

				// content transport and candidates
				xmpp_stanza_t * transport = xmpp_stanza_get_child_by_name(content,"transport");
				if(transport){
					xmppContent.transport.pwd = xmpp_stanza_get_attribute(transport,"pwd");
					xmppContent.transport.ufrag = xmpp_stanza_get_attribute(transport,"ufrag");
					xmpp_stanza_t * candidate = xmpp_stanza_get_child_by_name(transport,"candidate");
					while(candidate){
						ofxICECandidate xmppCandidate;
						xmppCandidate.component = ofToInt(xmpp_stanza_get_attribute(candidate,"component"));
						xmppCandidate.foundation = xmpp_stanza_get_attribute(candidate,"foundation");
						xmppCandidate.generation = ofToInt(xmpp_stanza_get_attribute(candidate,"generation"));
						xmppCandidate.id = xmpp_stanza_get_id(candidate);
						xmppCandidate.ip = xmpp_stanza_get_attribute(candidate,"ip");
						xmppCandidate.network = ofToInt(xmpp_stanza_get_attribute(candidate,"network"));
						xmppCandidate.port = ofToInt(xmpp_stanza_get_attribute(candidate,"port"));
						xmppCandidate.priority = ofToInt(xmpp_stanza_get_attribute(candidate,"priority"));
						xmppCandidate.protocol = xmpp_stanza_get_attribute(candidate,"protocol");
						xmppCandidate.type = xmpp_stanza_get_type(candidate);
						xmppContent.transport.candidates.push_back(xmppCandidate);

						candidate = xmpp_stanza_get_next(candidate);
					}
				}

				jingleInitiation.contents.push_back(xmppContent);

				content = xmpp_stanza_get_next(content);
			}

			if(jingle_action=="session-initiate"){
				xmpp->jingleState = GotInitiate;
				ofNotifyEvent(xmpp->jingleInitiationReceived,jingleInitiation,xmpp);
			}else{
				xmpp->jingleState = SessionAccepted;
				ofNotifyEvent(xmpp->jingleInitiationAccepted,jingleInitiation,xmpp);
			}

		}else if(jingle_action =="session-info" && xmpp->jingleState==InitiationACKd){
			if(xmpp_stanza_get_child_by_name(jingle,"ringing")){
				xmpp->jingleState = WaitingSessionAccept;
				xmpp->ackRing(xmpp_stanza_get_attribute(stanza,"from"));
			}
		}
	}else{
		if( iq_type && string(iq_type)=="result" && xmpp->jingleState==InitiatingRTP){
			xmpp->jingleState = InitiationACKd;
		}else if( iq_type && string(iq_type)=="result" && xmpp->jingleState==AcceptingRTP){
			xmpp->jingleState = SessionAccepted;
		}
	}


	cout << xmpp_conn_get_bound_jid(xmpp->conn) << " to state " << toString(xmpp->jingleState) << endl;
	return 1;
}

ofxXMPP::ofxXMPP()
:conn(NULL)
,currentShow(ofxXMPPShowAvailable)
,connectionState(ofxXMPPDisconnected)
,jingleState(Disconnected)
{
	static bool initialized = false;
	if(!initialized){
		xmpp_initialize();

		//xmpp_log_t * log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG);
		xmpp_log_t * log = NULL;
	    ctx = xmpp_ctx_new(NULL, log);

		startThread();
		initialized = true;
	}
}

ofxXMPP::~ofxXMPP() {
	//xmpp_shutdown();
}

string ofxXMPP::toString(ofxXMPPShowState showState){
	switch(showState){
	case ofxXMPPShowAway:
		return "away";
	case ofxXMPPShowDnd:
		return "dnd";
	case ofxXMPPShowXA:
		return "xa";
	default:
		return "available";
	}
}


ofxXMPPShowState ofxXMPP::fromString(string showState){
	if(showState=="away") return ofxXMPPShowAway;
	if(showState=="dnd") return ofxXMPPShowDnd;
	if(showState=="xa") return ofxXMPPShowXA;
	return ofxXMPPShowAvailable;
}

void ofxXMPP::setShow(ofxXMPPShowState showState){
	currentShow = showState;
	if(conn){
		sendPressence();
	}
}

void ofxXMPP::setStatus(const string & status){
	currentStatus = status;
	if(conn){
		sendPressence();
	}
}

void ofxXMPP::setCapabilities(const string & capabilities){
	this->capabilities = capabilities;
	if(conn){
		sendPressence();
	}
}

void ofxXMPP::sendMessage(const string & to, const string & message){
	xmpp_stanza_t * msg = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(msg,"message");
	xmpp_stanza_set_attribute(msg,"type","chat");
	xmpp_stanza_set_attribute(msg,"to",to.c_str());

	xmpp_stanza_t * msg_body = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(msg_body,"body");

	xmpp_stanza_t * msg_body_text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(msg_body_text,message.c_str());

	xmpp_stanza_add_child(msg_body,msg_body_text);
	xmpp_stanza_add_child(msg,msg_body);

	xmpp_send(conn, msg);
	xmpp_stanza_release(msg_body_text);
	xmpp_stanza_release(msg_body);
	xmpp_stanza_release(msg);
}

void ofxXMPP::sendPressence(){
	xmpp_stanza_t* pres = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(pres, "presence");

	xmpp_stanza_t* show = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(show,"show");

	xmpp_stanza_t* show_text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(show_text, toString(currentShow).c_str());
	xmpp_stanza_add_child(show,show_text);

	xmpp_stanza_add_child(pres,show);


	xmpp_stanza_t* status = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(status,"status");

	xmpp_stanza_t* status_text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(status_text, currentStatus.c_str());
	xmpp_stanza_add_child(status,status_text);

	xmpp_stanza_add_child(pres,status);

	//<caps:c xmlns:caps="http://jabber.org/protocol/caps" ext="pmuc-v1 sms-v1 camera-v1 video-v1 voice-v1" ver="1.1" node="http://mail.google.com/xmpp/client/caps"/>

	xmpp_stanza_t * capabilities = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(capabilities,"caps:c");
	xmpp_stanza_set_attribute(capabilities,"xmlns:caps","http://jabber.org/protocol/caps");
	xmpp_stanza_set_attribute(capabilities,"ext",this->capabilities.c_str());
	xmpp_stanza_set_attribute(capabilities,"ver","1.1");
	xmpp_stanza_add_child(pres,capabilities);

	/*xmpp_stanza_t * priority = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(priority,"priority");

	xmpp_stanza_t * priority_text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(priority_text,"-10");
	xmpp_stanza_add_child(priority,priority_text);
	xmpp_stanza_add_child(pres,priority);*/

	xmpp_send(conn, pres);
	/*xmpp_stanza_release(priority_text);
	xmpp_stanza_release(priority);*/
	xmpp_stanza_release(capabilities);
	xmpp_stanza_release(show);
	xmpp_stanza_release(show_text);
	xmpp_stanza_release(status_text);
	xmpp_stanza_release(status);
	xmpp_stanza_release(pres);
}

void ofxXMPP::connect(const string & host, const string & jid, const string & pass){

    conn = xmpp_conn_new(ctx);

    xmpp_conn_set_jid(conn, jid.c_str());
    xmpp_conn_set_pass(conn, pass.c_str());

    xmpp_handler_add(conn,
    		      presence_handler,
    		      NULL,
    		      "presence",
    		      NULL,
    		      this);

    xmpp_handler_add(conn,
    		      message_handler,
    		      NULL,
    		      "message",
    		      NULL,
    		      this);

    xmpp_handler_add(conn,
    				iq_handler,
    				NULL,
    				"iq",
    				NULL,
    				this);

    xmpp_connect_client(conn, host.c_str(), 0, conn_handler, this);

    connectionState = ofxXMPPConnecting;
    userName = jid;

    ofAddListener(ofEvents().update,this,&ofxXMPP::update);

}

vector<ofxXMPPUser> ofxXMPP::getFriends(){
	vector<ofxXMPPUser> friendsVector;
	lock();
	for(map<string,ofxXMPPUser>::iterator it=friends.begin();it!=friends.end();it++){
		friendsVector.push_back(it->second);
	}
	unlock();
	return friendsVector;
}

void ofxXMPP::update(ofEventArgs & args){
	lock();
	queue<ofxXMPPMessage> queueCopy = messageQueue;
	while(!messageQueue.empty()){
		messageQueue.pop();
	}
	unlock();

	while(!queueCopy.empty()){
		ofNotifyEvent(newMessage,queueCopy.front(),this);
		queueCopy.pop();
	}
}

ofxXMPPConnectionState ofxXMPP::getConnectionState(){
	return connectionState;
}

string ofxXMPP::getBoundJID(){
	return xmpp_conn_get_bound_jid(conn);
}

void ofxXMPP::initiateRTP(const string & to, ofxXMPPJingleInitiation & jingleInitiation){
	xmpp_stanza_t * iq = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(iq,"iq");
	xmpp_stanza_set_attribute(iq,"to",to.c_str());
	xmpp_stanza_set_type(iq,"set");

	xmpp_stanza_t * jingle = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(jingle,"jingle");
	xmpp_stanza_set_ns(jingle,"urn:xmpp:jingle:1");
	xmpp_stanza_set_attribute(jingle,"action","session-initiate");
	xmpp_stanza_set_attribute(jingle,"initiator",xmpp_conn_get_bound_jid(conn));
	//xmpp_stanza_set_attribute(jingle,"sid",...);

	for(size_t i = 0; i<jingleInitiation.contents.size(); i++){
		const ofxXMPPJingleContent  & xmppContent = jingleInitiation.contents[i];
		xmpp_stanza_t * content = xmpp_stanza_new(ctx);
		xmpp_stanza_set_name(content,"content");
		xmpp_stanza_set_attribute(content,"creator","initiator");
		xmpp_stanza_set_attribute(content,"name",xmppContent.name.c_str());

		xmpp_stanza_t * description = xmpp_stanza_new(ctx);
		xmpp_stanza_set_name(description,"description");
		xmpp_stanza_set_ns(description,"urn:xmpp:jingle:apps:rtp:1");
		xmpp_stanza_set_attribute(description,"media",xmppContent.media.c_str());

		xmpp_stanza_add_child(content,description);
		xmpp_stanza_release(description);

		for(size_t p=0;p<xmppContent.payloads.size();p++){
			const ofxXMPPPayload & xmppPayload = xmppContent.payloads[p];

			xmpp_stanza_t * payload = xmpp_stanza_new(ctx);
			xmpp_stanza_set_name(payload,"payload");
			xmpp_stanza_set_id(payload,ofToString(xmppPayload.id).c_str());
			xmpp_stanza_set_attribute(payload,"clockrate",ofToString(xmppPayload.clockrate).c_str());
			xmpp_stanza_set_attribute(payload,"name",xmppPayload.name.c_str());

			xmpp_stanza_add_child(content,payload);
			xmpp_stanza_release(payload);
		}

		xmpp_stanza_t * transport = xmpp_stanza_new(ctx);
		xmpp_stanza_set_name(transport,"transport");
		xmpp_stanza_set_ns(transport,"urn:xmpp:jingle:transports:ice-udp:1");
		xmpp_stanza_set_attribute(transport,"pwd",xmppContent.transport.pwd.c_str());
		xmpp_stanza_set_attribute(transport,"ufrag",xmppContent.transport.ufrag.c_str());

		for(size_t c=0;c<xmppContent.transport.candidates.size();c++){
			const ofxICECandidate & xmppCandidate = xmppContent.transport.candidates[c];
			xmpp_stanza_t * candidate = xmpp_stanza_new(ctx);
			xmpp_stanza_set_name(candidate,"candidate");
			xmpp_stanza_set_attribute(candidate,"component",ofToString(xmppCandidate.component).c_str());
			xmpp_stanza_set_attribute(candidate,"foundation",xmppCandidate.foundation.c_str());
			xmpp_stanza_set_attribute(candidate,"generation",ofToString(xmppCandidate.generation).c_str());
			xmpp_stanza_set_id(candidate,xmppCandidate.id.c_str());
			xmpp_stanza_set_attribute(candidate,"ip",xmppCandidate.ip.c_str());
			xmpp_stanza_set_attribute(candidate,"network",ofToString(xmppCandidate.network).c_str());
			xmpp_stanza_set_attribute(candidate,"port",ofToString(xmppCandidate.port).c_str());
			xmpp_stanza_set_attribute(candidate,"priority",ofToString(xmppCandidate.priority).c_str());
			xmpp_stanza_set_attribute(candidate,"protocol",xmppCandidate.protocol.c_str());
			xmpp_stanza_set_type(candidate,xmppCandidate.type.c_str());

			xmpp_stanza_add_child(transport,candidate);
			xmpp_stanza_release(candidate);
		}

		xmpp_stanza_add_child(content,transport);
		xmpp_stanza_release(transport);

		xmpp_stanza_add_child(jingle,content);
		xmpp_stanza_release(content);
	}

	xmpp_stanza_add_child(iq,jingle);
	xmpp_stanza_release(jingle);

	jingleState = InitiatingRTP;

	xmpp_send(conn,iq);
	xmpp_stanza_release(iq);
}

void ofxXMPP::ack(const ofxXMPPJingleInitiation & jingle){
	/*<iq from='juliet@capulet.lit/balcony'
	    id='sf93gv76'
	    to='romeo@montague.lit/orchard'
	    type='result'/>*/

	xmpp_stanza_t * iq = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(iq,"iq");
	xmpp_stanza_set_attribute(iq,"to",jingle.from.c_str());
	xmpp_stanza_set_type(iq,"result");
	xmpp_send(conn,iq);
	xmpp_stanza_release(iq);

}

void ofxXMPP::ring(const ofxXMPPJingleInitiation & xmppJingle){
	/*<iq from='juliet@capulet.lit/balcony'
		id='nf91g647'
		to='romeo@montague.lit/orchard'
		type='set'>
	  <jingle xmlns='urn:xmpp:jingle:1'
			  action='session-info'
			  initiator='romeo@montague.lit/orchard'
			  sid='a73sjjvkla37jfea'>
		<ringing xmlns='urn:xmpp:jingle:apps:rtp:info:1'/>
	  </jingle>
	</iq>*/

	xmpp_stanza_t * iq = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(iq,"iq");
	xmpp_stanza_set_attribute(iq,"to",xmppJingle.from.c_str());
	xmpp_stanza_set_type(iq,"set");

	xmpp_stanza_t * jingle = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(jingle,"jingle");
	xmpp_stanza_set_ns(jingle,"urn:xmpp:jingle:1");
	xmpp_stanza_set_attribute(jingle,"action","session-info");
	xmpp_stanza_set_attribute(jingle,"initiator",xmppJingle.from.c_str());

	xmpp_stanza_t * ringing = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(ringing,"ringing");
	xmpp_stanza_set_ns(ringing,"urn:xmpp:jingle:apps:rtp:info:1");

	xmpp_stanza_add_child(jingle,ringing);
	xmpp_stanza_add_child(iq,jingle);

	xmpp_send(conn,iq);

	xmpp_stanza_release(ringing);
	xmpp_stanza_release(jingle);
	xmpp_stanza_release(iq);
}

void ofxXMPP::ackRing(const string & to){
	xmpp_stanza_t * iq = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(iq,"iq");
	xmpp_stanza_set_attribute(iq,"to",to.c_str());
	xmpp_stanza_set_type(iq,"result");
	xmpp_send(conn,iq);
	xmpp_stanza_release(iq);
}

void ofxXMPP::stop(){
	xmpp_stop(ctx);
	xmpp_conn_release(conn);
	xmpp_ctx_free(ctx);
	ctx = NULL;
	conn = NULL;
}

void ofxXMPP::threadedFunction(){
	xmpp_run(ctx);
}

void ofxXMPP::acceptRTPSession(const string & to, ofxXMPPJingleInitiation & jingleInitiation){
	xmpp_stanza_t * iq = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(iq,"iq");
	xmpp_stanza_set_attribute(iq,"to",to.c_str());
	xmpp_stanza_set_type(iq,"set");

	xmpp_stanza_t * jingle = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(jingle,"jingle");
	xmpp_stanza_set_ns(jingle,"urn:xmpp:jingle:1");
	xmpp_stanza_set_attribute(jingle,"action","session-accept");
	xmpp_stanza_set_attribute(jingle,"initiator",to.c_str());
	xmpp_stanza_set_attribute(jingle,"responder",xmpp_conn_get_bound_jid(conn));
	//xmpp_stanza_set_attribute(jingle,"sid",...);

	for(size_t i = 0; i<jingleInitiation.contents.size(); i++){
		const ofxXMPPJingleContent  & xmppContent = jingleInitiation.contents[i];
		xmpp_stanza_t * content = xmpp_stanza_new(ctx);
		xmpp_stanza_set_name(content,"content");
		xmpp_stanza_set_attribute(content,"creator","initiator");
		xmpp_stanza_set_attribute(content,"name",xmppContent.name.c_str());

		xmpp_stanza_t * description = xmpp_stanza_new(ctx);
		xmpp_stanza_set_name(description,"description");
		xmpp_stanza_set_ns(description,"urn:xmpp:jingle:apps:rtp:1");
		xmpp_stanza_set_attribute(description,"media",xmppContent.media.c_str());

		xmpp_stanza_add_child(content,description);
		xmpp_stanza_release(description);

		for(size_t p=0;p<xmppContent.payloads.size();p++){
			const ofxXMPPPayload & xmppPayload = xmppContent.payloads[p];

			xmpp_stanza_t * payload = xmpp_stanza_new(ctx);
			xmpp_stanza_set_name(payload,"payload");
			xmpp_stanza_set_id(payload,ofToString(xmppPayload.id).c_str());
			xmpp_stanza_set_attribute(payload,"clockrate",ofToString(xmppPayload.clockrate).c_str());
			xmpp_stanza_set_attribute(payload,"name",xmppPayload.name.c_str());

			xmpp_stanza_add_child(content,payload);
			xmpp_stanza_release(payload);
		}

		xmpp_stanza_t * transport = xmpp_stanza_new(ctx);
		xmpp_stanza_set_name(transport,"transport");
		xmpp_stanza_set_ns(transport,"urn:xmpp:jingle:transports:ice-udp:1");
		xmpp_stanza_set_attribute(transport,"pwd",xmppContent.transport.pwd.c_str());
		xmpp_stanza_set_attribute(transport,"ufrag",xmppContent.transport.ufrag.c_str());

		for(size_t c=0;c<xmppContent.transport.candidates.size();c++){
			const ofxICECandidate & xmppCandidate = xmppContent.transport.candidates[c];
			xmpp_stanza_t * candidate = xmpp_stanza_new(ctx);
			xmpp_stanza_set_name(candidate,"candidate");
			xmpp_stanza_set_attribute(candidate,"component",ofToString(xmppCandidate.component).c_str());
			xmpp_stanza_set_attribute(candidate,"foundation",xmppCandidate.foundation.c_str());
			xmpp_stanza_set_attribute(candidate,"generation",ofToString(xmppCandidate.generation).c_str());
			xmpp_stanza_set_id(candidate,xmppCandidate.id.c_str());
			xmpp_stanza_set_attribute(candidate,"ip",xmppCandidate.ip.c_str());
			xmpp_stanza_set_attribute(candidate,"network",ofToString(xmppCandidate.network).c_str());
			xmpp_stanza_set_attribute(candidate,"port",ofToString(xmppCandidate.port).c_str());
			xmpp_stanza_set_attribute(candidate,"priority",ofToString(xmppCandidate.priority).c_str());
			xmpp_stanza_set_attribute(candidate,"protocol",xmppCandidate.protocol.c_str());
			xmpp_stanza_set_type(candidate,xmppCandidate.type.c_str());

			xmpp_stanza_add_child(transport,candidate);
			xmpp_stanza_release(candidate);
		}

		xmpp_stanza_add_child(content,transport);
		xmpp_stanza_release(transport);

		xmpp_stanza_add_child(jingle,content);
		xmpp_stanza_release(content);
	}

	xmpp_stanza_add_child(iq,jingle);
	xmpp_stanza_release(jingle);

	jingleState = AcceptingRTP;

	xmpp_send(conn,iq);
	xmpp_stanza_release(iq);
}

ofxXMPP::JingleState ofxXMPP::getJingleState(){
	return jingleState;
}
