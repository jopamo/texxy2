// src/core/loading.h

#ifndef LOADING_H
#define LOADING_H

#include <QThread>
#include <QString>

namespace Texxy {

class Loading : public QThread {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Loading)

   public:
    explicit Loading(const QString& fname,
                     const QString& charset,
                     bool reload,
                     int restoreCursor,
                     int posInLine,
                     bool forceUneditable,
                     bool multiple);
    ~Loading() override = default;

    void setSkipNonText(bool skip) noexcept { skipNonText_ = skip; }

   signals:
    void completed(const QString& text = QString(),
                   const QString& fname = QString(),
                   const QString& charset = QString(),
                   bool enforceEncod = false,
                   bool reload = false,
                   int restoreCursor = 0,
                   int posInLine = 0,
                   bool uneditable = false,
                   bool multiple = false);

   protected:
    void run() final override;

   private:
    QString fname_;
    QString charset_;
    bool reload_;           // is this a reload
    int restoreCursor_;     // how the cursor position should be restored
    int posInLine_;         // cursor position in line if relevant
    bool forceUneditable_;  // force document uneditable
    bool multiple_;         // multiple files to load
    bool skipNonText_;      // skip non text files
};

}  // namespace Texxy

#endif  // LOADING_H
