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

#include "base/command_line.h"
#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/peer/user.h"
#include "build/version.h"
#include "router/database_factory_sqlite.h"
#include "router/database.h"
#include "router/settings.h"

#if defined(OS_WIN)
#include "router/win/service.h"
#include "router/win/service_util.h"
#else
#include "base/crypto/scoped_crypto_initializer.h"
#include "base/message_loop/message_loop.h"
#include "router/server.h"
#endif

#include <iostream>

namespace {

void initLogging()
{
    router::Settings settings;

    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = settings.minLogLevel();

    base::initLogging(logging_settings);
}

void shutdownLogging()
{
    base::shutdownLogging();
}

bool generateKeys(base::ByteArray* private_key, base::ByteArray* public_key)
{
    base::KeyPair key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        std::cout << "Failed to generate keys" << std::endl;
        return false;
    }

    *private_key = key_pair.privateKey();
    *public_key = key_pair.publicKey();

    if (private_key->empty() || public_key->empty())
    {
        std::cout << "Empty keys generated";
        return false;
    }

    return true;
}

void generateAndPrintKeys()
{
    base::ByteArray private_key;
    base::ByteArray public_key;

    if (!generateKeys(&private_key, &public_key))
        return;

    std::cout << "Private key: " << base::toHex(private_key) << std::endl;
    std::cout << "Public key: " << base::toHex(public_key) << std::endl;
}

void createConfig()
{
    std::cout << "Creation of initial configuration started." << std::endl;

    std::filesystem::path settings_file_path = router::Settings::filePath();

    std::cout << "Settings file path: " << settings_file_path << std::endl;

    std::error_code error_code;
    if (std::filesystem::exists(settings_file_path, error_code))
    {
        std::cout << "Settings file already exists. Continuation is impossible." << std::endl;
        return;
    }

    std::cout << "Settings file does not exist yet." << std::endl;

    std::filesystem::path public_key_dir;
    if (!base::BasePaths::commonAppData(&public_key_dir))
    {
        std::cout << "Failed to get the path to the config directory." << std::endl;
        return;
    }

    public_key_dir.append("aspia");

    std::cout << "Public key directory path: " << public_key_dir << std::endl;

    if (!std::filesystem::exists(public_key_dir, error_code))
    {
        std::cout << "Public key directory does not exist (" << error_code.message()
                  << "). Attempt to create..." << std::endl;

        if (!std::filesystem::create_directories(public_key_dir, error_code))
        {
            std::cout << "Failed to create directory for public key: "
                      << error_code.message() << std::endl;
            return;
        }
        else
        {
            std::cout << "Public key directory created successfully." << std::endl;
        }
    }
    else
    {
        std::cout << "Public key directory already exists." << std::endl;
    }

    std::filesystem::path public_key_file = public_key_dir;
    public_key_file.append("router.pub");

    std::cout << "Public key file: " << public_key_file << std::endl;

    if (std::filesystem::exists(public_key_file, error_code))
    {
        std::cout << "Public key file already exists. Continuation is impossible." << std::endl;
        return;
    }
    else
    {
        std::cout << "Public key does not exist yet." << std::endl;
    }

    std::unique_ptr<router::Database> db = router::DatabaseFactorySqlite().createDatabase();
    if (!db)
    {
        db = router::DatabaseFactorySqlite().openDatabase();
        if (db)
        {
            std::cout << "Database already exists. Continuation is impossible." << std::endl;
        }
        else
        {
            std::cout << "Failed to create new database." << std::endl;
        }
        return;
    }

    std::cout << "Creating a user..." << std::endl;

    const char16_t kUserName[] = u"admin";
    const char16_t kPassword[] = u"admin";

    base::User user = base::User::create(kUserName, kPassword);
    if (!user.isValid())
    {
        std::cout << "Failed to create user." << std::endl;
        return;
    }

    std::cout << "User has been created. Adding a user to the database..." << std::endl;

    user.sessions = proto::ROUTER_SESSION_ADMIN | proto::ROUTER_SESSION_CLIENT;
    user.flags = base::User::ENABLED;

    if (!db->addUser(user))
    {
        std::cout << "Failed to add user to database." << std::endl;
        return;
    }

    std::cout << "User was successfully added to the database." << std::endl;
    std::cout << "Generating encryption keys..." << std::endl;

    base::ByteArray private_key;
    base::ByteArray public_key;
    if (!generateKeys(&private_key, &public_key))
        return;

    std::cout << "Private and public keys have been successfully generated." << std::endl;
    std::cout << "Writing a public key to a file..." << std::endl;

    if (!base::writeFile(public_key_file, base::toHex(public_key)))
    {
        std::cout << "Failed to write public key to file: " << public_key_file << std::endl;
        return;
    }

    // Save the configuration file.
    router::Settings settings;
    settings.reset();
    settings.setPrivateKey(private_key);
    settings.flush();

    std::cout << "Configuration successfully created. Don't forget to change your password!"
              << std::endl;
    std::cout << "User name: " << base::local8BitFromUtf16(kUserName) << std::endl;
    std::cout << "Password: " << base::local8BitFromUtf16(kPassword) << std::endl;
    std::cout << "Public key file: " << public_key_file << std::endl;
}

