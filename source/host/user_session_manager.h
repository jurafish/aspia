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

#ifndef HOST__USER_SESSION_MANAGER_H
#define HOST__USER_SESSION_MANAGER_H

#include "base/session_id.h"
#include "base/ipc/ipc_server.h"
#include "base/win/session_status.h"
#include "host/user_session.h"

namespace host {

class UserSession;

class UserSessionManager
    : public base::IpcServer::Delegate,
      public UserSession::Delegate
{
public:
    explicit UserSessionManager(std::shared_ptr<base::TaskRunner> task_runner);
    ~UserSessionManager();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onHostIdRequest(const std::string& session_name) = 0;
        virtual void onResetHostId(base::HostId host_id) = 0;
        virtual void onUserListChanged() = 0;
    };

    bool start(Delegate* delegate);
    void setSessionEvent(base::win::SessionStatus status, base::SessionId session_id);
    void setRouterState(const proto::internal::RouterState& router_state);
    void setHostId(const std::string& session_name, base::HostId host_id);
    void addNewSession(std::unique_ptr<ClientSession> client_session);
    std::unique_ptr<base::UserList> userList() const;

protected:
    // base::IpcServer::Delegate implementation.
    void onNewConnection(std::unique_ptr<base::IpcChannel> channel) override;
    void onErrorOccurred() override;

    // UserSession::Delegate implementation.
    void onUserSessionHostIdRequest(const std::string& session_name) override;
    void onUserSessionCredentialsChanged() override;
    void onUserSessionDettached() override;
    void onUserSessionFinished() override;

private:
    void startSessionProcess(base::SessionId session_id);
    void addUserSession(base::SessionId session_id, std::unique_ptr<base::IpcChannel> channel);

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::IpcServer> ipc_server_;
    std::vector<std::unique_ptr<UserSession>> sessions_;
    Delegate* delegate_ = nullptr;

    proto::internal::RouterState router_state_;

    DISALLOW_COPY_AND_ASSIGN(UserSessionManager);
};

} // namespace host

#endif // HOST__USER_SESSION_MANAGER_H
