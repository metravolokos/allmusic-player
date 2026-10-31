#pragma once

#include <QtGlobal>

class MailWebDavRangeDevice;

namespace AndroidMailRangeRegistry {
qint64 add(MailWebDavRangeDevice *device);
void interrupt(qint64 handle);
void release(qint64 handle);
void releaseAll();
bool registerJniMethods();
}
