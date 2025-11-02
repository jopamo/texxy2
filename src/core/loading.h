// src/core/loading.h

#ifndef LOADING_H
#define LOADING_H

#include <QThread>

namespace Texxy {

class Loading : public QThread {
    Q_OBJECT

   public:
    Loading(const QString& fname,
            const QString& charset,
            bool reload,
            int restoreCursor,
            int posInLine,
            bool forceUneditable,
            bool multiple);
    ~Loading();

    void setSkipNonText(bool skip) { skipNonText_ = skip; }

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

   private:
    void run();

    QString fname_;
    QString charset_;
    bool reload_;           // Is this a reloading? (Only passed.)
    int restoreCursor_;     // (How) should the cursor position be restored? (Only passed.)
    int posInLine_;         // The cursor position in line (if relevant).
    bool forceUneditable_;  // Should the doc be always uneditable? (Only passed.)
    bool multiple_;         // Are there multiple files to load? (Only passed.)
    bool skipNonText_;      // Should non-text files be skipped?
};

}  // namespace Texxy

#endif  // LOADING_H
