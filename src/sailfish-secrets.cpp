#include "sailfish-secrets.h"

#ifdef SAILFISH

#include <QCoreApplication>
#include <QByteArray>
#include <QEventLoop>
#include <QString>
#include <QTimer>
#include <cstring>

#include <Secrets/createcollectionrequest.h>
#include <Secrets/deletesecretrequest.h>
#include <Secrets/secret.h>
#include <Secrets/secretmanager.h>
#include <Secrets/collectionnamesrequest.h>
#include <Secrets/storesecretrequest.h>
#include <Secrets/storedsecretrequest.h>

using namespace Sailfish::Secrets;

namespace {

const QString kCollectionName = QStringLiteral("rsc");
const QString kSecretUsername = QStringLiteral("username");
const QString kSecretPassword = QStringLiteral("password");

QCoreApplication *ensure_app() {
    QCoreApplication *app = QCoreApplication::instance();
    if (app != nullptr) {
        return app;
    }

    static int argc = 1;
    static char app_name[] = "rsc-c";
    static char *argv[] = {app_name, nullptr};
    static QCoreApplication core_app(argc, argv);
    return &core_app;
}

bool wait_for_manager(SecretManager *manager) {
    if (manager->isInitialized()) {
        return true;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(manager, &SecretManager::isInitializedChanged,
                     &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,
                     &loop, &QEventLoop::quit);
    timer.start(2000);
    loop.exec();
    return manager->isInitialized();
}

bool collection_exists(SecretManager *manager) {
    CollectionNamesRequest request;
    request.setManager(manager);
    request.setStoragePluginName(SecretManager::DefaultEncryptedStoragePluginName);
    request.startRequest();
    request.waitForFinished();

    if (request.result().code() != Result::Succeeded) {
        fprintf(stderr, "SAILFISH SECRETS: list collections failed: %s\n",
                request.result().errorMessage().toUtf8().constData());
        return false;
    }

    const QStringList names = request.collectionNames();
    return names.contains(kCollectionName);
}

bool create_collection(SecretManager *manager,
                       CreateCollectionRequest::CollectionLockType lock_type,
                       const QString &storage_plugin,
                       const QString &encryption_plugin) {
    CreateCollectionRequest request;
    request.setManager(manager);
    request.setCollectionName(kCollectionName);
    request.setAccessControlMode(SecretManager::OwnerOnlyMode);
    request.setCollectionLockType(lock_type);
    if (lock_type == CreateCollectionRequest::DeviceLock) {
        request.setDeviceLockUnlockSemantic(SecretManager::DeviceLockKeepUnlocked);
    }
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.setStoragePluginName(storage_plugin);
    request.setEncryptionPluginName(encryption_plugin);
    request.startRequest();
    request.waitForFinished();

    if (request.result().code() != Result::Succeeded) {
        fprintf(stderr, "SAILFISH SECRETS: create collection failed: %s\n",
                request.result().errorMessage().toUtf8().constData());
        return false;
    }

    return true;
}

bool ensure_collection(SecretManager *manager) {
    if (collection_exists(manager)) {
        return true;
    }

    return create_collection(manager,
                             CreateCollectionRequest::DeviceLock,
                             SecretManager::DefaultEncryptedStoragePluginName,
                             SecretManager::DefaultEncryptedStoragePluginName);
}

Secret::Identifier make_identifier(const QString &name) {
    return Secret::Identifier(name, kCollectionName,
                              SecretManager::DefaultEncryptedStoragePluginName);
}

void delete_secret_if_present(SecretManager *manager, const QString &name) {
    DeleteSecretRequest request;
    request.setManager(manager);
    request.setIdentifier(make_identifier(name));
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished();
}

int store_secret(const QString &name, const char *value) {
    if (value == nullptr) {
        value = "";
    }

    ensure_app();

    SecretManager manager;
    if (!wait_for_manager(&manager)) {
        fprintf(stderr,
                "SAILFISH SECRETS: daemon not available (store %s)\n",
                name.toUtf8().constData());
        return 0;
    }
    if (!ensure_collection(&manager)) {
        fprintf(stderr,
                "SAILFISH SECRETS: collection unavailable (store %s)\n",
                name.toUtf8().constData());
        return 0;
    }

    Secret secret(make_identifier(name));
    secret.setType(Secret::TypeBlob);
    secret.setData(QByteArray(value));

    /* Replace existing value when saving updated credentials. */
    delete_secret_if_present(&manager, name);

    StoreSecretRequest request;
    request.setManager(&manager);
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.setSecretStorageType(StoreSecretRequest::CollectionSecret);
    request.setSecret(secret);
    request.startRequest();
    request.waitForFinished();

    if (request.result().code() != Result::Succeeded) {
        fprintf(stderr, "SAILFISH SECRETS: store %s failed: %s\n",
                name.toUtf8().constData(),
                request.result().errorMessage().toUtf8().constData());
    }

    return request.result().code() == Result::Succeeded;
}

int load_secret(const QString &name, char *buffer, size_t buffer_len) {
    if (buffer == nullptr || buffer_len == 0) {
        return 0;
    }

    ensure_app();

    SecretManager manager;
    if (!wait_for_manager(&manager)) {
        fprintf(stderr,
                "SAILFISH SECRETS: daemon not available (load %s)\n",
                name.toUtf8().constData());
        return 0;
    }

    if (!collection_exists(&manager)) {
        return 0;
    }

    StoredSecretRequest request;
    request.setManager(&manager);
    request.setIdentifier(make_identifier(name));
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished();

    if (request.result().code() != Result::Succeeded) {
        fprintf(stderr, "SAILFISH SECRETS: load %s failed: %s\n",
                name.toUtf8().constData(),
                request.result().errorMessage().toUtf8().constData());
        return 0;
    }

    QByteArray data = request.secret().data();
    size_t copy_len = static_cast<size_t>(data.size());
    if (copy_len >= buffer_len) {
        copy_len = buffer_len - 1;
    }

    memcpy(buffer, data.constData(), copy_len);
    buffer[copy_len] = '\0';
    return 1;
}

int clear_secret(const QString &name) {
    ensure_app();

    SecretManager manager;
    if (!wait_for_manager(&manager)) {
        fprintf(stderr,
                "SAILFISH SECRETS: daemon not available (clear %s)\n",
                name.toUtf8().constData());
        return 0;
    }

    DeleteSecretRequest request;
    request.setManager(&manager);
    request.setIdentifier(make_identifier(name));
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished();

    if (request.result().code() != Result::Succeeded) {
        fprintf(stderr, "SAILFISH SECRETS: clear %s failed: %s\n",
                name.toUtf8().constData(),
                request.result().errorMessage().toUtf8().constData());
    }

    return request.result().code() == Result::Succeeded;
}

} // namespace

int sailfish_secrets_store_username(const char *username) {
    return store_secret(kSecretUsername, username);
}

int sailfish_secrets_store_password(const char *password) {
    return store_secret(kSecretPassword, password);
}

int sailfish_secrets_load_username(char *username, size_t username_len) {
    return load_secret(kSecretUsername, username, username_len);
}

int sailfish_secrets_load_password(char *password, size_t password_len) {
    return load_secret(kSecretPassword, password, password_len);
}

int sailfish_secrets_clear_username(void) {
    return clear_secret(kSecretUsername);
}

int sailfish_secrets_clear_password(void) {
    return clear_secret(kSecretPassword);
}

#else

int sailfish_secrets_store_username(const char *username) {
    (void)username;
    return 0;
}

int sailfish_secrets_store_password(const char *password) {
    (void)password;
    return 0;
}

int sailfish_secrets_load_username(char *username, size_t username_len) {
    if (username != nullptr && username_len > 0) {
        username[0] = '\0';
    }
    return 0;
}

int sailfish_secrets_load_password(char *password, size_t password_len) {
    if (password != nullptr && password_len > 0) {
        password[0] = '\0';
    }
    return 0;
}

int sailfish_secrets_clear_username(void) {
    return 0;
}

int sailfish_secrets_clear_password(void) {
    return 0;
}

#endif
