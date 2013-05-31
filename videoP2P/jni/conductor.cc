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

#include "conductor.h"

#include <utility>

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "defaults.h"
#include "talk/media/devices/devicemanager.h"
#include "kxmppthread.h"
#include "callclient.h"
#include "gsession.h"
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"
#include "vie_base.h"

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static DummySetSessionDescriptionObserver* Create() {
    return
        new talk_base::RefCountedObject<DummySetSessionDescriptionObserver>();
  }
  virtual void OnSuccess() {
    LOG(INFO) << __FUNCTION__;
  }
  virtual void OnFailure(const std::string& error) {
    LOG(INFO) << __FUNCTION__ << " " << error;
  }

 protected:
  DummySetSessionDescriptionObserver() {}
  ~DummySetSessionDescriptionObserver() {}
};

Conductor::Conductor(KXmppThread *kxmpp_thread, GCallClient *client)
  : peer_name_(""),
    kxmpp_thread_(kxmpp_thread),
    capturer_(NULL),
    client_(client),
    iceservers_(NULL),
    imageOrientation_(0) {
  LOG(INFO) << "Conductor::Conductor ctor";
}

Conductor::~Conductor() {
  LOG(INFO) << "Conductor::~Conductor dtor";
  ASSERT(peer_connection_.get() == NULL);
  // delete the iceservers received from callclient
  if (iceservers_)
    delete iceservers_;

  // delete camera device
  if (capturer_) {
    delete capturer_;
    capturer_ = NULL;
  }
}

bool Conductor::connection_active() const {
  return peer_connection_.get() != NULL;
}

void Conductor::SetCamera(int deviceId, std::string &deviceUniqueName) {
  camera_id_ = deviceId;
  camera_name_ = deviceUniqueName;
}

void Conductor::SetImageOrientation(int degrees)
{
  imageOrientation_ = degrees;
}

bool Conductor::SetVideo(bool enable) {
  std::map<std::string, talk_base::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it
      = active_streams_.find(kStreamLabel);

  if(it == active_streams_.end()) {
      LOG(LS_ERROR) << "Stream list is empty.";
      return false;
  }

  talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream = it->second;

  stream->AddRef();

  webrtc::VideoTrackVector tracks = stream->GetVideoTracks();
  if (!tracks.empty()) {
      webrtc::VideoTrackInterface* track = tracks[0];
      track->set_enabled(enable);
  }

  stream->Release();
  return true;
}

bool Conductor::SetVoice(bool enable) {
  std::map<std::string, talk_base::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it
      = active_streams_.find(kStreamLabel);

  if(it == active_streams_.end()) {
    LOG(LS_ERROR) << "Stream list is empty.";
    return false;
  }

  talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream = it->second;

  stream->AddRef();

  webrtc::AudioTrackVector tracks = stream->GetAudioTracks();
  if (!tracks.empty()) {
    webrtc::AudioTrackInterface* track = tracks[0];
    track->set_enabled(enable);
  }

  stream->Release();
  return true;
}

void Conductor::Close() {
  DeletePeerConnection();
}

