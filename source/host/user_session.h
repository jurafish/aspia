//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef HOST__USER_SESSION_H
#define HOST__USER_SESSION_H

#include "base/session_id.h"
#include "base/waitable_timer.h"
#include "base/ipc/ipc_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/user_list.h"
#include "base/win/session_status.h"
#include "host/client_session.h"
#include "host/desktop_session_manager.h"
#include "proto/host_internal.pb.h"

namespace host {

class UserSession
    : public base::IpcChannel::Listener,
      public DesktopSession::Delegate,
      public ClientSession::Delegate
{
public:
    enum class Type
    {
        CONSOLE,
        RDP
    };

    enum class State
    {
        STARTED,
        DETTACHED,
        FINISHED
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onUserSessionHostIdRequest(const std::string& session_name) = 0;
        virtual void onUserSessionCredentialsChanged() = 0;
        virtual void onUserSessionDettached() = 0;
        virtual void onUserSessionFinished() = 0;
    };

    UserSession(std::shared_ptr<base::TaskRunner> task_runner,
                base::SessionId session_id,
                std::unique_ptr<base::IpcChannel> channel);
    ~UserSession();

    void start(Delegate* delegate);
    void restart(std::unique_ptr<base::IpcChannel> channel);

    Type type() const { return type_; }
    State state() const { return state_; }
    base::SessionId sessionId() const { return session_id_; }
    base::HostId hostId() const { return host_id_; }
    std::string sessionName() const;
    base::User user() const;

    void addNewSession(std::unique_ptr<ClientSession> client_session);
    void setSessionEvent(base::win::SessionStatus status, base::SessionId session_id);
    void setRouterState(const proto::internal::RouterState& router_state);
    void setHostId(base::HostId host_id);

protected:
    // base::IpcChannel::Listener implementation.
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

    // DesktopSession::Delegate implementation.
    void onDesktopSessionStarted() override;
    void onDesktopSessionStopped() override;
    void onScreenCaptured(const base::Frame* frame, const base::MouseCursor* cursor) override;
    void onAudioCaptured(const proto::AudioPacket& audio_packet) override;
    void onScreenListChanged(const proto::ScreenList& list) override;
    void onClipboardEvent(const proto::ClipboardEvent& event) override;

    // ClientSession::Delegate implementation.
    void onClientSessionConfigured() override;
    void onClientSessionFinished() override;

private:
    void onSessionDettached(const base::Location& location);
    void sendConnectEvent(const ClientSession& client_session);
    void sendDisconnectEvent(uint32_t session_id);
    void updateCredentials();
    void sendCredentials();
    void killClientSession(uint32_t id);
    void sendRouterState();

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::IpcChannel> channel_;

    Type type_;
    State state_ = State::DETTACHED;
    base::WaitableTimer attach_timer_;

    base::SessionId session_id_;
    proto::internal::RouterState router_state_;
    base::HostId host_id_ = base::kInvalidHostId;
    std::string password_;

    using ClientSessionPtr = std::unique_ptr<ClientSession>;
    using ClientSessionList = std::vector<ClientSessionPtr>;

    ClientSessionList desktop_clients_;
    ClientSessionList file_transfer_clients_;

    std::unique_ptr<DesktopSessionManager> desktop_session_;
    std::shared_ptr<DesktopSessionProxy> desktop_session_proxy_;

    proto::internal::UiToService incoming_message_;
    proto::internal::ServiceToUi outgoing_message_;

    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UserSession);
};

} // namespace host

#endif // HOST__USER_SESSION_H
