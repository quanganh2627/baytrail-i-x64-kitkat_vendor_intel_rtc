/*
 * libjingle
 * Copyright 2012, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#define PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/base/scoped_ptr.h"

#include "talk/app/webrtc/jsep.h"

namespace webrtc {
class VideoCaptureModule;
}  // namespace webrtc

namespace cricket {
class VideoRenderer;
}  // namespace cricket

class KXmppThread;
class GCallClient;

class Conductor
  : public webrtc::PeerConnectionObserver,
    public webrtc::CreateSessionDescriptionObserver {
//    public PeerConnectionClientObserver,
//    public MainWndCallback {
 public:
  enum CallbackID {
    MEDIA_CHANNELS_INITIALIZED = 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
    PEER_CONNECTION_ERROR,
    NEW_STREAM_ADDED,
    STREAM_REMOVED,
  };

  Conductor(KXmppThread *xmpp_thread, GCallClient *);
//  Conductor(GCallClient *client);

  void ConnectToPeer(const std::string &peer_name, bool video, bool audio);
  void OnInitiateMessage(bool video, bool audio);
  void AddIceCandidate(const webrtc::IceCandidateInterface* candidate);
  void ReceiveAccept(const cricket::SessionDescription* desc);
  void UpdateIceServers(const webrtc::JsepInterface::IceServers *iceservers);
  void ClearCandidates();

  void DisconnectFromCurrentPeer();
  bool connection_active() const;

  void SetCamera(int deviceId, std::string &deviceUniqueName);
  void SetImageOrientation(int degrees);
  bool SetVideo(bool enable);
  bool SetVoice(bool enable);

  virtual void Close();

 protected:
  ~Conductor();
  bool InitializePeerConnection(bool video, bool audio);
  void DeletePeerConnection();
  void EnsureStreamingUI();
  void AddStreams(bool video, bool audio);
  cricket::VideoCapturer* OpenVideoCaptureDevice();

  //
  // PeerConnectionObserver implementation.
  //
  virtual void OnError();
  virtual void OnStateChange(
      webrtc::PeerConnectionObserver::StateType state_changed) {}
  virtual void OnAddStream(webrtc::MediaStreamInterface* stream);
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream);
  virtual void OnRenegotiationNeeded() {}
  virtual void OnIceChange() {}
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);

  // CreateSessionDescriptionObserver implementation.
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
  virtual void OnFailure(const std::string& error);

 protected:
  // Send a message to the remote peer.
  void SendMessage(const std::string& json_object);

  std::string peer_name_;
  talk_base::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  cricket::VideoCapturer* capturer_;
  std::string camera_name_;
  int camera_id_;
  int imageOrientation_;

//  PeerConnectionClient* client_;
//  MainWindow* main_wnd_;
  KXmppThread *kxmpp_thread_;
  GCallClient *client_;
  std::deque<std::string*> pending_messages_;
  std::map<std::string, talk_base::scoped_refptr<webrtc::MediaStreamInterface> >
      active_streams_;
  std::string server_;
  const webrtc::JsepInterface::IceServers *iceservers_;
  std::vector<const webrtc::IceCandidateInterface*> candidate_queue_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
