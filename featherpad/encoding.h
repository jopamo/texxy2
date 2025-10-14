/*
  texxy/encoding.h
*/

#ifndef ENCODING_H
#define ENCODING_H

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace FeatherPad {

QString detectCharset(const QByteArray& byteArray);

}  // namespace FeatherPad

#endif  // ENCODING_H
