#include "AndroidMailRangeRegistry.h"

#include "../playback/MailWebDavRangeDevice.h"

#include <QJniEnvironment>
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QHash>
#include <memory>
#include <atomic>
#include <iterator>
#include <limits>

namespace {
struct Source final {
    explicit Source(MailWebDavRangeDevice *value) : device(value) {}
    ~Source()
    {
        if (device)
            device->retire();
    }

    MailWebDavRangeDevice *device = nullptr;
    QMutex ioMutex;
    std::atomic_bool active{true};
};

QMutex registryMutex;
QHash<jlong, std::shared_ptr<Source>> sources;
jlong nextHandle = 1;

std::shared_ptr<Source> lookup(jlong handle)
{
    QMutexLocker lock(&registryMutex);
    return sources.value(handle);
}

jlong nativeSize(JNIEnv *, jclass, jlong handle)
{
    const auto source = lookup(handle);
    if (!source || !source->active.load())
        return -1;
    return source->device->size();
}

jboolean nativeSeek(JNIEnv *, jclass, jlong handle, jlong position)
{
    const auto source = lookup(handle);
    if (!source || !source->active.load())
        return JNI_FALSE;
    QMutexLocker lock(&source->ioMutex);
    if (!source->active.load())
        return JNI_FALSE;
    return source->device->seek(position) ? JNI_TRUE : JNI_FALSE;
}

jint nativeRead(JNIEnv *environment, jclass, jlong handle, jlong position,
                jbyteArray output, jint outputOffset, jint requestedLength)
{
    constexpr jint BridgeReadLimit = 64 * 1024;
    if (!output || outputOffset < 0 || requestedLength <= 0
        || requestedLength > BridgeReadLimit
        || outputOffset > environment->GetArrayLength(output) - requestedLength)
        return -1;

    const auto source = lookup(handle);
    if (!source || !source->active.load())
        return -1;

    QMutexLocker lock(&source->ioMutex);
    if (!source->active.load())
        return -1;
    if (position < 0 || position > source->device->size())
        return -1;
    if (source->device->pos() != position && !source->device->seek(position))
        return -1;

    QByteArray scratch(requestedLength, Qt::Uninitialized);
    const qint64 bytesRead = source->device->read(scratch.data(), requestedLength);
    if (bytesRead > 0 && source->active.load()) {
        environment->SetByteArrayRegion(output, outputOffset, static_cast<jsize>(bytesRead),
                                         reinterpret_cast<const jbyte *>(scratch.constData()));
        if (environment->ExceptionCheck()) {
            environment->ExceptionClear();
            scratch.fill('\0');
            return -1;
        }
    }
    scratch.fill('\0');
    if (!source->active.load())
        return -1;
    if (bytesRead == 0 && position >= source->device->size())
        return -1;
    return static_cast<jint>(bytesRead);
}

void nativeInterrupt(JNIEnv *, jclass, jlong handle)
{
    const auto source = lookup(handle);
    if (source && source->active.load())
        source->device->interruptPendingRead();
}

void nativeRelease(JNIEnv *, jclass, jlong handle)
{
    AndroidMailRangeRegistry::release(handle);
}
}

namespace AndroidMailRangeRegistry {
qint64 add(MailWebDavRangeDevice *device)
{
    if (!device)
        return 0;
    QMutexLocker lock(&registryMutex);
    if (nextHandle <= 0)
        return 0;
    const jlong handle = nextHandle;
    nextHandle = nextHandle == std::numeric_limits<jlong>::max() ? 0 : nextHandle + 1;
    sources.insert(handle, std::make_shared<Source>(device));
    return handle;
}

void interrupt(qint64 handle)
{
    const auto source = lookup(handle);
    if (source && source->active.load())
        source->device->interruptPendingRead();
}

void release(qint64 handle)
{
    std::shared_ptr<Source> source;
    {
        QMutexLocker lock(&registryMutex);
        source = sources.take(handle);
    }
    if (!source)
        return;
    source->active.store(false);
    source->device->cancelAsync();
}

void releaseAll()
{
    QHash<jlong, std::shared_ptr<Source>> pending;
    {
        QMutexLocker lock(&registryMutex);
        pending.swap(sources);
    }
    for (const auto &source : pending) {
        source->active.store(false);
        source->device->cancelAsync();
    }
}

bool registerJniMethods()
{
    static const JNINativeMethod methods[] = {
        {const_cast<char *>("nativeSize"), const_cast<char *>("(J)J"),
         reinterpret_cast<void *>(nativeSize)},
        {const_cast<char *>("nativeSeek"), const_cast<char *>("(JJ)Z"),
         reinterpret_cast<void *>(nativeSeek)},
        {const_cast<char *>("nativeRead"), const_cast<char *>("(JJ[BII)I"),
         reinterpret_cast<void *>(nativeRead)},
        {const_cast<char *>("nativeInterrupt"), const_cast<char *>("(J)V"),
         reinterpret_cast<void *>(nativeInterrupt)},
        {const_cast<char *>("nativeRelease"), const_cast<char *>("(J)V"),
         reinterpret_cast<void *>(nativeRelease)},
    };
    QJniEnvironment environment;
    return environment.registerNativeMethods(
        "org/metravolokos/allmusicplayer/Media3PlaybackAdapter", methods,
        static_cast<jint>(std::size(methods)));
}
}
