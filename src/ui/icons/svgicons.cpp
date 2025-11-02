#include <QFile>
#include <QIconEngine>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmapCache>
#include <QRect>
#include <QApplication>
#include <QPalette>
#include <QImage>

#include "svgicons.h"

namespace Texxy {

class symbolicIconEngine : public QIconEngine
{
public:
    explicit symbolicIconEngine(QString file)
        : m_fileName(std::move(file))
    {
    }

    ~symbolicIconEngine() override = default;

    symbolicIconEngine* clone() const override
    {
        return new symbolicIconEngine(m_fileName);
    }

    void paint(QPainter* painter,
               const QRect& rect,
               QIcon::Mode mode,
               QIcon::State state) override
    {
        Q_UNUSED(state);

        QColor col;
        if (mode == QIcon::Disabled) {
            col = QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);
        } else if (mode == QIcon::Selected) {
            col = QApplication::palette().highlightedText().color();
        } else {
            col = QApplication::palette().windowText().color();
        }

        // Construct cache key
        const QString key = QStringLiteral("%1-%2x%3-%4")
            .arg(m_fileName,
                 QString::number(rect.width()),
                 QString::number(rect.height()),
                 col.name());

        QPixmap pix;
        if (!QPixmapCache::find(key, &pix)) {
            // Create a transparent pixmap with alpha channel
            QImage img(rect.size(), QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QSvgRenderer renderer;
            if (!m_fileName.isEmpty()) {
                QFile f(m_fileName);
                if (f.open(QIODevice::ReadOnly)) {
                    QByteArray bytes = f.readAll();
                    if (!bytes.isEmpty()) {
                        // Replace base color in SVG with desired color
                        bytes.replace("#000", col.name().toLatin1());
                        renderer.load(bytes);
                    }
                }
            }
            QPainter p(&img);
            renderer.render(&p, QRect(QPoint(0,0), rect.size()));
            p.end();
            pix = QPixmap::fromImage(img);
            QPixmapCache::insert(key, pix);
        }
        painter->drawPixmap(rect.topLeft(), pix);
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap pix(size);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        paint(&painter, QRect(QPoint(0,0), size), mode, state);
        painter.end();
        return pix;
    }

private:
    const QString m_fileName;
};

QIcon symbolicIcon::icon(const QString& fileName)
{
    return QIcon(new symbolicIconEngine(fileName));
}

} // namespace Texxy