bool Conductor::InitializePeerConnection(bool video, bool audio) {
  LOG(INFO) << "Conductor::InitializePeerConnection";
  ASSERT(peer_connection_factory_.get() == NULL);
  ASSERT(peer_connection_.get() == NULL);

  peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();
#if 1 // enable NativeWindow
  webrtc::VideoEngine::SetRemoteSurface(GetRemoteSurface());
#endif

  if (!peer_connection_factory_.get()) {
    LOG(LS_ERROR) << "Failed to initialize PeerConnectionFactory";
    DeletePeerConnection();
    return false;
  }

  //IceServerRequestHandler icereq(kxmpp_thread_->client());
  if(iceservers_ == NULL) {
    webrtc::PeerConnectionInterface::IceServers servers;
    webrtc::PeerConnectionInterface::IceServer server;

    server.uri = GetPeerConnectionString();
    servers.push_back(server);
    peer_connection_ = peer_connection_factory_->CreatePeerConnection(servers,
                                                                    NULL,
                                                                    this);
  } else {
    webrtc::JsepInterface::IceServers::const_iterator iter;
    for(iter = iceservers_->begin(); iter != iceservers_->end(); iter++) {
      LOG(INFO) << "IceServer: " << iter->uri;
    }

    peer_connection_ = peer_connection_factory_->CreatePeerConnection(*iceservers_,
                                                                    NULL,
                                                                    this);
  }

  if (!peer_connection_.get()) {
    LOG(LS_ERROR) << "CreatePeerConnection failed";
    DeletePeerConnection();
    return false;
  }
  AddStreams(video, audio);
  return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection() {
  peer_connection_ = NULL;
  active_streams_.clear();
  client_->StopLocalRenderer();
  client_->StopRemoteRenderer();
  peer_connection_factory_ = NULL;
  peer_name_ = "";
  if (capturer_) {
    delete capturer_;
    capturer_ = NULL;
  }
}

//
// PeerConnectionObserver implementation.
//

void Conductor::OnError() {
  LOG(LS_ERROR) << __FUNCTION__;
  // TODO: (khanh) report error to UI
}

// Called when a remote stream is added
void Conductor::OnAddStream(webrtc::MediaStreamInterface* stream) {
  LOG(INFO) << __FUNCTION__ << " " << stream->label();

  stream->AddRef();

  webrtc::VideoTrackVector tracks = stream->GetVideoTracks();
  if (!tracks.empty()) {
    webrtc::VideoTrackInterface* track = tracks[0];
    client_->StartRemoteRenderer(track);
  }

  stream->Release();
}

void Conductor::OnRemoveStream(webrtc::MediaStreamInterface* stream) {
  LOG(INFO) << __FUNCTION__ << " " << stream->label();
  stream->AddRef();
  webrtc::VideoTrackVector tracks = stream->GetVideoTracks();
  if (!tracks.empty()) {
    webrtc::VideoTrackInterface* track = tracks[0];
    client_->StopRemoteRenderer();
  }
  stream->Release();
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();
  LOG(INFO) << "candidate->sdp_mid:" << candidate->sdp_mid() << std::endl;
  std::string str;
  candidate->ToString(&str);
  LOG(INFO) << "candidate:" << str;
  kxmpp_thread_->SendIceCandidate(candidate);
}

void Conductor::ConnectToPeer(const std::string &peer_name, bool video, bool audio) {
  LOG(INFO) << "Conductor::ConnectToPeer name=" << peer_name;
#if 0 // TODO: (khanh) validate peer_name
  ASSERT(peer_name_ == "");
  ASSERT(peer_name != "");
#endif

  if (peer_connection_.get()) {
    LOG(LS_ERROR) << "Already connected";
    return;
  }

  if (InitializePeerConnection(video, audio)) {
    peer_name_ = peer_name;
    peer_connection_->CreateOffer(this, NULL);
  } else {
    LOG(LS_ERROR) << "Failed to initialize PeerConnection";
  }
}

void Conductor::OnInitiateMessage(bool video, bool audio)
{
  LOG(INFO) << "Conductor::OnInitiateMessage";
  cricket::GSession* session = client_->GetSession();
  if (peer_connection_.get()) {
    LOG(LS_ERROR) << "Conductor::OnInitiateMessage peer_connection_ already exist";
  }

  if (!InitializePeerConnection(video, audio)) {
    LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
    return;
  }

  webrtc::SessionDescriptionInterface* session_description = webrtc::CreateSessionDescription("offer", session->remote_description()->Copy());
  if (!session_description) {
    LOG(LS_ERROR) << "Failed to create session_description";
    return;
  }
  peer_connection_->SetRemoteDescription(
      DummySetSessionDescriptionObserver::Create(), session_description);
  peer_connection_->CreateAnswer(this, NULL);
}

cricket::VideoCapturer* Conductor::OpenVideoCaptureDevice() {
  LOG(INFO) << "OpenVideoCaptureDevice";
  talk_base::scoped_ptr<cricket::DeviceManagerInterface> dev_manager(
      cricket::DeviceManagerFactory::Create());
  if (!dev_manager->Init()) {
    LOG(LS_ERROR) << "Can't create device manager";
    return NULL;
  }
  cricket::Device dev(camera_name_, camera_id_);

  cricket::VideoCapturer* capturer = dev_manager->CreateVideoCapturer(dev);

  //LOG(LS_INFO) << "Conductor::OpenVideoCaptureDevice() imageOrientation_=" << imageOrientation_;
  capturer->SetCaptureRotation(imageOrientation_);
  capturer_ = capturer;
  return capturer;
}

void Conductor::AddStreams(bool video, bool audio) {
  if (active_streams_.find(kStreamLabel) != active_streams_.end())
    return;  // Already added.

  talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream =
      peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);

  if(audio) {
    LOG(LS_INFO) << "Adding audio track";
    talk_base::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      peer_connection_factory_->CreateAudioTrack(
          kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL)));
    stream->AddTrack(audio_track);
  }

  if(video) {
    LOG(LS_INFO) << "Adding video track";
    talk_base::scoped_refptr<webrtc::VideoTrackInterface> video_track(
      peer_connection_factory_->CreateVideoTrack(
          kVideoLabel,
          peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(),
                                                      NULL)));
    client_->StartLocalRenderer(video_track);
    stream->AddTrack(video_track);
  }

  if (!peer_connection_->AddStream(stream, NULL)) {
    LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
  }
  typedef std::pair<std::string,
                    talk_base::scoped_refptr<webrtc::MediaStreamInterface> >
      MediaStreamPair;
  active_streams_.insert(MediaStreamPair(stream->label(), stream));
}

