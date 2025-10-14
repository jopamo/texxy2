/*
  texxy/encoding.h
*/

#ifndef ENCODING_H
#define ENCODING_H

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace Texxy {

QString detectCharset(const QByteArray& byteArray);

}  // namespace Texxy

#endif  // ENCODING_H