void showHelp()
{
    std::cout << "aspia_router [switch]" << std::endl
        << "Available switches:" << std::endl
#if defined(OS_WIN)
        << '\t' << "--install" << '\t' << "Install service" << std::endl
        << '\t' << "--remove" << '\t' << "Remove service" << std::endl
        << '\t' << "--start" << '\t' << "Start service" << std::endl
        << '\t' << "--stop" << '\t' << "Stop service" << std::endl
#endif // defined(OS_WIN)
        << '\t' << "--create-config" << '\t' << "Creates a configuration" << std::endl
        << '\t' << "--keygen" << '\t' << "Generating public and private keys" << std::endl
        << '\t' << "--help" << '\t' << "Show help" << std::endl;
}

} // namespace

#if defined(OS_WIN)
int wmain()
{
    initLogging();

    base::CommandLine::init(0, nullptr); // On Windows ignores arguments.
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();

    if (command_line->hasSwitch(u"install"))
    {
        router::installService();
    }
    else if (command_line->hasSwitch(u"remove"))
    {
        router::removeService();
    }
    else if (command_line->hasSwitch(u"start"))
    {
        router::startService();
    }
    else if (command_line->hasSwitch(u"stop"))
    {
        router::stopService();
    }
    else if (command_line->hasSwitch(u"keygen"))
    {
        generateAndPrintKeys();
    }
    else if (command_line->hasSwitch(u"create-config"))
    {
        createConfig();
    }
    else if (command_line->hasSwitch(u"help"))
    {
        showHelp();
    }
    else
    {
        router::Service().exec();
    }

    shutdownLogging();
    return 0;
}
#else
int main(int argc, const char* const* argv)
{
    initLogging();

    base::CommandLine::init(argc, argv);
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();

    std::unique_ptr<base::ScopedCryptoInitializer> crypto_initializer =
        std::make_unique<base::ScopedCryptoInitializer>();

    if (command_line->hasSwitch(u"keygen"))
    {
        generateAndPrintKeys();
    }
    else if (command_line->hasSwitch(u"create-config"))
    {
        createConfig();
    }
    else if (command_line->hasSwitch(u"help"))
    {
        showHelp();
    }
    else
    {
        std::unique_ptr<base::MessageLoop> message_loop =
            std::make_unique<base::MessageLoop>(base::MessageLoop::Type::ASIO);

        std::unique_ptr<router::Server> server =
            std::make_unique<router::Server>(message_loop->taskRunner());

        server->start();
        message_loop->run();

        server.reset();
        message_loop.reset();
    }

    crypto_initializer.reset();
    shutdownLogging();
}
#endif