// Call from PeerConnection to return SDP from CreateOffer and CreateAnswer
void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {

  std::string sdp;
  desc->ToString(&sdp);
  LOG(INFO) << "Conductor::OnSuccess SDP message:\n" << sdp;

  peer_connection_->SetLocalDescription(
      DummySetSessionDescriptionObserver::Create(), desc);
  if (client_->IsIncomingCall()) {
    LOG(INFO) << "Conductor::OnSuccess SendAccept";
    kxmpp_thread_->SendAccept(desc->description()->Copy());
    while(!candidate_queue_.empty()) {
        LOG(INFO) << "Conductor::OnSuccess add queued candidate back";
        const webrtc::IceCandidateInterface* c = candidate_queue_.back();
        candidate_queue_.pop_back();
        AddIceCandidate(c);
    }
  } else {
    LOG(INFO) << "Conductor::OnSuccess SendOffer";
    kxmpp_thread_->SendOffer(peer_name_, desc->description()->Copy());
  }
}

void Conductor::OnFailure(const std::string& error) {
    LOG(LERROR) << error;
    // TODO: report error to UI
}

void Conductor::AddIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
  LOG(INFO) << "Conductor::AddIceCandidate";
  if (peer_connection_.get()) {
    peer_connection_->AddIceCandidate(candidate);
    delete candidate;
  } else {
    LOG(INFO) << "Conductor::AddIceCandidate failed, enqueue for now";
    candidate_queue_.push_back(candidate);
  }
}

void Conductor::ReceiveAccept(const cricket::SessionDescription* desc) {
  LOG(INFO) << "Conductor::ReceiveAccept, calling SetRemoteDescription";
  webrtc::SessionDescriptionInterface* session_description = webrtc::CreateSessionDescription("answer", const_cast<cricket::SessionDescription*>(desc->Copy())); 
  if (!session_description) {
    LOG(LS_ERROR) << "Failed to create session_description";
    return;
  }
  peer_connection_->SetRemoteDescription(
      DummySetSessionDescriptionObserver::Create(), session_description);
}

void Conductor::UpdateIceServers(const webrtc::JsepInterface::IceServers *iceservers)
{
  if(iceservers_ != NULL)
    delete iceservers_;

  iceservers_ = iceservers;
}

void Conductor::ClearCandidates() {
    LOG(INFO) << "Conductor::ClearCandidates";
    candidate_queue_.clear();
}
