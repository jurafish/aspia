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

#include "host/ui/host_main.h"

#include "base/command_line.h"
#include "build/version.h"
#include "host/integrity_check.h"
#include "host/system_settings.h"
#include "host/ui/application.h"
#include "host/ui/main_window.h"
#include "host/ui/settings_util.h"
#include "common/ui/update_dialog.h"
#include "qt_base/scoped_qt_logging.h"

#if defined(OS_WIN)
#include "base/win/process_util.h"
#include "base/win/scoped_thread_desktop.h"
#endif // defined(OS_WIN)

#include <QMessageBox>

namespace {

bool waitForValidInputDesktop()
{
    int max_attempt_count = 600;

    do
    {
        base::Desktop input_desktop(base::Desktop::inputDesktop());
        if (input_desktop.isValid())
        {
            if (input_desktop.setThreadDesktop())
                break;
        }

        Sleep(100);
    }
    while (--max_attempt_count > 0);

    if (max_attempt_count == 0)
    {
        LOG(LS_WARNING) << "Exceeded the number of attempts";
        return false;
    }

    return true;
}

} // namespace

int hostMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(qt_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    qt_base::ScopedQtLogging scoped_logging;
    base::CommandLine command_line(argc, argv);

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
    LOG(LS_INFO) << "Command line: " << command_line.commandLineString();

    bool is_hidden = command_line.hasSwitch(u"hidden");
    if (!is_hidden)
    {
        if (!base::win::isProcessElevated())
        {
            if (base::win::createProcess(command_line, base::win::ProcessExecuteMode::ELEVATE))
                return 0;
        }
    }
    else
    {
        if (!waitForValidInputDesktop())
            return 1;
    }

    host::Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    host::Application::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    host::Application application(argc, argv);

    if (!host::integrityCheck())
    {
        LOG(LS_WARNING) << "Integrity check failed";

        QMessageBox::warning(
            nullptr,
            QApplication::translate("Host", "Warning"),
            QApplication::translate("Host", "Application integrity check failed. Components are "
                                            "missing or damaged."),
            QMessageBox::Ok);
        return 1;
    }
    else
    {
        LOG(LS_INFO) << "Integrity check passed successfully";
    }

    if (command_line.hasSwitch(u"import") && command_line.hasSwitch(u"export"))
    {
        LOG(LS_WARNING) << "Import and export are specified at the same time";

        if (!command_line.hasSwitch(u"silent"))
        {
            QMessageBox::warning(
                nullptr,
                QApplication::translate("Host", "Warning"),
                QApplication::translate("Host", "Export and import parameters can not be specified together."),
                QMessageBox::Ok);
        }

        return 1;
    }
    else if (command_line.hasSwitch(u"import"))
    {
        if (!host::SettingsUtil::importFromFile(command_line.switchValuePath(u"import"),
                                                command_line.hasSwitch(u"silent")))
        {
            return 1;
        }
    }
    else if (command_line.hasSwitch(u"export"))
    {
        if (!host::SettingsUtil::exportToFile(command_line.switchValuePath(u"export"),
                                              command_line.hasSwitch(u"silent")))
        {
            return 1;
        }
    }
    else if (command_line.hasSwitch(u"update"))
    {
        common::UpdateDialog dialog(
            QString::fromStdU16String(host::SystemSettings().updateServer()), "host");
        dialog.show();
        dialog.activateWindow();

        return application.exec();
    }
    else if (command_line.hasSwitch(u"help"))
    {
        // TODO
    }
    else
    {
        if (application.isRunning())
        {
            application.activate();
        }
        else
        {
            host::MainWindow window;

            QObject::connect(&application, &host::Application::activated,
                             &window, &host::MainWindow::activateHost);

            if (is_hidden)
            {
                window.hideToTray();
            }
            else
            {
                window.show();
                window.activateWindow();
            }

            window.connectToService();

            return application.exec();
        }
    }

    return 0;
}
